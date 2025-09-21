/*
*  C Implementation: cow
*
* Copy-on-write functions
*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*/

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h>

#include "opts.h"
#include "findbranch.h"
#include "general.h"
#include "cow.h"
#include "cow_utils.h"
#include "string.h"
#include "debug.h"
#include "usyslog.h"


/**
 * l_nbranch (lower nbranch than nbranch) is write protected, create the dir path on
 * nbranch for an other COW operation.
 */
int path_create_cow(const char *path, int nbranch_ro, int nbranch_rw) {
	DBG("%s\n", path);

	if (!uopt.cow_enabled) RETURN(0);

	return path_create(path, nbranch_ro, nbranch_rw);
}

/**
 * Same as  path_create_cow(), but ignore the last segment in path,
 * i.e. it might be a filename.
 */
int path_create_cutlast_cow(const char *path, int nbranch_ro, int nbranch_rw) {
	DBG("%s\n", path);

	char *dname = u_dirname(path);
	if (dname == NULL) RETURN(-ENOMEM);
	int ret = path_create_cow(dname, nbranch_ro, nbranch_rw);
	free(dname);

	RETURN(ret);
}

/**
 * initiate the cow-copy action
 */
int cow_cp(const char *path, int branch_ro, int branch_rw, bool recursive) {
	DBG("%s\n", path);

	// create the path to the file
	int res = path_create_cutlast_cow(path, branch_ro, branch_rw);
	if (res != 0) RETURN(res);

	char from[PATHLEN_MAX], to[PATHLEN_MAX];
	if (BUILD_PATH(from, uopt.branches[branch_ro].path, path)) {
		RETURN(-ENAMETOOLONG);
	}
	if (BUILD_PATH(to, uopt.branches[branch_rw].path, path)) {
		RETURN(-ENAMETOOLONG);
	}

	setlocale(LC_ALL, "");

	struct cow cow;

	cow.uid = getuid();

	// Copy the umask for explicit mode setting.
	cow.umask = umask(0);
	umask(cow.umask);

	cow.from_path = from;
	cow.to_path = to;

	struct stat buf;
	lstat(cow.from_path, &buf);
	cow.stat = &buf;

	switch (buf.st_mode & S_IFMT) {
		case S_IFLNK:
			res = copy_link(&cow);
			break;
		case S_IFDIR:
			if (recursive) {
				res = copy_directory(path, branch_ro, branch_rw);
			} else {
				res = path_create_cow(path, branch_ro, branch_rw);
			}
			break;
		case S_IFBLK:
		case S_IFCHR:
			res = copy_special(&cow);
			break;
		case S_IFIFO:
			res = copy_fifo(&cow);
			break;
		case S_IFSOCK:
			USYSLOG(LOG_WARNING, "COW of sockets not supported: %s\n", cow.from_path);
			RETURN(1);
		default:
			if (uopt.all_writable)
				buf.st_mode |= (buf.st_mode & 0444) >> 1;
			res = copy_file(&cow);
	}

	RETURN(res);
}

/**
 * copy a directory between branches (includes all contents of the directory)
 */
int copy_directory(const char *path, int branch_ro, int branch_rw) {
	DBG("%s\n", path);

	/* create the directory on the destination branch */
	int res = path_create_cow(path, branch_ro, branch_rw);
	if (res != 0) {
		RETURN(res);
	}

	/* determine path to source directory on read-only branch */
	char from[PATHLEN_MAX];
	if (BUILD_PATH(from, uopt.branches[branch_ro].path, path)) RETURN(1);

	DIR *dp = opendir(from);
	if (dp == NULL) RETURN(1);

	struct dirent *de;
	while ((de = readdir(dp)) != NULL) {
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;

		char member[PATHLEN_MAX];
		if (BUILD_PATH(member, path, "/", de->d_name)) {
			res = 1;
			break;
		}

		// Generally if the target file already exists, we should not copy
		// anything. Directories are a special case as their contents may still
		// need to be merged.
		bool is_dir = false;
		if (branch_contains_path(branch_rw, member, &is_dir) &&
			(!is_dir || (branch_contains_path(branch_ro, member, &is_dir) && !is_dir))) {
			// File already exists in target and either source or target is not
			// a directory, skip it
			DBG("file %s copy skipped, exists in target\n", member);
			continue;
		}

		// If source file is hidden by a higher branch, we should not copy
		// anything. We do this by iterating through all higher branches and
		// checking if they have a whiteout. This is somewhat inefficient, but
		// it is the only simple way to handle this case. A more complex
		// solution would be to collect whiteouts to a hashtable, as has been
		// done in readdir.
		bool skip = false;
		for (int i = 0; i < branch_ro; i++) {
			// Assuming that only rw branches can have whiteouts
			if (uopt.branches[i].rw) {
				int hidden = path_hidden(member, i);
				if (hidden > 0) {
					// File was hidden, skip it
					DBG("file %s copy skipped, hidden by layer %i\n", member, i);
					skip = true;
					break;
				} else if (hidden < 0) {
					// error in path_hidden
					res = hidden;
					break;
				}
			}
		}
		if (res != 0) break;
		if (skip) continue;

		res = cow_cp(member, branch_ro, branch_rw, true);
		if (res != 0) break;
	}

	closedir(dp);
	RETURN(res);
}

