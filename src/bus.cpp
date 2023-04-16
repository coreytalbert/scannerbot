#include <filesystem>
#include <chrono>
#include <iostream>
#include <unordered_set>
#include <thread>
using std::cout;
using std::filesystem::directory_iterator;
using std::filesystem::path;
using std::filesystem::is_regular_file;
using std::string;
using std::thread;
using std::unordered_set;

bool do_exit = false;

void watch_directory(const path& dir_path) {
    directory_iterator end_itr;

    unordered_set<string> seen_files;

    while (not do_exit) {
        for (directory_iterator itr(dir_path); itr != end_itr; ++itr) {
            if (is_regular_file(itr->status())) {
                string filename = itr->path().filename().string();
                if (seen_files.find(filename) == seen_files.end()) {
                    cout << "New file detected: " << itr->path().string() << '\n';
                    seen_files.insert(filename);
                    // Do something with the new file
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main() {
    path dir_path("path/to/directory");

    thread t(watch_directory, dir_path);

    // Do other stuff in the main thread

    t.join();

    return 0;
}
