#ifndef UNIONFS_H
#define UNIONFS_H


#define PATHLEN_MAX 1024


typedef struct {
	char *path;
	unsigned char rw; // the writable flag
} root_entry_t;


int findroot(const char *path);
int findroot_cutlast(const char *path);

#endif
