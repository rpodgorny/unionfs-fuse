#ifndef OPTS_H
#define OPTS_H


#include <fuse.h>
#include "unionfs.h"


#define ROOT_SEP ":"

enum {
	KEY_STATS,
	KEY_HELP,
	KEY_VERSION
};

int unionfs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs);


#endif
