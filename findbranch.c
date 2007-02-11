/*
* Description: find a file in a branch
*
* License: BSD-style license
*
*
* Author: Radek Podgorny <radek@podgorny.cz>
*
*
*/

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

#include "unionfs.h"
#include "cache.h"
#include "opts.h"
#include "general.h"
#include "cow.h"
#include "findbranch.h"

/**
 * If path exists, return the root number that has path. Also create a cache entry.
 * TODO: We can still stat() fname_~HIDDEN, though these are hidden by readdir()
 *       and should mainly be for internal usage, only.
*/
int findroot(const char *path) {
	int i = -1;
	
	if (uopt.cache_enabled) cache_lookup(path);

	if (i != -1) return i;

	// create a new cache entry, if path exists
	bool hidden = false;
	for (i = 0; i < uopt.nroots; i++) {
		char p[PATHLEN_MAX];
		snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

		struct stat stbuf;
		int res = lstat(p, &stbuf);

		if (res == 0 && !hidden) {
			if (uopt.cache_enabled) cache_save(path, i);
			return i;
		} else if (hidden) {
			/* the file is hidden in this root, we also ignore it
			* in all roots below this level */
			return -1;
		}
		// check check for a hide file
		hidden = file_hidden(p);
	}

	return -1;
}


/**
 * Find a writable root. If file does not existent, we check for 
 * the parent directory.
 **/
int find_rw_root_with_cow(const char *path) {
	int root;

	root = cow(path); // copy-on-write

	if ((root < 0) && (errno == ENOENT)) {
		// So path does not exist, now again, but with dirname only
		char *dname = u_dirname(path);

		int root_ro = findroot(dname);

		if ((root_ro < 0) || uopt.roots[root_ro].rw || !uopt.cow_enabled) {
			// root does not exist or is already writable or cow disabled
			free(dname);
			return root_ro;
		}

		int root_rw = wroot_from_list(root_ro);
		int res = path_create(dname, root_ro, root_rw);

		free(dname);

		if (res) {
			// creating the path failed
			return -1;
		}

		return root_rw;
	}

	return root;
}

/**
 * Try to find root when we cut the last path element
 */
int findroot_cutlast(const char *path) {
	char* ri = rindex(path, '/'); // this char should always be found
	int len = ri - path;

	char p[PATHLEN_MAX];
	strncpy(p, path, len);
	p[len] = '\0';

	return findroot(p);
}

/**
 * Get a writable root in from our branch list, but only above root_ro.
 */
int wroot_from_list(int root_ro)
{
	int i;
	
	// take the first read-write root available
	for (i = 0; i < root_ro; i++)
		if (uopt.roots[i].rw) 
			return i; // found it it.

	return -1;
}
