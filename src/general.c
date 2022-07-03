/*
*  C Implementation: general
*
* Description: General functions, not directly related to file system operations
*
* original implementation by Radek Podgorny
*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*
*/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <pthread.h>

#include "unionfs.h"
#include "opts.h"
#include "string.h"
#include "cow.h"
#include "cow_utils.h"
#include "findbranch.h"
#include "general.h"
#include "debug.h"
#include "usyslog.h"

/**
 * Check if a file or directory with the hidden flag exists.
 */
static int filedir_hidden(const char *path) {
	// cow mode disabled, no need for hidden files
	if (!uopt.cow_enabled) RETURN(false);

	char p[PATHLEN_MAX];
	if (strlen(path) + strlen(HIDETAG) + 1 > PATHLEN_MAX) RETURN(-ENAMETOOLONG);
	snprintf(p, PATHLEN_MAX, "%s%s", path, HIDETAG);
	DBG("%s\n", p);

	struct stat stbuf;
	int res = lstat(p, &stbuf);
	if (res == 0) RETURN(1);

	RETURN(0);
}

/**
 * check if any dir or file within path is hidden
 */
int path_hidden(const char *path, int branch) {
	DBG("%s\n", path);

	if (!uopt.cow_enabled) RETURN(false);

	char whiteoutpath[PATHLEN_MAX];
	if (BUILD_PATH(whiteoutpath, uopt.branches[branch].path, METADIR, path)) RETURN(false);

	// -1 as we MUST not end on the next path element
	char *walk = whiteoutpath + uopt.branches[branch].path_len + strlen(METADIR) - 1;

	// first slashes, e.g. we have path = /dir1/dir2/, will set walk = dir1/dir2/
	while (*walk == '/') walk++;

	do {
		// walk over the directory name, walk will now be /dir2
		while (*walk != '\0' && *walk != '/') walk++;

		// +1 due to \0, which gets added automatically
		char p[PATHLEN_MAX];
		// walk - path = strlen(/dir1)
		snprintf(p, (walk - whiteoutpath) + 1, "%s", whiteoutpath);
		int res = filedir_hidden(p);
		if (res) RETURN(res); // path is hidden or error

		// as above the do loop, walk over the next slashes, walk = dir2/
		while (*walk == '/') walk++;
	} while (*walk != '\0');

	RETURN(0);
}

/**
 * Remove a hide-file in all branches up to maxbranch
 * If maxbranch == -1, try to delete it in all branches.
 */
int remove_hidden(const char *path, int maxbranch) {
	DBG("%s\n", path);

	if (!uopt.cow_enabled) RETURN(0);

	if (maxbranch == -1) maxbranch = uopt.nbranches;

	int i;
	for (i = 0; i <= maxbranch; i++) {
		char p[PATHLEN_MAX];
		if (BUILD_PATH(p, uopt.branches[i].path, METADIR, path)) RETURN(-ENAMETOOLONG);
		if (strlen(p) + strlen(HIDETAG) > PATHLEN_MAX) RETURN(-ENAMETOOLONG);
		strcat(p, HIDETAG); // TODO check length

		switch (path_is_dir(p)) {
			case IS_FILE: unlink(p); break;
			case IS_DIR: rmdir(p); break;
			case NOT_EXISTING: continue;
		}
	}

	RETURN(0);
}

/**
 * check if path is a directory
 *
 * return proper types given by filetype_t
 */
filetype_t path_is_dir(const char *path) {
	DBG("%s\n", path);

	struct stat buf;

	if (lstat(path, &buf) == -1) RETURN(NOT_EXISTING);

	if (S_ISDIR(buf.st_mode)) RETURN(IS_DIR);

	RETURN(IS_FILE);
}

/**
 * Create a file or directory that hides path below branch_rw
 */
static int do_create_whiteout(const char *path, int branch_rw, enum whiteout mode) {
	DBG("%s\n", path);

	char metapath[PATHLEN_MAX];

	if (BUILD_PATH(metapath, METADIR, path)) RETURN(-1);

	// p MUST be without path to branch prefix here! 2 x branch_rw is correct here!
	// this creates e.g. branch/.unionfs/some_directory
	path_create_cutlast_cow(metapath, branch_rw, branch_rw);

	char p[PATHLEN_MAX];
	if (BUILD_PATH(p, uopt.branches[branch_rw].path, metapath)) RETURN(-1);
	strcat(p, HIDETAG); // TODO check length

	int res;
	if (mode == WHITEOUT_FILE) {
		res = open(p, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
		if (res == -1) RETURN(-1);
		res = close(res);
	} else {
		res = mkdir(p, S_IRWXU);
		if (res)
			USYSLOG(LOG_ERR, "Creating %s failed: %s\n", p, strerror(errno));
	}

	RETURN(res);
}

/**
 * Create a file that hides path below branch_rw
 */
int hide_file(const char *path, int branch_rw) {
	DBG("%s\n", path);
	int res = do_create_whiteout(path, branch_rw, WHITEOUT_FILE);
	RETURN(res);
}

/**
 * Create a directory that hides path below branch_rw
 */
int hide_dir(const char *path, int branch_rw) {
	DBG("%s\n", path);
	int res = do_create_whiteout(path, branch_rw, WHITEOUT_DIR);
	RETURN(res);
}

/**
 * This is called *after* unlink() or rmdir(), create a whiteout file
 * if the same file/dir does exist in a lower branch
 */
int maybe_whiteout(const char *path, int branch_rw, enum whiteout mode) {
	DBG("%s\n", path);

	// we are not interested in the branch itself, only if it exists at all
	if (find_rorw_branch(path) != -1) {
		int res = do_create_whiteout(path, branch_rw, mode);
		RETURN(res);
	}

	RETURN(0);
}

/**
 * Set file owner of after an operation, which created a file.
 */
int set_owner(const char *path) {
	struct fuse_context *ctx = fuse_get_context();
	if (ctx->uid != 0 && ctx->gid != 0) {
		int res = lchown(path, ctx->uid, ctx->gid);
		if (res) {
			USYSLOG(LOG_WARNING,
			       ":%s: Setting the correct file owner failed: %s !\n",
			       __func__, strerror(errno));
			RETURN(-errno);
		}
	}
	RETURN(0);
}

/**
 * Actually create the directory here.
 */
static int do_create(const char *path, int nbranch_ro, int nbranch_rw) {
	DBG("%s\n", path);

	char dirp[PATHLEN_MAX]; // dir path to create
	sprintf(dirp, "%s%s", uopt.branches[nbranch_rw].path, path);

	struct stat buf;
	int res = stat(dirp, &buf);
	if (res != -1) RETURN(0); // already exists

	if (nbranch_ro == nbranch_rw) {
		// special case nbranch_ro = nbranch_rw, this is if we a create
		// unionfs meta directories, so not directly on cow operations
		buf.st_mode = S_IRWXU | S_IRWXG;
	} else {
		// data from the ro-branch
		char o_dirp[PATHLEN_MAX]; // the pathname we want to copy
		sprintf(o_dirp, "%s%s", uopt.branches[nbranch_ro].path, path);
		res = stat(o_dirp, &buf);
		if (res == -1) RETURN(1); // lower level branch removed in the mean time?
	}

	res = mkdir(dirp, buf.st_mode);
	if (res == -1) {
		if (errno == EEXIST) {
			// In an NFS environment with many clients trying to write to the same directory tree
			// and if that tree does not exist on the read write mount, there's a race between them.
			// The directory may have been created by another client. It's not a fatal error.
			USYSLOG(LOG_INFO, "Directory %s already existed - probably another client made it if using NFS", dirp);
		} else {
			USYSLOG(LOG_ERR, "Creating %s failed: \n", dirp);
			RETURN(1);
		}
	}

	if (nbranch_ro == nbranch_rw) RETURN(0); // the special case again

	if (setfile(dirp, &buf)) RETURN(1); // directory already removed by another process?

	// TODO: time, but its values are modified by the next dir/file creation steps?

	RETURN(0);
}

/**
 * create the dir path on nbranch_rw matching same path on nbranch_ro
 */
int path_create(const char *path, int nbranch_ro, int nbranch_rw) {
	DBG("%s\n", path);

	char p[PATHLEN_MAX];
	if (BUILD_PATH(p, uopt.branches[nbranch_rw].path, path)) RETURN(-ENAMETOOLONG);

	struct stat st;
	if (!stat(p, &st)) {
		// path does already exists, no need to create it
		RETURN(0);
	}

	char *walk = (char *)path;

	// first slashes, e.g. we have path = /dir1/dir2/, will set walk = dir1/dir2/
	while (*walk == '/') walk++;

	do {
		// walk over the directory name, walk will now be /dir2
		while (*walk != '\0' && *walk != '/') walk++;

		// +1 due to \0, which gets added automatically
		snprintf(p, (walk - path) + 1, "%s", path); // walk - path = strlen(/dir1)
		int res = do_create(p, nbranch_ro, nbranch_rw);
		if (res) RETURN(res); // creating the directory failed

		// as above the do loop, walk over the next slashes, walk = dir2/
		while (*walk == '/') walk++;
	} while (*walk != '\0');

	RETURN(0);
}

/**
 * Same as  path_create(), but ignore the last segment in path,
 * i.e. it might be a filename.
 */
int path_create_cutlast(const char *path, int nbranch_ro, int nbranch_rw) {
	DBG("%s\n", path);

	char *dname = u_dirname(path);
	if (dname == NULL)
		RETURN(-ENOMEM);
	int ret = path_create(dname, nbranch_ro, nbranch_rw);
	free(dname);

	RETURN(ret);
}
