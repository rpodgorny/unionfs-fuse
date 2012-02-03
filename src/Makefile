CFLAGS += -Wall
CPPFLAGS += $(shell pkg-config --cflags fuse)
CPPFLAGS += -DFUSE_USE_VERSION=26
CPPFLAGS += -DHAVE_XATTR

LDFLAGS += 

LIB = $(shell pkg-config --libs fuse)

HASHTABLE_OBJ = hashtable.o hashtable_itr.o
UNIONFS_OBJ = unionfs.o stats.o opts.o debug.o findbranch.o readdir.o \
		general.o unlink.o rmdir.o cow.o cow_utils.o string.o \
		usyslog.o


unionfs: $(UNIONFS_OBJ) $(HASHTABLE_OBJ) version.h
	$(CC) $(LDFLAGS) -o $@ $(UNIONFS_OBJ) $(HASHTABLE_OBJ) $(LIB)

clean:
	rm -f unionfs
	rm -f *.o
