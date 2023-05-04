#include <fcntl.h>
#include <iostream>
#include <mqueue.h>
#include <signal.h>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include <sys/wait.h>

using std::cout;

// The name of the recorder posix message queue
const char *REC_MQ_NAME = "/sb_rec_inbox";
const char *BUS_MQ_NAME = "/sb_bus_inbox";
// Map of posix message queue names to their file descriptors
std::unordered_map<const char *, mqd_t> mqdMap;

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

/*signal handler*/

int main()
{

    mq_init();
    cout << "\nThe recorder is running!";
    char message[256];
    mq_receive(mqdMap[REC_MQ_NAME], message, sizeof(message), nullptr);
    cout << message;

    for (auto &[queue_name, mqd] : mqdMap)
    {
        mq_close(mqd);
    }
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