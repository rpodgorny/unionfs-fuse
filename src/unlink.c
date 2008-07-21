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

/**
  * If the branch that has the file to be unlinked is in read-only mode,
  * we create a file with a HIDE tag in an upper level branch.
  * To other fuse functions this tag means, not to expose the 
  * lower level file.
  */
static int unlink_ro(const char *path, int branch_ro) {
	DBG_IN()	

	// find a writable branch above branch_ro
	int branch_rw = find_lowest_rw_branch(branch_ro);

	if (branch_rw < 0) return EACCES;

	if (hide_file(path, branch_rw) == -1) {
		// creating the file with the hide tag failed
		// TODO: open() error messages are not optimal on unlink()
		return errno;
	}

	return 0;
}

/**
  * If the branch that has the file to be unlinked is in read-write mode,
  * we can really delete the file.
  */
static int unlink_rw(const char *path, int branch_rw) {
	DBG_IN();
	
	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", uopt.branches[branch_rw].path, path);

	int res = unlink(p);
	if (res == -1) return errno;

	return 0;
}

/**
  * unlink() call
  */
int unionfs_unlink(const char *path) {
	DBG_IN();
	
	to_user();

	int i = find_rorw_branch(path);
	if (i == -1) {
		to_root();
		return errno;
	}

	int res;
	if (!uopt.branches[i].rw) {
		// read-only branch
		if (!uopt.cow_enabled) {
			res = EROFS;
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

	to_root();
	return -res;
}
