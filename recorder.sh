#!/bin/bash
# recorder.sh
# Corey Talbert
# April 2023
#
# The user must supply frequency as a command line argument.
#
# TODO: Automatic noise floor detection.
# TODO: Default parameters.
# TODO: Take command line arguments.
# TODO: Improve audio quality.

# This function is used to generate unique file names.
timestamp() { date +"%m-%d-%Y-%H:%M:%S"; }

# SDR parameters
# 160.71M - San Diego Trolly\
# -f 160.665M -f 160.380M -f 161.295M -f 161.565M -f 161.565
FREQ="-f 160.71M"
RTL_MODE=fm # -M
GAIN=50 # -g
SDR_SAMPLE_RATE=16k # -s
BANDWIDTH=4k # -r
SQUELCH="6" # -l
# SoX parameters
SILENCE_THRESHOLD=-35d
PLAY="sox -S -r $BANDWIDTH -e signed -b 16 -c 1 -V1 -t raw - -d"
REC="sox -S -r $BANDWIDTH -e signed -b 16 -c 1 -V1 -t raw - $(timestamp).mp3 \
    silence 1 00:00:00.2 $SILENCE_THRESHOLD 1 00:00:01 $SILENCE_THRESHOLD : newfile : restart"

echo "Starting scannerbot recorder on frequency $FREQ."

# Starting the stram of data from the radio.
rtl_fm $FREQ -M $RTL_MODE -s $SDR_SAMPLE_RATE -r $BANDWIDTH -g $GAIN -l $SQUELCH -p 0 |
#sox -S -r $BANDWIDTH -e signed -b 16 -c 1 -V1 -t raw - $(timestamp).mp3 silence 1 00:00:00.5 1% 1 00:00:03 1% : newfile : restart
tee >($PLAY) >($REC) > /dev/null