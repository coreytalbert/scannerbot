#include <atomic>
#include <chrono>
#include <condition_variable>
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
using std::function;

const path audio_dir("/home/corey/scannerbot/audio");
const path transcript_dir("/home/corey/scannerbot/transcripts/");

std::atomic<bool> shutdown = false;
std::atomic<bool> do_watch;
std::unordered_map<string, thread> threadMap;
std::mutex threadMapMutex;





void signalHandler(int signum)
{
    cout << "\nInterrupt signal (" << signum << ") received.\n";
    shutdown = true;
    do_watch = false;

    for (auto &[function, thread] : threadMap)
        if (thread.joinable())
            thread.join();
    
    exit(signum);
}





void start_recorder() {
    int rec_ret_status = system("/home/corey/scannerbot/src/recorder.sh");
    //std::this_thread::yield();
}





void watch_directories()
{
    //cout << "\nThread " << std::this_thread::get_id() << " entering watch_directories";

    unordered_set<string> seen_audio_files;
    unordered_set<string> seen_transcript_files;

    while (do_watch and not shutdown)
    {
        //cout << "\nEntering watch loop...";
        // Handle new audio files.
        for (auto& file : directory_iterator(audio_dir))
        {
            //cout << "\nIterating audio directory...";
            // The file is already in the set.
            if (seen_audio_files.count(file.path()) > 0)
                continue;
            
            // Wait 10 seconds since last write to transcribe file.
            auto min_wait_s = std::chrono::seconds(120);
            auto time_now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
            auto last_file_write_time = std::chrono::duration_cast<std::chrono::seconds>(std::filesystem::last_write_time(file).time_since_epoch());
            if ((time_now - last_file_write_time) < min_wait_s)
                continue;

            // The file is too small to bother with.
            if (file.file_size() < 4096)
                continue;

            seen_audio_files.insert(file.path());

            // The file isn't audio.
            if (not is_regular_file(file.status()))
                continue;

            //cout << "\nNew audio detected: " << file.path();  
            // Send the new audio file to the transcriber.
            string command = "python3 /home/corey/scannerbot/src/transcriber.py \"";
            command += file.path();
            command += "\"";
            command += " > /dev/null";
            int trans_ret_status = system(command.c_str());
        }

        // Handle new transcripts.
        for (auto& file : directory_iterator(transcript_dir))
        {
            //cout << "\nIterating transcript directory...";
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
            int pub_ret_status = system(command.c_str());
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}





void start_cli() {
    std::string user_input;
    std::cout << "scannerbot> ";
    
    while (not shutdown and std::getline(std::cin, user_input)) {
        std::lock_guard<std::mutex> lock(threadMapMutex);

        if (user_input == "start") {
            if (threadMap.count("start_recorder") == 0) 
                threadMap["start_recorder"] = thread(start_recorder);

            do_watch = true;
            if (threadMap.count("watch_directories") == 0)
                threadMap["watch_directories"] = thread(watch_directories);
        }
        
        else if (user_input == "stop") {
            do_watch = false;
            threadMap["start_recorder"].detach();
            threadMap["watch_directories"].detach();
            threadMap.erase("start_recorder");
            threadMap.erase("watch_directories");
        }
        
        else if (user_input == "gain") {
            
        }
        
        else if (user_input == "frequency") {
            
        }
        
        else if (user_input == "quiet") {
            
        }
        
        else if (user_input == "verbose") {
            
        }
        
        else {
            std::cout << "\nNo such option.\n";
        }
        
        std::cout << "scannerbot> ";
    }
}

// void cli_worker() {
    
//     while (not shutdown){
//         std::unique_lock<std::mutex> lock(commandQueueMutex);
//         while (commandQueue.empty()) 
//             commandCV.wait(lock); // Waiting for user input.

//         if (shutdown) break;

//         function<void()> command = commandQueue.front();
//         commandQueue.pop();
//         thread * t = new thread(command);
//         threadMap[move(command)] = t;

//         // todo: enum
//         switch(option_selection.load()){
//             case(1): { // start
//                 thread * rec_thread = new thread(start_recorder);
//                 threadMap[&start_recorder] = rec_thread;

//                 std::this_thread::sleep_for(std::chrono::seconds(2));
                
//                 thread * watch_thread = new thread(watch_directories);
//                 threadMap[&watch_directories] = watch_thread;
//             }
//             case(2): { //stop
//                 // detach recorder thread
//             }
//         }

//         option_selection == -1;
//         commandCV.notify_one();
//     }
//}

int main()
{
    try {
        signal(SIGINT, signalHandler);

        std::unique_lock<std::mutex> lock(threadMapMutex);
        threadMap["start_cli"] = thread(start_cli);
        lock.unlock();
                
        for (auto &[function, thread] : threadMap)
            if (thread.joinable())
                thread.join();
    }
    catch (std::exception e) {
        cout << '\n' <<e.what() ;
        exit(1);
    }
    return 0;
}
