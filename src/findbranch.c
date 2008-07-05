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
*	branch was requested we return EACCESS. On the other hand we ignore
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
#include <syslog.h>

#include "unionfs.h"
#include "opts.h"
#include "general.h"
#include "cow.h"
#include "findbranch.h"
#include "string.h"

/*
 *  Find a branch that has "path". Return the branch number.
 */
static int find_branch(const char *path, searchflag_t flag) {
	int i = 0;
	for (i = 0; i < uopt.nbranches; i++) {
		char p[PATHLEN_MAX];
		snprintf(p, PATHLEN_MAX, "%s%s", uopt.branches[i].path, path);

		struct stat stbuf;
		int res = lstat(p, &stbuf);

		if (res == 0) { // path was found
			switch (flag) {
			case RWRO:  
				// any path we found is fine
				return i; break; 
			case RWONLY: 
				// we need a rw-branch
				if (!uopt.branches[i].rw) break; 
			default: 
				syslog(LOG_ERR, "%s: Unknown flag %d\n", __func__, flag);
			}
		}

		// check check for a hide file, checking first here is the magic to hide files *below* this level
		if (path_hidden(path, i)) {
			// So no path, but whiteout found. No need to search in further branches
			errno = ENOENT;
			return -1;
		}
	}

	errno = ENOENT;
	return -1;
}

/**
 * Find a ro or rw branch.
 */
int find_rorw_branch(const char *path) {
	return find_branch(path, RWRO);
}

/**
 * Find a rw branch.
 */
static int find_rw_branch(const char *path) {
	return find_branch(path, RWONLY);
}

/*
 * Find a writable branch. If file does not exist, we check for 
 * the parent directory.
 */
int find_rw_branch_cutlast(const char *path) {
	int branch = find_rw_branch_cow(path);

	if (branch < 0 && errno == ENOENT) {
		// So path does not exist, now again, but with dirname only
		char *dname = u_dirname(path);
		int branch_rorw = find_rw_branch(dname);
		free(dname);

		// nothing found
		if (branch_rorw < 0) return -1;

		// the returned branch is writable, good!
		if (uopt.branches[branch_rorw].rw) return branch_rorw;
		
		// cow is disabled and branch is not writable, so deny write permission
		if (!uopt.cow_enabled) {
			errno = EACCES;
			return -1;
		}

		int branch_rw = find_lowest_rw_branch(branch_rorw);

		// no writable branch found, we must return an error
		if (branch_rw < 0) return -1;

		dname = u_dirname(path);
		int res = path_create(dname, branch_rorw, branch_rw);
		free(dname);

		// creating the path failed
		if (res) return -1;

		return branch_rw;
	}

	return branch;
}

/*
 * copy-on-write
 * Find path in a union branch and if this branch is read-only, 
 * copy the file to a read-write branch.
 */
int find_rw_branch_cow(const char *path) {
	int branch_rorw = find_rorw_branch(path);

	// not found anywhere
	if (branch_rorw < 0) return -1;

	// the found branch is writable, good!
	if (uopt.branches[branch_rorw].rw) return branch_rorw;

	// cow is disabled and branch is not writable, so deny write permission
	if (!uopt.cow_enabled) {
		errno = EACCES;
		return -1;
	}

	int branch_rw = find_lowest_rw_branch(branch_rorw);
	if (branch_rw < 0) {
		// no writable branch found
		errno = EACCES;
		return -1;
	}

	if (cow_cp(path, branch_rorw, branch_rw)) return -1;

	// remove a file that might hide the copied file
	remove_hidden(path, branch_rw);

	return branch_rw;
}

/*
 * Find lowest possible writable branch but only lower than branch_ro.
 */
int find_lowest_rw_branch(int branch_ro) {
	int i = 0;
	for (i = 0; i < branch_ro; i++) {
		if (uopt.branches[i].rw) return i; // found it it.
	}

	return -1;
}
