gcc -o unionfs *.c -Wall -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=22 -lfuse $@
