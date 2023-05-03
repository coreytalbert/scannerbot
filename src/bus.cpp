#include <atomic>
#include <cstring>
#include <chrono>
#include <exception>
#include <iostream>
#include <filesystem>
#include <csignal>
#include <thread>
#include <mutex>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <queue>
#include <functional>
#include <spawn.h>
#include <sys/wait.h>
#include <sstream>
using std::cin;
using std::cout;
using std::function;
using std::string;
using std::thread;
using std::unordered_set;
using std::vector;
using std::filesystem::directory_entry;
using std::filesystem::directory_iterator;
using std::filesystem::is_regular_file;
using std::filesystem::path;
using std::this_thread::get_id;

const path audio_dir("/home/corey/scannerbot/audio");
const path transcript_dir("/home/corey/scannerbot/transcripts/");

std::atomic<bool> shutdown = false;
std::atomic<bool> do_watch;
std::unordered_map<string, thread> threadMap;
std::mutex threadMapMutex;

pid_t recorder_pid = -1;

void signalHandler(int signum)
{
    cout << '\n'
         << getpid() << " received interrupt signal (" << signum << ").\n";
    shutdown = true;
    do_watch = false;

    for (auto &[function, thread] : threadMap)
        if (thread.joinable())
            thread.join();

    exit(signum);
}

void kill_recorder()
{
    if (recorder_pid != -1)
        return;

    if (kill(recorder_pid, SIGTERM) == -1)
    {
        perror("Unable to kill recorder.");
        exit(EXIT_FAILURE);
    }

    cout << "\nRecorder [PID " << recorder_pid << "] killed.";

    recorder_pid = -1;
}

void run_recorder(char **recorder_args)
{
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
            cout << "\nRecorder exited with status " << WEXITSTATUS(status) << '\n';
        }
        else
        {
            cout << "\nChild process did not exit normally";
        }
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
            auto min_wait_s = std::chrono::seconds(120);
            auto time_now = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch());
            auto last_file_write_time =
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::filesystem::last_write_time(file).time_since_epoch());

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

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void run_cli()
{
    string input_line_str;
    std::stringstream input_line_stream;
    string command;
    cout << "scannerbot> ";

    while (not shutdown and getline(cin, input_line_str))
    {
        input_line_stream.str(input_line_str);
        input_line_stream >> command;
        cout << "command: " << command << "\nargs: ";

        if (command == "start")
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

            std::lock_guard<std::mutex> lock(threadMapMutex);

            if (threadMap.count("run_recorder") == 0)
                threadMap["run_recorder"] = thread(run_recorder, recorder_args);

            do_watch = true; // Enable forever directory watcher loop
            if (threadMap.count("watch_directories") == 0)
                threadMap["watch_directories"] = thread(watch_directories);
        }

        else if (command == "stop")
        {
            do_watch = false;

            std::lock_guard<std::mutex> lock(threadMapMutex);

            threadMap["run_recorder"].detach();
            threadMap["watch_directories"].detach();
            threadMap.erase("run_recorder");
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

        else
        {
            std::cout << "\nNo such option.\n";
        }

        std::cout << "scannerbot> ";
    }
}

int main()
{
    try
    {
        signal(SIGINT, signalHandler);

        std::unique_lock<std::mutex> lock(threadMapMutex);
        threadMap["run_cli"] = thread(run_cli);
        lock.unlock();

        for (auto &[function, thread] : threadMap)
            if (thread.joinable())
                thread.join();
    }
    catch (std::exception e)
    {
        cout << '\n'
             << e.what();
        exit(1);
    }
    return 0;
}
