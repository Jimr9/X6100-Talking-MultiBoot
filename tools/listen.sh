#!/bin/sh
# The talking menu's sound path on the running radio: the mixer, play mode, the standard player
for i in 1 2 3 4; do killall -9 pulseaudio 2>/dev/null; sleep 0.2; done
amixer -c 0 cset name='AIF1 DA0 Playback Volume' 161 >/dev/null
amixer -c 0 cset name='AIF1 Slot 0 Digital DAC Playback Switch' on >/dev/null
amixer -c 0 cset name='AIF1 DA0 Stereo Playback Route' 'Mix Mono' >/dev/null
amixer -c 0 cset name='Headphone Source Playback Route' DAC >/dev/null
amixer -c 0 cset name='Headphone Playback Switch' on >/dev/null
amixer -c 0 cset name='Headphone Playback Volume' 47 >/dev/null
/tmp/basewrite 12 0x8
/tmp/basewrite 14 20
for i in 1 2 3; do killall -9 pulseaudio 2>/dev/null; sleep 0.1; done
aplay -D hw:0,0 ${1:-/tmp/START2.WAV} 2>&1 | tail -2
/tmp/basewrite 12 0
echo done
