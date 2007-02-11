#ifndef COW_H
#define COW_H

#include <sys/stat.h>

struct cow {
	mode_t umask;
	uid_t uid;
	
	// source file
	char  *from_path;
	struct stat *stat;
	
	// destination file
	char *to_path;
};

#define VM_AND_BUFFER_CACHE_SYNCHRONIZED
#define MAXBSIZE 4096

int path_create(const char *path, int nroot_ro, int nroot_rw);
int path_create_cutlast(const char *path, int nroot_ro, int nroot_rw);
int cow(const char *path);

int copy_special(struct cow *cow);
int copy_fifo(struct cow *cow);
int copy_link(struct cow *cow);
int copy_file(struct cow *cow);

#endif
