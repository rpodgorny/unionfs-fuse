CFLAGS += -Wall
CPPFLAGS += -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26
#CPPFLAGS += -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 -DHAVE_SETXATTR
LDFLAGS += 
DESTDIR?=/usr/local

LIB = -lfuse -lpthread -lm

build:
	make -C src/

clean:
	make -C src/ clean

install: build
	cp src/unionfs $(DESTDIR)/sbin/
	cp man/unionfs-fuse.8 $(DESTDIR)/share/man/man8/
