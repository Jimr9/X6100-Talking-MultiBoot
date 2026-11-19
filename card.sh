#!/bin/bash
# The multi-boot card: F1 X6100 Access, F2 OpenX6X00 Speech Access, F3/F4 the original builds, F5 the talking menu.
#   card.sh OUT_NAME F1_IMAGE.zip F2_IMAGE.zip
# Needs ~/Xiegu6x00-Multi-Boot (TomekR1's tool, with x6100mb/data/u-boot-x6100-menu.bin from its release) and
# the talking menu's root filesystem from build.sh (~/talkmenu/talkmenu-rootfs.ext4). Writes OUT_NAME.img.zip
# to the Windows Downloads folder.
set -e
S=$(cd "$(dirname "$0")" && pwd)
NAME=${1:?out name}
F1=${2:?F1 image}
F2=${3:?F2 image}
DL=/mnt/c/Users/Jim/Downloads
W=~/mbcard-$NAME
mkdir -p $W && rm -f $W/*
cd ~/Xiegu6x00-Multi-Boot
python3 $S/talkcard.py $W/$NAME.img $S/prompts ~/talkmenu/talkmenu-rootfs.ext4 \
    "$F1" "$F2" openx6100@v1.2.0 openx6x00@v0.3.12
echo "== boot script, one-time part"
grep -c "bm_once" $W/$NAME.img.boot.cmd
cd $W && rm -f $DL/$NAME.img.zip && zip -j -q $DL/$NAME.img.zip $NAME.img
ls -la $DL/$NAME.img.zip
