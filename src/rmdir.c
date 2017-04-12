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
#include <sys/types.h>
#include <dirent.h>

#include "unionfs.h"
#include "opts.h"
#include "debug.h"
#include "cow.h"
#include "general.h"
#include "findbranch.h"
#include "string.h"
#include "readdir.h"
#include "usyslog.h"

/**
  * If the branch that has the directory to be removed is in read-write mode,
  * we can really delete the file.
  */
static int rmdir_rw(const char *path, int branch_rw) {
	DBG("%s\n", path);

	int res;
	char p[PATHLEN_MAX];
	if ((res = BUILD_PATH(p, uopt.branches[branch_rw].path, path)) < 0) return res;

	if ((res = rmdir(p)) == -1) return -errno;

	return 0;
}

/**
  * If the branch that has the directory to be removed is in read-only mode,
  * we create a file with a HIDE tag in an upper level branch.
  * To other fuse functions this tag means, not to expose the
  * lower level directory.
  */
static int rmdir_ro(const char *path, int branch_ro) {
	DBG("%s\n", path);

	int branch_rw, res;

	// find a writable branch above branch_ro
	if ((res = find_lowest_rw_branch(branch_ro, &branch_rw)) < 0) return -EACCES;

	DBG("Calling hide_dir\n");
	if ((res = hide_dir(path, branch_rw)) < 0) {
		switch (res) {
		case (-EEXIST):
		case (-ENOTDIR):
		case (-ENOTEMPTY):
			// catch errors not allowed for rmdir()
			USYSLOG (LOG_ERR, "%s: Creating the whiteout failed: %s\n",
				__func__, strerror(-res));
			res = -EFAULT;
		}
		return res;
	}

	return 0;
}

/**
  * rmdir() call
  */
int unionfs_rmdir(const char *path) {
	DBG("%s\n", path);

	if (dir_not_empty(path)) return -ENOTEMPTY;

	int i, res;

	if ((res = find_rorw_branch(path, &i)) < 0) return res;

	if (!uopt.branches[i].rw) {
		// read-only branch
		if (!uopt.cow_enabled) {
			res = -EROFS;
		} else {
			res = rmdir_ro(path, i);
		}
	} else {
		// read-write branch
		res = rmdir_rw(path, i);
		if (res == 0) {
			// No need to be root, whiteouts are created as root!
			maybe_whiteout(path, i, WHITEOUT_DIR);
		}
	}

	return res;
}
