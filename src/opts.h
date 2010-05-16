/*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*/

#ifndef OPTS_H
#define OPTS_H


#include <fuse.h>
#include <stdbool.h>

#include "unionfs.h"


#define ROOT_SEP ":"

typedef struct {
	int nbranches;
	branch_entry_t *branches;

	bool stats_enabled;
	bool cow_enabled;
	bool statfs_omit_ro;
	int doexit;
	int retval;
	char *chroot; 		// chroot we might go into
	bool hide_meta_dir;
	bool relaxed_permissions;

} uopt_t;

enum {
	KEY_CHROOT,
	KEY_COW,
	KEY_HELP,
	KEY_HIDE_METADIR,
	KEY_MAX_FILES,
	KEY_NOINITGROUPS,
	KEY_RELAXED_PERMISSIONS,
	KEY_STATFS_OMIT_RO,
	KEY_STATS,
	KEY_VERSION
};


extern uopt_t uopt;


void uopt_init();
int unionfs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs);
void unionfs_post_opts();

#endif
