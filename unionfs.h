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
int wroot_from_list(int root_ro);

#endif
