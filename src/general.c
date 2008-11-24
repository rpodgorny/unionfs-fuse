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
#include "findbranch.h"
#include "general.h"
#include "debug.h"


/**
 * Check if a file or directory with the hidden flag exists.
 */
static bool filedir_hidden(const char *path) {
	DBG_IN();

	// cow mode disabled, no need for hidden files
	if (!uopt.cow_enabled) return false;
	
	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", path, HIDETAG);

	struct stat stbuf;
	int res = lstat(p, &stbuf);
	if (res == 0) return true;

	return false;
}

/**
 * check if any dir or file within path is hidden
 */
bool path_hidden(const char *path, int branch) {
	DBG_IN();

	if (!uopt.cow_enabled) return false;

	char whiteoutpath[PATHLEN_MAX];
	if (BUILD_PATH(whiteoutpath, uopt.branches[branch].path, METADIR, path))
		return false;

	char *walk = whiteoutpath;

	// first slashes, e.g. we have path = /dir1/dir2/, will set walk = dir1/dir2/
	while (*walk != '\0' && *walk == '/') walk++;

	bool first = true;
	do {
		// walk over the directory name, walk will now be /dir2
		while (*walk != '\0' && *walk != '/') walk++;
	
		if (first) {
			// first dir in path is our branch, no need to check if it is hidden
			first = false;
			continue;
		}
		// +1 due to \0, which gets added automatically
		char p[PATHLEN_MAX];
		snprintf(p, (walk - whiteoutpath) + 1, "%s", whiteoutpath); // walk - path = strlen(/dir1)
		bool res = filedir_hidden(p);
		if (res) return res; // path is hidden

		// as above the do loop, walk over the next slashes, walk = dir2/
		while (*walk != '\0' && *walk == '/') walk++;
	} while (*walk != '\0');

	return 0;
}

/**
 * Remove a hide-file in all branches up to maxbranch
 * If maxbranch == -1, try to delete it in all branches.
 */
int remove_hidden(const char *path, int maxbranch) {
	DBG_IN();

	if (!uopt.cow_enabled) return 0;

	if (maxbranch == -1) maxbranch = uopt.nbranches;

	int i;
	for (i = 0; i <= maxbranch; i++) {
		char p[PATHLEN_MAX];
		if (BUILD_PATH(p, uopt.branches[i].path, METADIR, path, HIDETAG))
			return 1;

		switch (path_is_dir(p)) {
			case IS_FILE: unlink (p); break;
			case IS_DIR:  rmdir  (p); break;
			case NOT_EXISTING: continue;
		}
	}

	return 0;
}

/**
 * check if path is a directory
 *
 * return 1 if it is a directory, 0 if it is a file and -1 if it does not exist
 */
filetype_t path_is_dir(const char *path) {
	DBG_IN();

	struct stat buf;
	
	if (lstat(path, &buf) == -1) return -1;
	
	if (S_ISDIR(buf.st_mode)) return 1;
	
	return 0;
}

/**
 * Create a file or directory that hides path below branch_rw
 */
static int do_create_whiteout(const char *path, int branch_rw, enum whiteout mode) {
	DBG_IN();

	char metapath[PATHLEN_MAX];

	if (BUILD_PATH(metapath, METADIR, path))  return -1;

	// p MUST be without path to branch prefix here! 2 x branch_rw is correct here!
	// this creates e.g. branch/.unionfs/some_directory
	path_create_cutlast(metapath, branch_rw, branch_rw);

	char p[PATHLEN_MAX];
	if (BUILD_PATH(p, uopt.branches[branch_rw].path, metapath, HIDETAG))
		return -1;

	int res;
	if (mode == WHITEOUT_FILE) {
		res = open(p, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
		if (res == -1) return -1;
		res = close(res);
	} else {
		res = mkdir(p, S_IRWXU);
	}

	return res;
}

/**
 * Create a file that hides path below branch_rw
 */
int hide_file(const char *path, int branch_rw) {
	DBG_IN();
	return do_create_whiteout(path, branch_rw, WHITEOUT_FILE);
}

/**
 * Create a directory that hides path below branch_rw
 */
int hide_dir(const char *path, int branch_rw) {
	DBG_IN();
	return do_create_whiteout(path, branch_rw, WHITEOUT_DIR);
}

/**
 * This is called *after* unlink() or rmdir(), create a whiteout file
 * if the same file/dir does exist in a lower branch
 */
int maybe_whiteout(const char *path, int branch_rw, enum whiteout mode) {
	DBG_IN();

	// we are not interested in the branch itself, only if it exists at all
	if (find_rorw_branch(path) != -1) {
		return do_create_whiteout(path, branch_rw, mode);
	}

	return 0;
}

/**
 * Set file owner of after an operation, which created a file.
 */
int set_owner(const char *path) {
	struct fuse_context *ctx = fuse_get_context();
	if (ctx->uid != 0 && ctx->gid != 0) {
		int res = lchown(path, ctx->uid, ctx->gid);
		if (res) {
			usyslog(LOG_WARNING,
			       ":%s: Setting the correct file owner failed: %s !\n", 
			       __func__, strerror(errno));
			return -errno;
		}
	}
	return 0;
}

/**
 * Initialize statvfs() branches
 */
static void statvfs_branches(void)
{
	int nbranches;

	// for counting the free space, we should only consider rw-branches
	if (ufeatures.rw_branches.n_rw > 0) {
		nbranches = ufeatures.rw_branches.n_rw;
	} else {
		 // no rw-branch, so free space not important, take everything then
		nbranches = uopt.nbranches;
	}

	int i;
	dev_t devno[uopt.nbranches];
	int *stv = NULL;	// array of all branches we will do a statvfs on
	int n_stv = 0;		// number of statvfs branches
	for (i = 0; i < nbranches; i++) {
		struct stat st;
		branch_entry_t *branch;
		int br_num; // branch number in the global uopt branches structure

		if (ufeatures.rw_branches.n_rw > 0)
			br_num = ufeatures.rw_branches.rw_br[i];
		else
			br_num = i;

		branch = &uopt.branches[br_num];

		int res = stat(branch->path, &st);
		if (res == -1) continue;
		devno[i] = st.st_dev;

		// Eliminate same devices
		int j = 0;
		for (j = 0; j < i; j ++) {
			if (st.st_dev == devno[j]) break;
		}

		if (j == i) {
			n_stv++;
			stv = realloc(stv, sizeof(*stv) * n_stv);
			if (stv == NULL) {
				fprintf(stderr, "%s:%d: Realloc failed\n", 
					__func__, __LINE__);
				// still very early stage when we are here, 
				// so we can abort
				exit (1);
			}
			stv[n_stv-1] = i;
		}
	}

	if (n_stv < 1) {
		fprintf(stderr, "%s:%d: Error, no statfs branches!\n",
			__func__, __LINE__);
		// still very early stage when we are here, so we can abort
		exit (1);
	}

	ufeatures.statvfs.nbranches = n_stv;
	ufeatures.statvfs.branches  = stv;
}

/**
  * Initialize our features structure, thus, fill in all the information.
  * This is to be called as late as possible, but before any access to the
  * filesystem. So either before fuse_main or from an extra fuse .init method.
  */
int initialize_features(void)
{
	// the branch information
	int i = 0;
	int n_rw = 0; //number of rw_branches
	unsigned *rw_br = NULL; // array of rw_branch numbers

	for (i = 0; i < uopt.nbranches; i++) {
		if (uopt.branches[i].rw) {
			n_rw++;
			rw_br = realloc(rw_br, sizeof(*rw_br) * n_rw);
			rw_br[n_rw-1] = i;
		}
	}

	ufeatures.rw_branches.n_rw  = n_rw;
	ufeatures.rw_branches.rw_br = rw_br;

	statvfs_branches();
	
	return 0;
}
