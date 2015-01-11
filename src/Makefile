CFLAGS += -Wall
CPPFLAGS += $(shell pkg-config --cflags fuse)
CPPFLAGS += -DFUSE_USE_VERSION=26

CPPFLAGS += -DLIBC_XATTR # glibc nowadays includes xattr
# CPPFLAGS += -DLIBATTR_XATTR # define this to libattr xattr include

# CPPFLAGS += -DDISABLE_XATTR # disable xattr support
# CPPFLAGS += -DDISABLE_AT    # disable *at function support

LDFLAGS += 

LIB = $(shell pkg-config --libs fuse) -lpthread

HASHTABLE_OBJ = hashtable.o hashtable_itr.o
UNIONFS_FUSE_OBJ = unionfs.o opts.o debug.o findbranch.o readdir.o \
		general.o unlink.o rmdir.o cow.o cow_utils.o string.o \
		usyslog.o
UNIONFS_FUSE_CTL_OBJ = unionfs-fuse-ctl.o


all: unionfs-fuse unionfs-fuse-ctl

unionfs-fuse: $(UNIONFS_FUSE_OBJ) $(HASHTABLE_OBJ) uioctl.h version.h
	$(CC) $(LDFLAGS) -o $@ $(UNIONFS_FUSE_OBJ) $(HASHTABLE_OBJ) $(LIB)

unionfs-fuse-ctl: $(UNIONFS_FUSE_CTL_OBJ) uioctl.h version.h
	$(CC) $(LDFLAGS) -o $@ $(UNIONFS_FUSE_CTL_OBJ)

clean:
	rm -f unionfs-fuse
	rm -f unionfs-fuse-ctl
	rm -f *.o
