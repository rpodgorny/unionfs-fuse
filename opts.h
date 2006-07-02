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

static struct fuse_opt unionfs_opts[] = {
	FUSE_OPT_KEY("--help", KEY_HELP),
	FUSE_OPT_KEY("--version", KEY_VERSION),
	FUSE_OPT_KEY("-h", KEY_HELP),
	FUSE_OPT_KEY("-V", KEY_VERSION),
	FUSE_OPT_KEY("stats", KEY_STATS),
	FUSE_OPT_END
};

int unionfs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs);


#endif
