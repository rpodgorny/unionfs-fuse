/*
* Description: find a file in a branch
*
* License: BSD-style license
*
* original implementation by Radek Podgorny
*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*
*
* Details about finding branches:
* 	We begin at the top-level branch to look for a file or directory, in
*	the code usually called "path". If path was not found, we check for a
*	whiteout-file/directory. If we find a whiteout, we won't check further
*	in lower level branches. If neither path nor the corresponding whiteout
*	have been found, we do the test the next branch and so on.
*	If a file was found, but it is on a read-only branch and a read-write
*	branch was requested we return EACCES. On the other hand we ignore
*	directories on read-only branches, since the directory in the higher
*	level branch doesn't prevent the user can later on see the file on the
*	lower level branch - so no problem to create path in the lower level
*	branch.
*	It also really important the files in higher level branches have
*	priority, since this is the reason, we can't write to file in a
*	lower level branch, when another file already exists in a higher
*	level ro-branch - on further access to the file the unmodified
*	file in the ro-branch will visible.
* TODO Terminology:
* 	In the code below we have functions like find_lowest_rw_branch().
* 	IMHO this is rather mis-leading, since it actually will find the
* 	top-level rw-branch. The naming of the function is due to the fact
* 	we begin to cound branches with 0, so 0 is actually the top-level
* 	branch with highest file serving priority.
*/

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

#include "unionfs.h"
#include "opts.h"
#include "general.h"
#include "cow.h"
#include "findbranch.h"
#include "string.h"
#include "debug.h"
#include "usyslog.h"

/**
 *  Find a branch that has "path". Return the branch number.
 */
static int find_branch(const char *path, int *branch, searchflag_t flag) {
	DBG("%s\n", path);

	int i, res;
	for (i = 0; i < uopt.nbranches; i++) {
		char p[PATHLEN_MAX];
		if ((res = BUILD_PATH(p, uopt.branches[i].path, path)) < 0) {
			RETURN(res);
		}

		struct stat stbuf;
		res = lstat(p, &stbuf);

		DBG("%s: res = %d\n", p, res);

		if (res == 0) { // path was found
			switch (flag) {
			case RWRO:
				// any path we found is fine
				*branch = i;
				RETURN(0);
			case RWONLY:
				// we need a rw-branch
				if (uopt.branches[i].rw) {
					*branch = i;
					RETURN(0);
				}
				break;
			default:
				USYSLOG(LOG_ERR, "%s: Unknown flag %d\n", __func__, flag);
			}
		}

		// check check for a hide file, checking first here is the magic to hide files *below* this level
		res = path_hidden(path, i);
		if (res > 0) {
			// So no path, but whiteout found. No need to search in further branches
			RETURN(-ENOENT);
		} else if (res < 0) {
			RETURN(res);
		}
	}

	RETURN(-ENOENT);
}

/**
 * Find a ro or rw branch.
 */
int find_rorw_branch(const char *path, int *branch) {
	DBG("%s\n", path);
	int res = find_branch(path, branch, RWRO);
	RETURN(res);
}

/**
 * Find a writable branch. If file does not exist, we check for
 * the parent directory.
 * @path 	- the path to find or to copy (with last element cut off)
 * @ rw_hint	- the rw branch to copy to, set to -1 to autodetect it
 */
int __find_rw_branch_cutlast(const char *path, int *branch, int rw_hint) {
	int res = find_rw_branch_cow(path, branch);
	DBG("branch = %d\n", *branch);

	if (res == 0 || (res < 0 && res != -ENOENT)) RETURN(res);

	DBG("Check for parent directory\n");

	// So path does not exist, now again, but with dirname only.
	// We MUST NOT call find_rw_branch_cow() // since this function
	// doesn't work properly for directories.
	char *dname = u_dirname(path);
	if (dname == NULL) {
		RETURN(-ENOMEM);
	}

	res = find_rorw_branch(dname, branch);
	DBG("branch = %d\n", *branch);

	// No branch found, so path does nowhere exist, error
	if (res < 0) goto out;

	// Reminder rw_hint == -1 -> autodetect, we do not care which branch it is
	if (uopt.branches[*branch].rw
	&& (rw_hint == -1 || *branch == rw_hint)) {
		res = 0;
		goto out;
	}

	if (!uopt.cow_enabled) {
		// So path exists, but is not writable.
		res = -EACCES;
		goto out;
	}

	int branch_rw;
	// since it is a directory, any rw-branch is fine
	if (rw_hint == -1)
		res = find_lowest_rw_branch(uopt.nbranches, &branch_rw);
	else
		res = branch_rw = rw_hint;

	DBG("branch_rw = %d\n", branch_rw);

	// no writable branch found, we must return an error
	if (res < 0 || branch_rw < 0) {
		res = -EACCES;
		goto out;
	}

	if (path_create(dname, *branch, branch_rw) == 0) *branch = branch_rw; // path successfully copied

out:
	free(dname);

	RETURN(res);
}

/**
 * Call __find_rw_branch_cutlast()
 */
int find_rw_branch_cutlast(const char *path, int *branch) {
	int rw_hint = -1; // autodetect rw_branch
	int res = __find_rw_branch_cutlast(path, branch, rw_hint);
	RETURN(res);
}

int find_rw_branch_cow(const char *path, int *branch) {
	return find_rw_branch_cow_common(path, branch, false);
}

/**
 * copy-on-write
 * Find path in a union branch and if this branch is read-only,
 * copy the file to a read-write branch.
 * NOTE: Don't call this to copy directories. Use path_create() for that!
 *       It will definitely fail, when a ro-branch is on top of a rw-branch
 *       and a directory is to be copied from ro- to rw-branch.
 */
int find_rw_branch_cow_common(const char *path, int *branch, bool copy_dir) {
	DBG("%s\n", path);

	int branch_rorw, branch_rw, res;

	// not found anywhere
	if ((res = find_rorw_branch(path, &branch_rorw)) < 0) RETURN(res);

	// the found branch is writable, good!
	if (uopt.branches[branch_rorw].rw) {
		*branch = branch_rorw;
		RETURN(0);
	}

	// cow is disabled and branch is not writable, so deny write permission
	if (!uopt.cow_enabled) {
		RETURN(-EACCES);
	}

	if ((res = find_lowest_rw_branch(branch_rorw, &branch_rw)) < 0) {
		// no writable branch found
		RETURN(-EACCES);
	}

	if ((res = cow_cp(path, branch_rorw, branch_rw, copy_dir)) < 0)
		RETURN(res);

	// remove a file that might hide the copied file
	remove_hidden(path, branch_rw);

	*branch = branch_rw;

	RETURN(0);
}

/**
 * Find lowest possible writable branch but only lower than branch_ro.
 */
int find_lowest_rw_branch(int branch_ro, int *branch) {
	DBG_IN();

	int i;
	for (i = 0; i < branch_ro; i++) {
		if (uopt.branches[i].rw) {
			*branch = i;
			RETURN(0);
		}
	}

	RETURN(-1);
}
