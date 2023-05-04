#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <spawn.h>
#include <sstream>
#include <sys/wait.h>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using std::cin;
using std::cout;
using std::string;
using std::thread;
using std::unordered_set;
using std::vector;
using namespace std::this_thread;
using namespace std::chrono;
using namespace std::filesystem;

const path audio_dir("/home/corey/scannerbot/audio");
const path transcript_dir("/home/corey/scannerbot/transcripts/");

// Signals that the user has requested to quit the program
std::atomic<bool> shutdown = false;
// Controls the watch_directories loop
std::atomic<bool> do_watch;

// A map relating function names to the threads running them
std::unordered_map<string, thread *> threadMap;
// Use this mutex to lock the threadMap
std::mutex threadMapMutex;

// The process ID of the running recorder program, if any. Set to -1 if not
pid_t recorder_pid = -1;

std::stringstream input_line_stream;

void kill_recorder();
void interruptHandler();
void watch_directories();
void run_recorder();
void run_cli();

void interruptHandler(int signum)
{
    cout << '\n'
         << getpid() << " received interrupt signal (" << signum << ").\n";
    shutdown = true;
    do_watch = false;

    kill_recorder();

    for (auto &[function, thread] : threadMap)
        if (thread->joinable())
            thread->join();

    exit(signum);
}

void kill_recorder()
{
    if (recorder_pid == -1)
        return;

    if (kill(recorder_pid, SIGTERM) == -1)
    {
        perror("Unable to kill recorder.");
        exit(EXIT_FAILURE);
    }

    cout << "\nRecorder [PID " << recorder_pid << "] killed.";

    recorder_pid = -1;
}

void run_recorder()
{
    // Repack command line args for posix_spawn
    char **recorder_args = new char *[20];
    string arg;
    for (int i = 0; input_line_stream >> arg and i < 20; ++i)
    {
        recorder_args[i] = new char[arg.length() + 1];
        strcpy(recorder_args[i], arg.c_str());
        cout << recorder_args[i] << ' ';
    }
    cout << '\n';

    // Terminate any existing recorder process
    kill_recorder();

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
    // Parent
    else
    {
        cout << "\nRecorder has PID " << recorder_pid;
        int status;
        waitpid(recorder_pid, &status, 0);
        if (WIFEXITED(status))
        {
            cout << "\nRecorder exited with status " << WEXITSTATUS(status);
        }
        else
        {
            cout << "\nChild process did not exit normally";
        }
    }
    cout << "\nEnd of run_recorder";
}

void run_cli()
{
    string input_line_str;
    string command;
    cout << "scannerbot> ";

    while (not shutdown and getline(cin, input_line_str))
    {
        command.clear();

        input_line_stream.str(input_line_str);
        input_line_stream >> command;
        cout << "command: " << command << "\nargs: ";

        if (command == "start")
        {
            std::lock_guard<std::mutex> lock(threadMapMutex);

            if (threadMap.count("run_recorder") == 0)
                threadMap["run_recorder"] = new thread(run_recorder);

            do_watch = true; // Enable forever directory watcher loop
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

        else if (command == "gain")
        {
        }

        else if (command == "frequency")
        {
        }

        else if (command == "quiet")
        {
        }

        else if (command == "verbose")
        {
        }

        else if (command == "quit")
        {
            shutdown = true;
            do_watch = false;
        }

        else
        {
            std::cout << "\nNo such option.\n";
        }

        std::cout << "scannerbot> ";
    }
}

void watch_directories()
{
    // cout << "\nThread " << std::this_thread::get_id() << " entering watch_directories";

    unordered_set<string> seen_audio_files;
    unordered_set<string> seen_transcript_files;

    while (do_watch and not shutdown)
    {
        // cout << "\nEntering watch loop...";
        //  Handle new audio files.
        for (auto &file : directory_iterator(audio_dir))
        {
            // cout << "\nIterating audio directory...";
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
            // cout << "\nIterating transcript directory...";
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

int main()
{
    cout << '\n';
    cout << R"(   ____                           __        __ )" << '\n';
    cout << R"(  / __/______ ____  ___  ___ ____/ /  ___  / /_)" << '\n';
    cout << R"( _\ \/ __/ _ `/ _ \/ _ \/ -_) __/ _ \/ _ \/ __/)" << '\n';
    cout << R"(/___/\__/\_,_/_//_/_//_/\__/_/ /_.__/\___/\__/ )"
         << "\n\n\n";

    cout << R"(    An experiment in software engineering by   )" << '\n';
    cout << R"(                Citlally Gomez                 )" << '\n';
    cout << R"(                  Kailyn King                  )" << '\n';
    cout << R"(                   Andrew Le                   )" << '\n';
    cout << R"(                 Corey Talbert                 )"
         << "\n\n";

    try
    {
        signal(SIGINT, interruptHandler);

        // Start the scanner bot command line interface
        run_cli();

        while (not shutdown)
        {
        }

        for (auto &[function, thread] : threadMap)
            if (thread->joinable())
                thread->join();
    }
    catch (std::exception e)
    {
        cout << '\n'
             << e.what();
        exit(1);
    }
    return 0;
}
