CFLAGS += -Wall
CPPFLAGS += -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26
LDFLAGS += -lfuse

UNIONFS_OBJ = unionfs.o stats.o opts.o debug.o cache.o


unionfs: $(UNIONFS_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(UNIONFS_OBJ) 

clean:
	rm -f unionfs 
	rm -f *.o	
