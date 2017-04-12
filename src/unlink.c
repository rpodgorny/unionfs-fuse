/*
*  C Implementation: unlink
*
* Description: unionfs unlink() call
*              If the file to unlink exists in a lower branch, create a
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
#include "string.h"

/**
  * If the branch that has the file to be unlinked is in read-only mode,
  * we create a file with a HIDE tag in an upper level branch.
  * To other fuse functions this tag means, not to expose the 
  * lower level file.
  */
static int unlink_ro(const char *path, int branch_ro) {
	DBG("%s\n", path);

	int branch_rw, res;
	// find a writable branch above branch_ro
	if ((res = find_lowest_rw_branch(branch_ro, &branch_rw)) < 0)
		RETURN(-EACCES);

	if ((res = hide_file(path, branch_rw)) < 0) {
		// creating the file with the hide tag failed
		// TODO: open() error messages are not optimal on unlink()
		RETURN(res);
	}

	RETURN(0);
}

/**
  * If the branch that has the file to be unlinked is in read-write mode,
  * we can really delete the file.
  */
static int unlink_rw(const char *path, int branch_rw) {
	DBG("%s\n", path);

	int res;
	char p[PATHLEN_MAX];
	if ((res = BUILD_PATH(p, uopt.branches[branch_rw].path, path)) < 0) RETURN(res);

	if ((res = unlink(p)) == -1) RETURN(-errno);

	RETURN(0);
}

/**
  * unlink() call
  */
int unionfs_unlink(const char *path) {
	DBG("%s\n", path);

	int i, res;

	if ((res = find_rorw_branch(path, &i)) < 0) RETURN(res);

	if (!uopt.branches[i].rw) {
		// read-only branch
		if (!uopt.cow_enabled) {
			res = -EROFS;
		} else {
			res = unlink_ro(path, i);
		}
	} else {
		// read-write branch
		res = unlink_rw(path, i);
		if (res == 0) {
			// No need to be root, whiteouts are created as root!
			maybe_whiteout(path, i, WHITEOUT_FILE);
		}
	}

	RETURN(res);
}
