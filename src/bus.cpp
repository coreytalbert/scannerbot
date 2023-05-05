#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <iostream>
#include <mqueue.h>
#include <mutex>
#include <spawn.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unordered_map>

using std::cin;
using std::cout;
using std::string;
using std::thread;
using namespace std::this_thread;
using namespace std::chrono;

// Signals that the user has requested to quit the program
std::atomic<bool> do_shutdown = false;

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

void kill_recorder();
void interruptHandler();
void run_recorder(string);
void run_cli();
void show_help();
void mq_init();
void cleanup();

void cleanup()
{
    kill_recorder();

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
    char ** recorder_args = new char*[20];
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
    {
        cout << "\nRecorder exited with status " << WEXITSTATUS(status);
    }
    else
    {
        cout << "\nChild process did not exit normally";
    }

    cout << "\nEnd of run_recorder";
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
         << R"(        squelch   Set radio quelch             )" << '\n'
         << "\n"
         << std::endl;
}

void run_cli()
{
    string input_line_str;
    string command;
    cout << "scannerbot> ";

    while (not do_shutdown and getline(cin, input_line_str))
    {
        command.clear();

        input_line_stream.str(input_line_str);
        input_line_stream >> command;
        cout << "command: " << command << "\nargs: ";

        if (command == "start" or command == "s")
        {
            std::lock_guard<std::mutex> lock(threadMapMutex);

            if (threadMap.count("run_recorder") == 0)
                threadMap["run_recorder"] = new thread(run_recorder, input_line_str);
        }

        else if (command == "stop")
        {
            std::lock_guard<std::mutex> lock(threadMapMutex);

            kill_recorder();
            if (threadMap["run_recorder"]->joinable())
                threadMap["run_recorder"]->join();
            threadMap.erase("run_recorder");
        }

        else if (command == "gain")
        {
        }

        else if (command == "frequency")
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

        std::cout << "scannerbot> ";
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
