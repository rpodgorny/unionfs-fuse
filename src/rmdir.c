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
#include <syslog.h>

#include "unionfs.h"
#include "opts.h"
#include "debug.h"
#include "cow.h"
#include "general.h"
#include "findbranch.h"
#include "string.h"

/**
  * If the branch that has the directory to be removed is in read-write mode,
  * we can really delete the file.
  */
static int rmdir_rw(const char *path, int branch_rw) {
	DBG_IN();

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", uopt.branches[branch_rw].path, path);

	int res = rmdir(p);
	if (res == -1) return errno;

	return 0;
}

/**
  * If the branch that has the directory to be removed is in read-only mode,
  * we create a file with a HIDE tag in an upper level branch.
  * To other fuse functions this tag means, not to expose the 
  * lower level directory.
  */
static int rmdir_ro(const char *path, int branch_ro) {
	DBG_IN();

	// find a writable branch above branch_ro
	int branch_rw = find_lowest_rw_branch(branch_ro);

	if (branch_rw < 0) return -EACCES;

	if (hide_dir(path, branch_rw) == -1) {
		switch (errno) {
		case (EEXIST):
		case (ENOTDIR):
		case (ENOTEMPTY):
			// catch errors not allowed for rmdir()
			syslog (LOG_ERR, "%s: Creating the whiteout failed: %s\n",
				__func__, strerror(errno));
			errno = EFAULT;
		}
		return errno;
	}

	return 0;
}

/**
  * rmdir() call
  */
int unionfs_rmdir(const char *path) {
	DBG_IN();

	to_user();

	int i = find_rorw_branch(path);
	if (i == -1) {
		to_root();
		return -errno;
	}

	int res;
	if (!uopt.branches[i].rw) {
		// read-only branch
		if (!uopt.cow_enabled)
			res = EROFS;
		else
			res = rmdir_ro(path, i);
	} else {
		// read-write branch
		res = rmdir_rw(path, i);
		if (res == 0) {
			// No need to be root, whiteouts are created as root!
			maybe_whiteout(path, i, WHITEOUT_DIR);
		}
	}

	to_root();
	return -res;
}
