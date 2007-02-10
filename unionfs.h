#ifndef UNIONFS_H
#define UNIONFS_H

#include <fuse.h>

#define PATHLEN_MAX 1024
#define HIDETAG "_HIDDEN~"

typedef struct {
	char *path;
	unsigned char rw; // the writable flag
} root_entry_t;


int findroot(const char *path);
int findroot_cutlast(const char *path);
int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
		    struct fuse_file_info *fi);


#endif
