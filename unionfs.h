#ifndef UNIONFS_H
#define UNIONFS_H

#define PATHLEN_MAX 1024
#define HIDETAG "_HIDDEN~"

typedef struct {
	char *path;
	unsigned char rw; // the writable flag
} root_entry_t;

#endif
