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
static int find_branch(const char *path, searchflag_t flag) {
	DBG("%s\n", path);

	int i = 0;
	for (i = 0; i < uopt.nbranches; i++) {
		char p[PATHLEN_MAX];
		if (BUILD_PATH(p, uopt.branches[i].path, path)) RETURN(-ENAMETOOLONG);

		struct stat stbuf;
		int res = lstat(p, &stbuf);

		DBG("%s: res = %d\n", p, res);

		if (res == 0) { // path was found
			switch (flag) {
			case RWRO:
				// any path we found is fine
				RETURN(i);
			case RWONLY:
				// we need a rw-branch
				if (uopt.branches[i].rw) RETURN(i);
				break;
			default:
				USYSLOG(LOG_ERR, "%s: Unknown flag %d\n", __func__, flag);
			}
		}

		// check check for a hide file, checking first here is the magic to hide files *below* this level
		res = path_hidden(path, i);
		if (res > 0) {
			// So no path, but whiteout found. No need to search in further branches
			errno = ENOENT;
			RETURN(-1);
		} else if (res < 0) {
			errno = res; // error
			RETURN(-1);
		}
	}

	errno = ENOENT;
	RETURN(-1);
}

/**
 * Find a ro or rw branch.
 */
int find_rorw_branch(const char *path) {
	DBG("%s\n", path);
	int res = find_branch(path, RWRO);
	RETURN(res);
}

/**
 * Find a writable branch. If file does not exist, we check for
 * the parent directory.
 * @path 	- the path to find or to copy (with last element cut off)
 * @ rw_hint	- the rw branch to copy to, set to -1 to autodetect it
 */
int __find_rw_branch_cutlast(const char *path, int rw_hint) {
	int branch = find_rw_branch_cow(path);
	DBG("branch = %d\n", branch);

	if (branch >= 0 || (branch < 0 && errno != ENOENT)) RETURN(branch);

	DBG("Check for parent directory\n");

	// So path does not exist, now again, but with dirname only.
	// We MUST NOT call find_rw_branch_cow() // since this function 
	// doesn't work properly for directories.
	char *dname = u_dirname(path);
	if (dname == NULL) {
		errno = ENOMEM;
		RETURN(-1);
	}

	branch = find_rorw_branch(dname);
	DBG("branch = %d\n", branch);

	// No branch found, so path does nowhere exist, error
	if (branch < 0) goto out; 

	// Reminder rw_hint == -1 -> autodetect, we do not care which branch it is
	if (uopt.branches[branch].rw 
	&& (rw_hint == -1 || branch == rw_hint)) goto out;

	if (!uopt.cow_enabled) {
		// So path exists, but is not writable.
		branch = -1;
		errno = EACCES;
		goto out;
	}

	int branch_rw;
	// since it is a directory, any rw-branch is fine
	if (rw_hint == -1)
		branch_rw = find_lowest_rw_branch(uopt.nbranches);
	else
		branch_rw = rw_hint;

	DBG("branch_rw = %d\n", branch_rw);

	// no writable branch found, we must return an error
	if (branch_rw < 0) {
		branch = -1;
		errno = EACCES;
		goto out;
	}

	if (path_create(dname, branch, branch_rw) == 0) branch = branch_rw; // path successfully copied

out:
	free(dname);

	RETURN(branch);
}

/**
 * Call __find_rw_branch_cutlast()
 */
int find_rw_branch_cutlast(const char *path) {
	int rw_hint = -1; // autodetect rw_branch
	int res = __find_rw_branch_cutlast(path, rw_hint);
	RETURN(res);
}

int find_rw_branch_cow(const char *path) {
	return find_rw_branch_cow_common(path, false);
}

/**
 * copy-on-write
 * Find path in a union branch and if this branch is read-only, 
 * copy the file to a read-write branch.
 * NOTE: Don't call this to copy directories. Use path_create() for that!
 *       It will definitely fail, when a ro-branch is on top of a rw-branch
 *       and a directory is to be copied from ro- to rw-branch.
 */
int find_rw_branch_cow_common(const char *path, bool copy_dir) {
	DBG("%s\n", path);

	int branch_rorw = find_rorw_branch(path);

	// not found anywhere
	if (branch_rorw < 0) RETURN(-1);

	// the found branch is writable, good!
	if (uopt.branches[branch_rorw].rw) RETURN(branch_rorw);

	// cow is disabled and branch is not writable, so deny write permission
	if (!uopt.cow_enabled) {
		errno = EACCES;
		RETURN(-1);
	}

	int branch_rw = find_lowest_rw_branch(branch_rorw);
	if (branch_rw < 0) {
		// no writable branch found
		errno = EACCES;
		RETURN(-1);
	}

	if (cow_cp(path, branch_rorw, branch_rw, copy_dir)) RETURN(-1);

	// remove a file that might hide the copied file
	remove_hidden(path, branch_rw);

	RETURN(branch_rw);
}

/**
 * Find lowest possible writable branch but only lower than branch_ro.
 */
int find_lowest_rw_branch(int branch_ro) {
	DBG_IN();

	int i = 0;
	for (i = 0; i < branch_ro; i++) {
		if (uopt.branches[i].rw) RETURN(i); // found it it.
	}

	RETURN(-1);
}
