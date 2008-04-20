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

#include "unionfs.h"
#include "opts.h"
#include "debug.h"
#include "cow.h"
#include "general.h"
#include "findbranch.h"

enum {
	EMPTY = 0,
	HAS_WHITEOUTS
};


/**
  * path might have whiteout files which can't be deleted from
  * outside of unionfs, test for these files
  */
static int test_for_whiteouts(const char *path) {
	DIR *dp = opendir(path);
	if (dp == NULL) return errno;
	int res = EMPTY;

	struct dirent *de;
	while ((de = readdir(dp)) != NULL) {
		if (strncmp (de->d_name, ".", 1) == 0
		||  strncmp (de->d_name, "..", strlen(de->d_name)) == 0)
			continue;

		if (!whiteout_tag(de->d_name)) {
			closedir (dp);
			return ENOTEMPTY;
		} else {
			res = HAS_WHITEOUTS;
			continue;
		}
	}
	return res;
}

/**
  * recursively delete a directory
  */
static int recursive_rmdir(const char *path)
{
	int res;
	char fname[PATHLEN_MAX];

	DIR *dp = opendir(path);
	if (dp == NULL) return errno;

	struct dirent *de;
	while ((de = readdir(dp)) != NULL) {
		struct stat st;

		if (strncmp (de->d_name, ".", 1) == 0
		||  strncmp (de->d_name, "..", strlen(de->d_name)) == 0)
			continue;

		if (string_too_long(2, path, de->d_name)) return ENAMETOOLONG;
		sprintf (fname, "%s/%s", path, de->d_name);

		if (lstat(fname, &st) == 0) {
			if (S_ISDIR(st.st_mode)) {
				res = recursive_rmdir(fname);
				if (res) {
					errno = res;
					goto err_out;
				}
			} else {
				// file, delete it
				res = unlink(fname);
				if (res == -1) goto err_out;
			}
		} else goto err_out;
	}
	res = rmdir (path);
	if (res) goto err_out;

	closedir (dp);
	return 0;

err_out:
	closedir (dp);
	return errno;
}

/**
  * If the root that has the directory to be removed is in read-write mode,
  * we can really delete the file.
  */
static int rmdir_rw(const char *path, int root_rw) {
	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[root_rw].path, path);

	int res = test_for_whiteouts(p);

	if (res < 0)
		return res; // directory probably not empty
	else if (res == EMPTY) {
		// directory is empty, we can delete it
		res = rmdir(p);
		if (res == -1) return errno;
	} else {
		// directory has whiteout files
		res = recursive_rmdir(p);
		if (res) return res;

		// since there had been whiteout files within path, we really should hide lower branches
		hide_dir (path, root_rw);
	}

	return 0;
}

/**
  * If the root that has the directory to be removed is in read-only mode,
  * we create a file with a HIDE tag in an upper level root.
  * To other fuse functions this tag means, not to expose the 
  * lower level directory.
  */
static int rmdir_ro(const char *path, int root_ro) {
	int i = -1;

	// find a writable root above root_ro
	int root_rw = find_lowest_rw_root(root_ro);

	if (root_rw >= 0) i = path_create_cutlast(path, root_ro, root_rw);

	// no writable path, or some other error
	if (i < 0) return EACCES;

	if (hide_dir(path, root_rw) == -1) {
		// creating the file with the hide tag failed
		// TODO: open() error messages are not optimal on rmdir()
		return errno;
	}

	return 0;
}

/**
  * rmdir() call
  */
int unionfs_rmdir(const char *path) {
	DBG("rmdir\n");
	
	to_user();

	int i = find_rorw_root(path);
	if (i == -1) {
		to_root();
		return -errno;
	}

	int res;
	// root is read-only and cow is enabled
	if (!uopt.roots[i].rw && uopt.cow_enabled) {
		res = rmdir_ro(path, i);
		to_root();
		return -res;
	}
	
	res = rmdir_rw(path, i);
	to_root();
	if (res == 0)
		unionfs_rmdir(path); /* make _real_ sure the directory is removed */
	return -res;
}
