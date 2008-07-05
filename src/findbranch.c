/*
* Description: find a file in a branch
*
* License: BSD-style license
*
*
* original implementation by Radek Podgorny
*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*
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

/*
 * Find a rw or ro branch that has "path". Return the branch number.
 */
int find_rorw_branch(const char *path) {
	bool hidden = false;

	int i = 0;
	for (i = 0; i < uopt.nbranches; i++) {
		char p[PATHLEN_MAX];
		snprintf(p, PATHLEN_MAX, "%s%s", uopt.branches[i].path, path);

		struct stat stbuf;
		int res = lstat(p, &stbuf);

		if (res == 0 && !hidden) {
			return i;
		} else if (hidden) {
			// the file is hidden in this branch, we *only* ignore it in branches below this level
			return -1;
		}
		// check check for a hide file, checking first here is the magic to hide files *below* this level
		hidden = path_hidden(path, i);
	}

	return -1;
}

/*
 *  Find a rw branch that has "path". Return the branch number.
 */
static int find_rw_branch(const char *path) {
	int i = 0;
	for (i = 0; i < uopt.nbranches; i++) {
		char p[PATHLEN_MAX];
		snprintf(p, PATHLEN_MAX, "%s%s", uopt.branches[i].path, path);

		struct stat stbuf;
		int res = lstat(p, &stbuf);

		if (res == 0 && uopt.branches[i].rw) return i;

		// check check for a hide file, checking first here is the magic to hide files *below* this level
		if (path_hidden(path, i)) {
			// So no path, but whiteout found. No need to search in further branches
			errno = ENOENT;
			return -1;
		}
	}

	return -1;
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
