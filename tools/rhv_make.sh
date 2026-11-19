#!/bin/sh
# The talking menu's prompts in the radio's own voice (RHVoice), into /tmp/rhv
V=${1:-slt}
RATE=${2:-}
mkdir -p /tmp/rhv && rm -f /tmp/rhv/*
N1="X 6 1 hundred Access, version 1 point 2, with speech"
N2="Open X 6 X hundred, with speech"
N3="Open X 6 1 hundred, version 1 point 2, the original, no speech"
N4="Open X 6 X hundred, version 0 point 3 point 12, the original, no speech"
say() { echo "$2" > /tmp/rhv/in.txt; RHVoice-test -p "$V" $RATE -i /tmp/rhv/in.txt -o "/tmp/rhv/$1" 2>&1 | head -2; }
say MENU.WAV "Boot menu. Press F1 for $N1. Press F2 for $N2. Press F3 for $N3. Press F4 for $N4."
say START1.WAV "Starting $N1. Please wait."
say START2.WAV "Starting $N2. Please wait."
say START3.WAV "Starting $N3. Please wait."
say START4.WAV "Starting $N4. Please wait."
for k in 1 2 3 4; do say WAIT$k.WAV "If no key is pressed, F$k starts by itself."; done
say OFF.WAV "Power off."
rm -f /tmp/rhv/in.txt
ls -l /tmp/rhv
