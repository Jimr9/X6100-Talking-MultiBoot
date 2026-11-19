#!/bin/sh
# the prompts in /tmp/wav onto the card's BOOT/TALK
set -e
mkdir -p /tmp/b; mount /dev/mmcblk0p1 /tmp/b
cp /tmp/wav/*.WAV /tmp/b/TALK/
ls -l /tmp/b/TALK | head -3
sync; umount /tmp/b
echo copied
