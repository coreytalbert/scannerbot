#!/bin/bash
# recorder.sh
# Corey Talbert
# April 2023
#
# The user must supply frequency as a command line argument.

# File
timestamp() { date; }

# 160.71M - San Diego Trolly
FREQ=$1
RTL_MODE=fm
GAIN=50
RTL_SAMPLE_RATE=200k
RTL_FM_RESAMPLE_RATE=12k
SQUELCH=40

echo "Starting scannerbot recorder on frequency $1"

rtl_fm -M $RTL_MODE -s $RTL_SAMPLE_RATE -r $RTL_FM_RESAMPLE_RATE -g $GAIN -l $SQUELCH -f $FREQ -p 0 |
(sleep 3s; play -r $RTL_FM_RESAMPLE_RATE -t raw -e signed -b 16 -c 1 -V1 - |
rec "$(timestamp)".wav )
