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
# TODO: Fix filename generation.

# This function is used to generate unique file names.
timestamp() { date +"%m-%d-%Y-%H:%M:%S"; }

# SDR parameters
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
#$RTL_FM | $QUIET_SOX > /dev/null