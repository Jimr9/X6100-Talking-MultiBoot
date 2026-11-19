#!/bin/bash
# The talking menu: a static /sbin/init in a small ext4 root filesystem
set -euo pipefail
SRC=$(cd "$(dirname "$0")" && pwd)
W=~/talkmenu
CC=~/openx6x00/AetherX6100Buildroot/build/host/bin/arm-buildroot-linux-gnueabihf-gcc
mkdir -p $W
$CC -O2 -Wall -Wno-unused-result -static -s -o $W/talkmenu $SRC/talkmenu.c
file $W/talkmenu
R=$W/root
rm -rf $R && mkdir -p $R/sbin $R/dev $R/proc $R/sys $R/boot $R/tmp $R/donor
cp $W/talkmenu $R/sbin/init
rm -f $W/talkmenu-rootfs.ext4
mke2fs -q -t ext4 -L TALK -O ^metadata_csum_seed -d $R $W/talkmenu-rootfs.ext4 8M
e2fsck -n -f $W/talkmenu-rootfs.ext4 >/dev/null && echo "rootfs ok"
ls -la $W/talkmenu $W/talkmenu-rootfs.ext4
