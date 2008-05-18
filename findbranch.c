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

/**
 * If path exists, return the root number that has path.
 * TODO: We can still stat() fname_~HIDDEN, though these are hidden by readdir()
 *       and should mainly be for internal usage, only.
*/
int find_rorw_root(const char *path) {
	bool hidden = false;

	int i = 0;
	for (i = 0; i < uopt.nroots; i++) {
		char p[PATHLEN_MAX];
		snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

		struct stat stbuf;
		int res = lstat(p, &stbuf);

		if (res == 0 && !hidden) {
			return i;
		} else if (hidden) {
			// the file is hidden in this root, we *only* ignore it in roots below this level
			return -1;
		}
		// check check for a hide file, checking first here is the magic to hide files *below* this level
		hidden = path_hidden(path, i);
	}

	return -1;
}

/**
 * Find a writable root. If file does not exist, we check for 
 * the parent directory.
 **/
int find_rw_root_cutlast(const char *path) {
	int root = find_rw_root_cow(path);

	if (root < 0 && errno == ENOENT) {
		// So path does not exist, now again, but with dirname only
		char *dname = u_dirname(path);
		int root_rorw = find_rorw_root(dname);
		free(dname);

		// nothing found
		if (root_rorw < 0) return -1;

		// the returned root is writable, good!
		if (uopt.roots[root_rorw].rw) return root_rorw;
		
		// cow is disabled, return whatever was found
		if (!uopt.cow_enabled) return root_rorw;

		int root_rw = find_lowest_rw_root(root_rorw);

		dname = u_dirname(path);
		int res = path_create(dname, root_rorw, root_rw);
		free(dname);

		// creating the path failed
		if (res) return -1;

		return root_rw;
	}

	return root;
}

/**
 * copy-one-write
 * Find path in a union branch and if this branch is read-only, 
 * copy the file to a read-write branch.
 */
int find_rw_root_cow(const char *path) {
	int root_rorw = find_rorw_root(path);

	// not found anywhere
	if (root_rorw < 0) return -1;

	// the found root is writable, good!
	if (uopt.roots[root_rorw].rw) return root_rorw;

	// cow is disabled, return whatever was found
	if (!uopt.cow_enabled) return root_rorw;

	int root_rw = find_lowest_rw_root(root_rorw);
	if (root_rw < 0) {
		// no writable root found
		errno = EACCES;
		return -1;
	}

	if (cow_cp(path, root_rorw, root_rw)) return -1;

	// remove a file that might hide the copied file
	remove_hidden(path, root_rw);

	return root_rw;
}

/**
 * Find lowest possible writable root but only lower than root_ro.
 */
int find_lowest_rw_root(int root_ro) {
	int i = 0;
	for (i = 0; i < root_ro; i++) {
		if (uopt.roots[i].rw) return i; // found it it.
	}

	return -1;
}
