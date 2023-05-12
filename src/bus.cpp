#include "sqlite3.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <mqueue.h>
#include <mutex>
#include <ncurses.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <sstream>

using std::cin;
using std::cout;
using std::string;
using std::thread;
using std::unordered_set;
using namespace std::this_thread;
using namespace std::chrono;
using namespace std::filesystem;

sqlite3 *db;

const path audio_dir("/home/corey/scannerbot/audio");
const path transcript_dir("/home/corey/scannerbot/transcripts/");

// Signals that the user has requested to quit the program
std::atomic<bool> do_shutdown = false;
std::atomic<bool> do_watch = false;

// A map relating function names to the threads running them
std::unordered_map<string, thread *> threadMap;
// Use this mutex to lock the threadMap
std::mutex threadMapMutex;

// The process ID of the running recorder program, if any. Set to -1 if not
pid_t recorder_pid = -1;
char *currentfreq = "160.71M";

// The name of the recorder posix message queue
const char *REC_MQ_NAME = "/sb_rec_inbox";
const char *BUS_MQ_NAME = "/sb_bus_inbox";
// Map of posix message queue names to their file descriptors
std::unordered_map<const char *, mqd_t> mqdMap;

void cleanup();
void db_init();
void interruptHandler();
void kill_recorder();
void mq_init();
void run_cli();
void run_recorder(string);
void show_help();
void watch_directories();

static int callback(void *data, int argc, char **argv, char **azColName)
{
    for (int i = 0; i < argc; i++)
    {
        std::cout << azColName[i] << ": " << (argv[i] ? argv[i] : "NULL") << std::endl;
    }
    std::cout << std::endl;
    return 0;
}

void cleanup()
{
    sqlite3_close(db);

    kill_recorder();
    do_watch = false;

    // Clean up message queues
    for (auto &[queue_name, mqd] : mqdMap)
    {
        mq_close(mqd);
        mq_unlink(queue_name);
    }

    // Clean up threads
    for (auto &[function, thread] : threadMap)
        if (thread->joinable())
            thread->join();
}

void interruptHandler(int signum)
{
    cout << '\n'
         << getpid() << " received interrupt signal (" << signum << ").\n";
    do_shutdown = true;
    do_watch = false;

    cleanup();

    exit(signum);
}

void kill_recorder()
{
    if (recorder_pid == -1)
        return;

    // Message the recorder to kill its shell script, if any
    string sndmsg("quit");
    if (recorder_pid != -1)
        mq_send(mqdMap[REC_MQ_NAME], sndmsg.c_str(),
                sizeof(sndmsg), 0);
    char *retmsg = new char[64];
    mq_receive(mqdMap[BUS_MQ_NAME], retmsg, 64, 0);
    cout << "\n\nRecorder sez: " << retmsg;

    // Kill entire recorder process group
    if (kill(recorder_pid, SIGTERM) == -1)
    {
        perror("Unable to kill recorder");
    }

    cout << "\nRecorder [PID " << recorder_pid << "] killed.";

    recorder_pid = -1;
}

void run_recorder(string args)
{
    // Terminate any existing recorder process
    kill_recorder();
    char **recorder_args = new char *[20];
    // auto file_actions = posix_spawn_file_actions_init();
    int spawn_err = posix_spawn(&recorder_pid,
                                "/home/corey/scannerbot/bin/recorder",
                                nullptr, nullptr, recorder_args, nullptr);
    // Error
    if (spawn_err != 0)
    {
        perror("\nError starting recorder\n");
        exit(EXIT_FAILURE);
    }

    // Send command to recorder process
    mq_send(mqdMap[REC_MQ_NAME], args.c_str(), sizeof(args), 0);

    cout << "\nRecorder has PID " << recorder_pid;
    int status;
    waitpid(recorder_pid, &status, 0);
    if (WIFEXITED(status))
        cout << "\nRecorder exited with status " << WEXITSTATUS(status);
    else
        cout << "\nRecorder did not exit normally";
}

void watch_directories()
{
    unordered_set<string> seen_audio_files;
    unordered_set<string> seen_transcript_files;

    while (not do_shutdown and do_watch)
    {
        //  Handle new audio files.
        for (auto &file : directory_iterator(audio_dir))
        {
            //  The file is already in the set.
            if (seen_audio_files.count(file.path()) > 0)
                continue;

            // We're giving time for the recorder to finish working with this
            // audio file. Transmission may briefly cut in and out so we don't
            // want to send audio to the transcriber until everything has been
            // captured.
            int min_wait = 30;
            // Getting the time the audio file was last modified.
            char *path_buf = (char *)file.path().c_str();
            struct stat audio_file_stats;
            stat(path_buf, &audio_file_stats);
            auto file_time = audio_file_stats.st_mtim.tv_sec;
            // Getting the current time.
            struct timespec now_time_st;
            clock_gettime(CLOCK_REALTIME, &now_time_st);
            auto now_time = now_time_st.tv_sec;

            cout << "\nFile wirte time: " << file_time;
            cout << "\nNow: " << now_time;
            cout << "\nDifference: " << (now_time - file_time);

            if ((now_time - file_time) < min_wait)
                continue;

            // The file is too small to bother with.
            // if (file.file_size() < 4096)
            //     continue;

            seen_audio_files.insert(file.path());

            // The file isn't audio.
            if (not is_regular_file(file.status()))
                continue;

            // Prepare to push to database
            struct tm *raw = localtime(&audio_file_stats.st_ctim.tv_sec);
            char time_buf[32];
            memset(time_buf, '\0', 32);
            strftime(time_buf, 32, "%d-%m-%Y %H:%M:%S", raw);
            char date[16];
            memset(date, '\0', 16);
            char time[16];
            memset(time, '\0', 16);
            strncpy(date, time_buf, strcspn(time_buf, " "));
            date[11] = '\0';
            strcpy(time, strchr(time_buf, ' ')+1);
            
            char insert_command[1024];
            memset(insert_command, '\0', 1024);
            // Append command start
            string command_prefix = "INSERT INTO info (date, time, audioPath) VALUES (";
            strncat(insert_command, command_prefix.c_str(), command_prefix.length());
            insert_command[strlen(insert_command)] = ' ';
            // Append date
            insert_command[strlen(insert_command)] = '\'';
            strncat(insert_command, date, strlen(date));
            insert_command[strlen(insert_command)] = '\'';
            insert_command[strlen(insert_command)] = ',';
            // Append time
            insert_command[strlen(insert_command)] = '\'';
            strncat(insert_command, time, strlen(time));
            insert_command[strlen(insert_command)] = '\'';
            insert_command[strlen(insert_command)] = ',';
            // Append audio file path
            insert_command[strlen(insert_command)] = '\'';
            strncat(insert_command, path_buf, strlen(path_buf));
            // Append command end
            string command_postfix = "\');";
            strncat(insert_command, command_postfix.c_str(), command_postfix.length());

            cout << "\n" << insert_command;

            char * errmsg = 0;
            int resultcode = sqlite3_exec(db, insert_command, callback, 0, &errmsg);
            if (resultcode != SQLITE_OK)
            {
                std::cerr << "SQL error: " << errmsg << std::endl;
                sqlite3_free(errmsg);
            }

            //  Send the new audio file to the transcriber.
            string command = "python3 /home/corey/scannerbot/src/transcriber.py \"";
            command += file.path();
            command += "\"";
            command += " > /dev/null";
            [[maybe_unused]] int trans_ret_status = system(command.c_str());
        }

        // Handle new transcripts.
        for (auto &file : directory_iterator(transcript_dir))
        {
            if (seen_transcript_files.count(file.path()) > 0)
                continue;

            // The file is too small to bother with.
            if (file.file_size() < 16)
                continue;

            seen_transcript_files.insert(file.path());

            if (not is_regular_file(file.status()))
                continue;

            // Send the new transcript file to the publisher.
            string command = "python3 /home/corey/scannerbot/src/publisher.py \"";
            command += file.path();
            command += "\"";
            command += " > /dev/null";
            [[maybe_unused]] int pub_ret_status = system(command.c_str());
        }

        sleep_for(5s);
    }
}

void show_help()
{
    cout << '\n'
         << R"(Scannerbot commands:                           )" << '\n'
         << R"(    s   start     Begin recording transmissions)" << '\n'
         << R"(        stop      Stop recording               )" << '\n'
         << R"(    q   quit      Stop scannerbot and exit     )" << '\n'
         << R"(    h   help      Show this list of commands   )" << '\n'
         << R"(    f   freq      Set radio frequency          )" << '\n'
         << R"(    g   gain      Set radio gain               )" << '\n'
         << R"(    l   squelch   Set radio quelch             )"
         << std::endl;
}

void run_cli()
{
    string user_input;
    string command;
    string args;

    while (not do_shutdown)
    {
        // Get user input a parse it
        cout << "\nscannerbot> ";
        getline(cin, user_input);
        command = user_input.substr(0, user_input.find_first_of(' '));
        args = user_input.substr(user_input.find_first_of(' ') + 1);

        if (command == "start" or command == "s")
        {
            std::lock_guard<std::mutex> lock(threadMapMutex);

            // Launch recorder
            if (threadMap.count("run_recorder") == 0)
                threadMap["run_recorder"] = new thread(run_recorder, user_input);

            // Launch directory watchers
            do_watch = true;
            if (threadMap.count("watch_directories") == 0)
                threadMap["watch_directories"] = new thread(watch_directories);
        }

        else if (command == "stop")
        {
            std::lock_guard<std::mutex> lock(threadMapMutex);

            kill_recorder();
            if (threadMap["run_recorder"]->joinable())
                threadMap["run_recorder"]->join();
            threadMap.erase("run_recorder");

            do_watch = false;
            if (threadMap["watch_directories"]->joinable())
                threadMap["watch_directories"]->join();
            threadMap.erase("watch_directories");
        }

        else if (command == "gain" or command == "g")
        {
            string message("gain " + args);
            if (recorder_pid != -1)
                mq_send(mqdMap[REC_MQ_NAME], message.c_str(),
                        sizeof(message), 0);
            else
                cout << "\nThe recorder has not started yet.";
        }

        else if (command == "frequency" or command == "freq" or command == "f")
        {
            string message("freq " + args);
            if (recorder_pid != -1)
                mq_send(mqdMap[REC_MQ_NAME], message.c_str(),
                        sizeof(message), 0);
            else
                cout << "\nThe recorder has not started yet.";
        }

        else if (command == "quit" or command == "q")
        {
            cleanup();
            exit(EXIT_SUCCESS);
        }

        else if (command == "help" or command == "h")
        {
            show_help();
        }

        else
        {
            std::cout << "\nNo such option.\n";
        }

        sleep_for(50ms);
    }
}

void mq_init()
{
    mq_unlink(BUS_MQ_NAME);
    mq_unlink(REC_MQ_NAME);
    struct mq_attr attr;   // Message queue attributes
    attr.mq_maxmsg = 10;   // Maximum number of messages in the queue
    attr.mq_msgsize = 256; // Maximum message size (in bytes)

    // Connect to the bus inbox for reading messages from other modules
    mqdMap[BUS_MQ_NAME] = mq_open(BUS_MQ_NAME, O_CREAT | O_RDONLY, S_IRUSR | S_IWUSR, &attr);
    if (mqdMap[BUS_MQ_NAME] == -1)
    {
        perror("bus inbox mq_open");
        exit(EXIT_FAILURE);
    }

    // Connect to the recorder inbox for sending messages
    mqdMap[REC_MQ_NAME] = mq_open(REC_MQ_NAME, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR, &attr);
    if (mqdMap[REC_MQ_NAME] == -1)
    {
        perror("recorder inbox mq_open");
        exit(EXIT_FAILURE);
    }
}



void db_init()
{
    char *errmsg = 0;

    // Open database
    int resultcode = sqlite3_open("db/info.db", &db);
    if (resultcode != SQLITE_OK)
    {
        cout << "Can't open database: " << sqlite3_errmsg(db) << '\n';
        sqlite3_close(db);
    }

    // Create table
    const char *createTableSQL =
        "CREATE TABLE IF NOT EXISTS info(\
            id INTEGER PRIMARY KEY AUTOINCREMENT,\
            date TEXT, time TEXT, freq TEXT, agency TEXT,\
            transcript TEXT, audioPath TEXT,\
            postID TEXT, postURL TEXT);";

    resultcode = sqlite3_exec(db, createTableSQL, callback, 0, &errmsg);
    if (resultcode != SQLITE_OK)
    {
        std::cerr << "SQL error: " << errmsg << std::endl;
        sqlite3_free(errmsg);
    }
}

int main()
{
    signal(SIGINT, interruptHandler); // Handler for keyboard interrupt

    mq_init(); // Set up inter-process communication
    db_init();

    cout << '\n'
         << R"(   ____                           __        __ )" << '\n'
         << R"(  / __/______  ___  ___  ___ ____/ /  ___  / /_)" << '\n'
         << R"( _\ \/ __/ _ `/ _ \/ _ \/ -_) __/ _ \/ _ \/ __/)" << '\n'
         << R"(/___/\__/\_,_/_//_/_//_/\__/_/ /_.__/\___/\__/ )" << '\n'
         << std::endl;

    cout << R"(    An experiment in software engineering by   )" << '\n'
         << R"(         Citlally Gomez  Corey Talbert         )" << '\n'
         << R"(            Kailyn King  Andrew Le             )"
         << std::endl;

    show_help(); // List available commands

    try
    {
        run_cli(); // Start the command line interface
    }
    catch (std::exception e)
    {
        cout << '\n'
             << e.what();
        exit(1);
    }

    cleanup();

    return 0;
}
