CFLAGS += -W -Wall -fPIC

CPPFLAGS += $(shell pkg-config --cflags fuse3)
LIB = $(shell pkg-config --libs fuse3)
CPPFLAGS += -DFUSE_USE_VERSION=35

# set this even for 32bit systems - see https://github.com/rpodgorny/unionfs-fuse/issues/142
CPPFLAGS += -D_FILE_OFFSET_BITS=64

# define the following (and comment out the above) for libfuse2
#CPPFLAGS += $(shell pkg-config --cflags fuse)
#LIB = $(shell pkg-config --libs fuse) -lpthread
#CPPFLAGS += -DFUSE_USE_VERSION=29

CPPFLAGS += -DLIBC_XATTR # glibc nowadays includes xattr
# CPPFLAGS += -DLIBATTR_XATTR # define this to libattr xattr include

# CPPFLAGS += -DDISABLE_XATTR # disable xattr support
# CPPFLAGS += -DDISABLE_AT    # disable *at function support

LDFLAGS +=

HASHTABLE_OBJ = hashtable.o hashtable_itr.o
LIBUNIONFS_OBJ = fuse_ops.o opts.o debug.o findbranch.o readdir.o \
		general.o unlink.o rmdir.o cow.o cow_utils.o string.o \
		usyslog.o
UNIONFS_OBJ = unionfs.o
UNIONFSCTL_OBJ = unionfsctl.o


all: unionfs unionfsctl libunionfs.a libunionfs.so

unionfs: $(UNIONFS_OBJ) libunionfs.a uioctl.h version.h
	$(CC) $(LDFLAGS) -o $@ $(UNIONFS_OBJ) libunionfs.a $(LIB)

unionfsctl: $(UNIONFSCTL_OBJ) uioctl.h version.h
	$(CC) $(LDFLAGS) -o $@ $(UNIONFSCTL_OBJ)

libunionfs.a: $(LIBUNIONFS_OBJ) $(HASHTABLE_OBJ) uioctl.h version.h
	$(AR) rc $@ $(LIBUNIONFS_OBJ) $(HASHTABLE_OBJ)

libunionfs.so: libunionfs.a
	$(CC) -shared -o $@ $(LIBUNIONFS_OBJ) $(HASHTABLE_OBJ) $(LIB)

clean:
	rm -f unionfs
	rm -f unionfsctl
	rm -f *.o *.a *.so
