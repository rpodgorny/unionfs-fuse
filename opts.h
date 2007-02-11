#ifndef OPTS_H
#define OPTS_H


#include <fuse.h>
#include <stdbool.h>

#include "unionfs.h"


#define ROOT_SEP ":"

typedef struct {
	int nroots;
	root_entry_t *roots;

	char stats_enabled;
	bool cache_enabled;
	int cache_time;

	int doexit;
} uopt_t;

enum {
	KEY_STATS,
	KEY_CACHE,
	KEY_CACHE_TIME,
	KEY_HELP,
	KEY_VERSION
};


extern uopt_t uopt;


void uopt_init();
int unionfs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs);


#endif
