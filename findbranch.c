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
int find_rorw_root(const char *path) {
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
			// the file is hidden in this root, we also ignore it in all roots below this level
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
int find_rw_root_cow(const char *path) {
	int root = find_rw_root_cow_cutlast(path);

	if (root < 0 && errno == ENOENT) {
		// So path does not exist, now again, but with dirname only
		char *dname = u_dirname(path);
		int root_ro = find_rorw_root(dname);
		free(dname);

		// nothing found
		if (root_ro < 0) return -1;

		// the returned root is writable, good!
		if (uopt.roots[root_ro].rw) return root_ro;
		
		// cow is disabled, return whatever was found
		if (!uopt.cow_enabled) return root_ro;

		int root_rw = find_lowest_rw_root(root_ro);

		dname = u_dirname(path);
		int res = path_create(dname, root_ro, root_rw);
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
int find_rw_root_cow_cutlast(const char *path) {
	int root_ro = find_rorw_root(path);

	// not found anywhere
	if (root_ro < 0) return -1;

	// the found root is writable, good!
	if (uopt.roots[root_ro].rw) return root_ro;

	// cow is disabled, return whatever was found
	if (!uopt.cow_enabled) return root_ro;

	int root_rw = find_lowest_rw_root(root_ro);
	if (root_rw < 0) {
		// no writable root found
		errno = EACCES;
		return -1;
	}

	// copy the file from root_ro to root_rw
	if (cow_cp(path, root_ro, root_rw)) {
		// some error
		return -1;
	}

	// remove a file that might hide the copied file
	remove_hidden(path, root_rw);

	if (uopt.cache_enabled) {
		// update the cache
		cache_invalidate_path(path);
		cache_save(path, root_rw);
	}

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
