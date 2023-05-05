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

void cleanup()
{
    // Close connection to message queues
    for (auto &[queue_name, mqd] : mqdMap)
        mq_close(mqd);

    // Clean up threads
    for (auto &[function, thread] : threadMap)
        if (thread->joinable())
            thread->join();
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
        cout << "\nThe recorder is watching the message queue.";
        char message[256] = {""};
        // mq_receive blocks until there's something in the queue
        mq_receive(mqdMap[REC_MQ_NAME], message, sizeof(message), nullptr);
        cout << "\nmessage: " << message;

        if (message[0] == '\0')
            continue; // Empty message.

        char *command;
        char *option;
        char *arg;

        // Parse message into command, options, and arguments
        command = strtok(message, " ");
        cout << "\ncommand: " << command;
        string valid_options = "fgMsrl";
        while ((option = strtok(nullptr, " ")) != nullptr)
        {
            arg = strtok(nullptr, " ");
            if (valid_options.find(option) != string::npos)
            {
                radioOptions[option] = arg;
            }

            // cout << "\nmap size: " << radioOptions.size() << " option: "
            //      << option << " arg: " << radioOptions[option]; // wtf
        }

        char **radio_args = new char *[radioOptions.size()];
        int i = 0;
        for (auto &[opt, arg] : radioOptions)
        {
            radio_args[i] = new char[16];
            string opt_arg = '-' + opt + ' ' + arg;
            strcpy(radio_args[i], opt_arg.c_str());
            cout << '\n'
                 << radio_args[i];
            i++;
        }

        if (strcmp(command, "start") == 0)
        {
            cout << "\nReceived command \"start\"";
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
        }
        else if (strcmp(command, "freq") == 0)
        {
        }
        else if (strcmp(command, "gain") == 0)
        {
        }
        else if (strcmp(command, "squelch") == 0)
        {
        }
        else
        {
            cout << "\nNot a command.";
        }
    }
}

void kill_rec_shell()
{
    if (rec_shell_pid == -1)
        return;

    // Kill entire recorder process group
    if (kill(rec_shell_pid, SIGTERM) == -1)
    {
        perror("Unable to kill recorder shell script.");
        exit(EXIT_FAILURE);
    }

    cout << "\nRecorder shell script [PID " << rec_shell_pid << "] killed.";

    rec_shell_pid = -1;
}

int main()
{

    mq_init();
    threadMap["mq_watcher"] = new thread(mq_watcher);
    cout << "\nThe recorder is running!";

    while (not do_shutdown)
        ;

    cleanup();
    /*
    // Parse command line
    std::unordered_map<char, std::vector<std::string>> options;
    char option;
    std::string optstring;
    while ( option = getopt(argc, argv, optstring.c_str()) != -1 ) {
        options[option].push_back(optarg);
    }
    */
    /*# SDR parameters
    # 160.71M - San Diego Trolly\
    # -f 160.665M -f 160.380M -f 161.295M -f 161.565M -f 161.565
    FREQ="-f 160.71M"
    RTL_MODE=fm # -M
    GAIN=50 # -g
    SDR_SAMPLE_RATE=8k # -s
    BANDWIDTH=4k # -r resample rate
    SQUELCH=25 # -l
    # SoX parameters
    SILENCE_THRESHOLD="1%"
    # SDR interface
    RTL_FM="rtl_fm $FREQ -M $RTL_MODE -g $GAIN -s $SDR_SAMPLE_RATE -r $BANDWIDTH -l $SQUELCH -p 0"
    # SoX commands
    PLAY="sox -t raw -r $BANDWIDTH -e signed -b 16 -c 1 -V3 - -d"
    REC="sox --temp ../tmp/ -t raw -r $BANDWIDTH -e signed -b 16 -c 1 -V3 - /home/corey/scannerbot/audio/$(timestamp).mp3 \
       silence 1 00:00:01 $SILENCE_THRESHOLD 1 00:00:01 $SILENCE_THRESHOLD : newfile : restart"
    QUIET_SOX="sox -S -r $BANDWIDTH -e signed -b 16 -c 1 -V1 -t raw - /home/corey/scannerbot/audio/$(timestamp).mp3 silence 1 00:00:00.5 1% 1 00:00:03 1% : newfile : restart"

    # Starting the stream of data from the radio.
    $RTL_FM | tee >($PLAY) >($REC) > /dev/null
    #$RTL_FM | $QUIET_SOX > /dev/null*/
}