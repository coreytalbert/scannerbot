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
#include <sstream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unordered_map>
#include <unordered_set>

using std::cin;
using std::cout;
using std::string;
using std::thread;
using std::unordered_set;
using namespace std::this_thread;
using namespace std::chrono;
using namespace std::filesystem;

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

std::stringstream input_line_stream;

// The name of the recorder posix message queue
const char *REC_MQ_NAME = "/sb_rec_inbox";
const char *BUS_MQ_NAME = "/sb_bus_inbox";
// Map of posix message queue names to their file descriptors
std::unordered_map<const char *, mqd_t> mqdMap;

void cleanup();
void interruptHandler();
void kill_recorder();
void mq_init();
void run_cli();
void run_recorder(string);
void show_help();
void watch_directories();

void cleanup()
{
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

    cleanup();

    exit(signum);
}

void kill_recorder()
{
    if (recorder_pid == -1)
        return;

    // Kill entire recorder process group
    if (kill(recorder_pid, SIGTERM) == -1)
    {
        perror("Unable to kill recorder.");
        exit(EXIT_FAILURE);
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

    cout << "\nEnd of run_recorder";
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

            // Wait 10 seconds since last write to transcribe file.
            auto min_wait_s = 30s;
            // std::this_thread::sleep_for(min_wait_s);
            auto time_now =
                duration_cast<seconds>(system_clock::now().time_since_epoch());
            auto last_file_write_time =
                duration_cast<seconds>(last_write_time(file).time_since_epoch());

            if ((time_now - last_file_write_time) < min_wait_s)
                continue;

            // The file is too small to bother with.
            if (file.file_size() < 4096)
                continue;

            seen_audio_files.insert(file.path());

            // The file isn't audio.
            if (not is_regular_file(file.status()))
                continue;

            // cout << "\nNew audio detected: " << file.path();
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

            cout << "\nNew transcript detected: " << file.path();
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
    cout << R"(Scannerbot commands:                           )" << '\n'
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
	initscr(); // initialize ncurses
	keypad(stdscr, true); // special keys
	cbreak(); // hide line buffering
	curs_set(0); // hide cursor
	
	// define terminal appearance
	int width, height;
	getmaxyx(stdscr, width, height);
	WINDOW* window = newwin(width-1, height, 0, 0);
	
    string input_line_str;
    string command;
    //cout << "\nscannerbot> ";

    while (not do_shutdown and getline(cin, input_line_str))
    {
		command.clear();
		
		// prompt
		mvwprintw(window, width02, 0, "scannerbot> ");
		wrefresh(window);
	
		// wgetstr(window, input)
		// if (strcmp(input, "quit") == 0) {}
		
        input_line_stream.str(input_line_str);
        input_line_stream >> command;
        cout << "command: " << command << "\nargs: ";

        if (command == "start" or command == "s")
        {
            std::lock_guard<std::mutex> lock(threadMapMutex);

            if (threadMap.count("run_recorder") == 0)
                threadMap["run_recorder"] = new thread(run_recorder, input_line_str);
            
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
        }

        else if (command == "frequency" or command == "freq" or command == "f")
        {
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

        sleep_for(250ms);
        //cout << "\nscannerbot> ";
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

int main()
{
    signal(SIGINT, interruptHandler); // Handler for keyboard interrupt

    mq_init(); // Set up inter-process communication

    cout << '\n'
         << R"(   ____                           __        __ )" << '\n'
         << R"(  / __/______  ___  ___  ___ ____/ /  ___  / /_)" << '\n'
         << R"( _\ \/ __/ _ `/ _ \/ _ \/ -_) __/ _ \/ _ \/ __/)" << '\n'
         << R"(/___/\__/\_,_/_//_/_//_/\__/_/ /_.__/\___/\__/ )" << '\n'
         << std::endl;

    cout << R"(    An experiment in software engineering by   )" << '\n'
         << R"(         Citlally Gomez  Corey Talbert         )" << '\n'
         << R"(            Kailyn King  Andrew Le             )" << '\n'
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
