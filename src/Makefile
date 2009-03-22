CFLAGS += -Wall
CPPFLAGS += -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26
#CPPFLAGS += -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 -DHAVE_SETXATTR
LDFLAGS += 

LIB = -lfuse -lpthread -lm

HASHTABLE_OBJ = hashtable.o hashtable_itr.o
UNIONFS_OBJ = unionfs.o stats.o opts.o debug.o findbranch.o readdir.o general.o unlink.o rmdir.o cow.o cow_utils.o string.o


unionfs: $(UNIONFS_OBJ) $(HASHTABLE_OBJ) version.h
	$(CC) $(LDFLAGS) -o $@ $(UNIONFS_OBJ) $(HASHTABLE_OBJ) $(LIB)

clean:
	rm -f unionfs
	rm -f *.o
