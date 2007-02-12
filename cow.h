#ifndef COW_H
#define COW_H

#include <sys/stat.h>

#define VM_AND_BUFFER_CACHE_SYNCHRONIZED
#define MAXBSIZE 4096

struct cow {
	mode_t umask;
	uid_t uid;

	// source file
	char  *from_path;
	struct stat *stat;

	// destination file
	char *to_path;
};

int cow_cp(const char *path, int root_ro, int root_rw);
int path_create(const char *path, int nroot_ro, int nroot_rw);
int path_create_cutlast(const char *path, int nroot_ro, int nroot_rw);

#endif
