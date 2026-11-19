#!/bin/sh
# Put the new talking menu on the card in the radio: its partition p8, the stereo prompts on BOOT
set -e
grep -q "mmcblk0p8" /proc/mounts && { echo "p8 is mounted, stopping"; exit 1; }
SZ=$(cat /sys/class/block/mmcblk0p8/size)
echo "p8 sectors: $SZ, new rootfs bytes: $(wc -c < /tmp/talk.ext4)"
[ $((SZ * 512)) -ge $(wc -c < /tmp/talk.ext4) ] || { echo "too big"; exit 1; }
dd if=/tmp/talk.ext4 of=/dev/mmcblk0p8 bs=1M conv=fsync 2>&1 | tail -1
mkdir -p /tmp/b; mount /dev/mmcblk0p1 /tmp/b
cp /tmp/wav/*.WAV /tmp/b/TALK/
ls -l /tmp/b/TALK | head -4
df -h /tmp/b | tail -1
sync; umount /tmp/b
ls /donor 2>/dev/null; echo installed
