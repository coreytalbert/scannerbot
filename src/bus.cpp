#include <chrono>
#include <exception>
#include <iostream>
#include <filesystem>
#include <csignal>
#include <thread>
#include <unordered_set>
#include <vector>
using std::cout;
using std::string;
using std::this_thread::get_id;
using std::thread;
using std::unordered_set;
using std::vector;
using std::filesystem::directory_entry;
using std::filesystem::directory_iterator;
using std::filesystem::is_regular_file;
using std::filesystem::path;

const path audio_dir("/home/corey/scannerbot/audio");
const path transcript_dir("/home/corey/scannerbot/transcripts/");

bool do_exit = false;
vector<thread *> threads;

void signalHandler(int signum)
{
    cout << "\nInterrupt signal (" << signum << ") received.\n";
    do_exit = true;

    for (thread * t : threads)
        if (t->joinable())
            t->join();
    
    exit(signum);
}

void watch_directories()
{
    //cout << "\nThread " << std::this_thread::get_id() << " entering watch_directories";

    unordered_set<string> seen_audio_files;
    unordered_set<string> seen_transcript_files;

    while (not do_exit)
    {
        //cout << "\nEntering while loop...";
        // Handle new audio files.
        for (auto& file : directory_iterator(audio_dir))
        {
            //cout << "\nIterating audio directory...";
            if (seen_audio_files.count(file.path()) > 0)
                continue;
            
            seen_audio_files.insert(file.path());

            if (not is_regular_file(file.status()))
                continue;

            //cout << "\nNew audio detected: " << file.path();  
            // Send the new audio file to the transcriber.
            string command = "python3 /home/corey/scannerbot/src/transcriber.py \"";
            command += file.path();
            command += "\"";
            //command += " > /dev/null";
            int trans_ret_status = system(command.c_str());
        }

        // Handle new transcripts.
        for (auto& file : directory_iterator(transcript_dir))
        {
            cout << "\nIterating transcript directory...";
            if (seen_transcript_files.count(file.path()) > 0)
                continue;

            seen_transcript_files.insert(file.path());

            if (not is_regular_file(file.status()))
                continue;

            cout << "\nNew transcript detected: " << file.path();  
            // Send the new transcript file to the publisher.
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

int main()
{
    try {
        signal(SIGINT, signalHandler);

        threads.push_back(new thread(watch_directories));
        
        for (thread * t : threads)
            if (t->joinable())
                t->join();
    }
    catch (std::exception e) {
        cout << '\n' <<e.what() ;
        exit(1);
    }
    return 0;
}
