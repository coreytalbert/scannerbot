#include <atomic>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <mqueue.h>
#include <mutex>
#include <signal.h>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

using std::cout;
using std::mutex;
using std::string;
using std::thread;
using std::unordered_map;
using std::vector;

// Signals that the user has requested to quit the program
std::atomic<bool> do_shutdown = false;

// A map relating function names to the threads running them
unordered_map<string, thread *> threadMap;
// Use this mutex to lock the threadMap
mutex threadMapMutex;

// The name of the recorder posix message queue
const char *REC_MQ_NAME = "/sb_rec_inbox";
const char *BUS_MQ_NAME = "/sb_bus_inbox";
// Map of posix message queue names to their file descriptors
unordered_map<const char *, mqd_t> mqdMap;

// Map of rtl_fm options and their arguments
unordered_map<string, string> radioOptions;

// The process ID of the running recorder shell, if any. Set to -1 if not
pid_t rec_shell_pid = -1;

void mq_init();
void mq_watcher();
void cleanup();
void kill_rec_shell();
void interruptHandler(int signum);

void interruptHandler(int signum)
{
    cout << '\n'
         << getpid() << " received interrupt signal (" << signum << ").\n";
    do_shutdown = true;

    cleanup();

    exit(signum);
}

void cleanup()
{
    kill_rec_shell();

    // Close connection to message queues
    for (auto &[queue_name, mqd] : mqdMap)
        mq_close(mqd);

    // Clean up threads
    for (auto &[function, thread] : threadMap)
        if (thread->joinable())
            thread->join();
}

void kill_rec_shell()
{
    if (rec_shell_pid == -1)
        return;

    // Kill entire recorder process group
    if (kill(rec_shell_pid, SIGTERM) == -1)
    {
        perror("Unable to kill recorder shell script");
        return;
    }

    cout << "\nRecorder shell script [PID " << rec_shell_pid << "] killed.";

    rec_shell_pid = -1;
    
    string message = "rec shell killed";
    mq_send(mqdMap[BUS_MQ_NAME], message.c_str(), message.length()+1, 0);
}

void mq_init()
{
    // Connect to the bus inbox for sending messages to the bus
    mqdMap[BUS_MQ_NAME] = mq_open(BUS_MQ_NAME, O_WRONLY);
    if (mqdMap[BUS_MQ_NAME] == -1)
    {
        perror("bus inbox mq_open");
        exit(EXIT_FAILURE);
    }

    // Connect to the recorder inbox for reading messages from the bus
    mqdMap[REC_MQ_NAME] = mq_open(REC_MQ_NAME, O_RDONLY);
    if (mqdMap[REC_MQ_NAME] == -1)
    {
        perror("recorder inbox mq_open");
        exit(EXIT_FAILURE);
    }
}

void mq_watcher()
{
    while (not do_shutdown)
    {
        // Get next command from the command line interface
        char message[256] = {""};
        // mq_receive blocks until there's something in the queue
        mq_receive(mqdMap[REC_MQ_NAME], message, sizeof(message), nullptr);
        if (message[0] == '\0')
            continue; // Empty message.

        // Parse message into command and radio options map
        char *command, *option, *arg;
        command = strtok(message, " ");

        if (strcmp(command, "start") == 0)
        {
            string valid_options = "fgsrl";
            while ((option = strtok(nullptr, " ")) != nullptr)
            {
                arg = strtok(nullptr, " ");
                if (valid_options.find(option) != string::npos)
                    radioOptions[option] = arg;
            }
        }

        else if (strcmp(command, "freq") == 0)
        {
            strcpy(option, "f");
            if ((arg = strtok(nullptr, " ")) == nullptr)
            {
                cout << "\nMissing argument.";
                continue;
            }
            radioOptions["f"] = arg;
            cout << "\nGot message f " << arg;
        }

        else if (strcmp(command, "gain") == 0)
        {
            strcpy(option, "g");
            if ((arg = strtok(nullptr, " ")) == nullptr)
            {
                cout << "\nMissing argument.";
                continue;
            }
            radioOptions["g"] = arg;
            cout << "\nGot message g " << arg;
        }

        else if (strcmp(command, "squelch") == 0)
        {
            strcpy(option, "l");
            if ((arg = strtok(nullptr, " ")) == nullptr)
            {
                cout << "\nMissing argument.";
                continue;
            }
            radioOptions["l"] = arg;
            cout << "\nGot message l " << arg;
        }

        else if (strcmp(command, "quit") == 0)
        {
            cout << "\n\n*recorder got quit command\n\n";
            do_shutdown = true;
            cleanup();
            exit(EXIT_SUCCESS);
        }

        else
        {
            cout << "\nNot a command.";
            continue;
        }

        // Place options/args into char* array for passing to spawn
        char **radio_args = new char *[radioOptions.size() + 1];
        radio_args[0] = new char[0]; // arg 0 is skipped by getopts in the bash script
        for (int i = 1; auto &[opt, arg] : radioOptions)
        {
            radio_args[i] = new char[16];
            string opt_arg = '-' + opt + ' ' + arg;
            strcpy(radio_args[i], opt_arg.c_str());
            i++;
        }

        // Terminate existing recorder shell
        kill_rec_shell();

        // Start new rtl_fm process using supplied arguments, if any
        int spawn_err = posix_spawn(&rec_shell_pid,
                                    "/home/corey/scannerbot/src/recorder.sh",
                                    nullptr, nullptr, radio_args, nullptr);
        // Error
        if (spawn_err != 0)
        {
            perror("\nError starting recorder\n");
            exit(EXIT_FAILURE);
        }

        cout << "\n\n*Rec shell has PID " << rec_shell_pid << "*\n\n";

        for (size_t i = 0; i < radioOptions.size() + 1; i++) {
            delete radio_args[i];
        }
        delete[] radio_args;
    }
}



int main()
{
    signal(SIGINT, interruptHandler); // Handler for keyboard interrupt

    mq_init();
    threadMap["mq_watcher"] = new thread(mq_watcher);

    while (not do_shutdown)
        ;

    cleanup();
}