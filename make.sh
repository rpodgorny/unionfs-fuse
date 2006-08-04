#!/bin/sh

[ -z ${CC} ] && CC=gcc

CFLAGS="${CFLAGS:--Wall}"
CPPFLAGS="${CPPFLAGS} -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26"
LDFLAGS="${LDFLAGS} -lfuse"

${CC} ${CPPFLAGS} ${CFLAGS} ${LDFLAGS} -o unionfs *.c "$@"
