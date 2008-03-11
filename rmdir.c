/*
*  C Implementation: rmdir
*
* Description: unionfs rmdir() call
*              If the directory to remove exists in a lower branch, create a
*              file with a tag informing other functions that the file
*              is hidden.
*
* original implementation by Radek Podgorny
*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>

#include "unionfs.h"
#include "opts.h"
#include "debug.h"
#include "cow.h"
#include "general.h"
#include "findbranch.h"

/**
  * If the root that has the directory to be removed is in read-only mode,
  * we create a file with a HIDE tag in an upper level root.
  * To other fuse functions this tag means, not to expose the 
  * lower level directory.
  */
static int rmdir_ro(const char *path, int root_ro) {
	int i = -1;

	// find a writable root above root_ro
	int root_rw = find_lowest_rw_root(root_ro);

	if (root_rw >= 0) i = path_create_cutlast(path, root_ro, root_rw);

	// no writable path, or some other error
	if (i < 0) return -EACCES;

	if (hide_file(path, root_rw) == -1) {
		// creating the file with the hide tag failed
		// TODO: open() error messages are not optimal on rmdir()
		return -errno;
	}

	return 0;
}

/**
  * If the root that has the directory to be removed is in read-write mode,
  * we can really delete the file.
  */
static int rmdir_rw(const char *path, int root_rw) {
	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[root_rw].path, path);

	int res = rmdir(p);

	if (res == -1) return -errno;

	return 0;
}

/**
  * rmdir() call
  */
int unionfs_rmdir(const char *path) {
	DBG("rmdir\n");
	
	to_user();

	int i = find_rorw_root(path);
	if (i == -1) {
		to_root();
		return -errno;
	}

	int res;
	// root is read-only and cow is enabled
	if (!uopt.roots[i].rw && uopt.cow_enabled) {
		res = rmdir_ro(path, i);
		to_root();
		return res;
	}
	
	res = rmdir_rw(path, i);
	if (res == 0)
		unionfs_rmdir(path); /* make _real_ sure the directory is removed */
	to_root();
	return res;
}
