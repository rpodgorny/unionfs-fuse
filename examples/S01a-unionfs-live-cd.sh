#!/bin/sh

# Copyright: Bernd Schubert <bernd.schubert@fastmail.fm>
# BSD license, see LICENSE file for details

FUSE_OPT="-o allow_other,suid,dev,nonempty"
CHROOT_PATH="/tmp/unionfs"
UNION_OPT="-ocow,chroot=$CHROOT_PATH,max_files=32768"

UBIN=/usr/bin/unionfs-fuse

mount -t proc proc /proc
mount -t tmpfs tmpfs /tmp

mkdir -p $CHROOT_PATH/root
mkdir -p $CHROOT_PATH/rw
mkdir -p /tmp/union

mount --bind / $CHROOT_PATH/root

$UBIN $FUSE_OPT $UNION_OPT /rw=RW:/root=RO /tmp/union

mount -t proc proc /tmp/union/proc

cd /tmp/union
mkdir oldroot
pivot_root . oldroot

init q

