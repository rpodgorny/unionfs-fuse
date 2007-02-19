CFLAGS += -Wall
CPPFLAGS += -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26
LDFLAGS += -lfuse -lm


HASHTABLE_OBJ = hashtable.o hashtable_itr.o hash.o elfhash.o
UNIONFS_OBJ = unionfs.o stats.o opts.o debug.o findbranch.o readdir.o general.o unlink.o cow.o cow_utils.o


unionfs: $(UNIONFS_OBJ) $(HASHTABLE_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(UNIONFS_OBJ) $(HASHTABLE_OBJ)

clean:
	rm -f unionfs 
	rm -f *.o	
