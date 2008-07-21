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
#include <stdbool.h>
#include <syslog.h>
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


static uid_t daemon_uid = -1; // the uid the daemon is running as
static pthread_mutex_t mutex; // the to_user() and to_root() locking mutex


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
	if (BUILD_PATH(whiteoutpath, uopt.branches[branch].path, METADIR, path)) {
		syslog (LOG_WARNING, "%s(): Path too long\n", __func__);
		return false;
	}

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
		if (BUILD_PATH(p, uopt.branches[i].path, METADIR, path, HIDETAG)) {
			syslog(LOG_WARNING, "%s: Path too long\n", __func__);
			return 1;
		}

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
	int res = -1;

	to_root(); // whiteouts are root business

	if (BUILD_PATH(metapath, METADIR, path)) {
		syslog (LOG_WARNING, "%s(): Path too long\n", __func__);
		goto out;
	}

	// p MUST be without path to branch prefix here! 2 x branch_rw is correct here!
	// this creates e.g. branch/.unionfs/some_directory
	path_create_cutlast(metapath, branch_rw, branch_rw);

	char p[PATHLEN_MAX];
	if (BUILD_PATH(p, uopt.branches[branch_rw].path, metapath, HIDETAG)) {
		syslog (LOG_WARNING, "%s(): Path too long\n", __func__);
		goto out;
	}

	if (mode == WHITEOUT_FILE) {
		res = open(p, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
		if (res == -1) goto out;
		res = close(res);
	} else {
		res = mkdir(p, S_IRWXU);
	}

out:
	to_user();
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

static void initgroups_uid(uid_t uid) {
	DBG_IN();

	struct passwd pwd;
	struct passwd *ppwd;
	char buf[BUFSIZ];

	if (!uopt.initgroups) return;

	getpwuid_r(uid, &pwd, buf, sizeof(buf), &ppwd);
	if (ppwd) initgroups(ppwd->pw_name, ppwd->pw_gid);
}

/**
 * Set the euid of the user performing the fs operation.
 */
void to_user(void) {
	DBG_IN();

	static bool first = true;
	int errno_orig = errno;

	if (first) {
		daemon_uid = getuid();
		pthread_mutex_init(&mutex, NULL);
		first = false;
	}

	if (daemon_uid != 0) return;

	struct fuse_context *ctx = fuse_get_context();
	if (!ctx) return;

	// disabled, since we temporarily enforce single threading
	//pthread_mutex_lock(&mutex);

	initgroups_uid(ctx->uid);

	if (ctx->gid != 0) {
		if (setegid(ctx->gid)) syslog(LOG_WARNING, "setegid(%i) failed\n", ctx->gid);
	}
	if (ctx->uid != 0) {
		if (seteuid(ctx->uid)) syslog(LOG_WARNING, "seteuid(%i) failed\n", ctx->uid);
	}

	errno = errno_orig;
}

/**
 * Switch back to the root user.
 */
void to_root(void) {
	DBG_IN();

	int errno_orig = errno;

	if (daemon_uid != 0) return;

	struct fuse_context *ctx = fuse_get_context();
	if (!ctx) return;

	if (ctx->uid != 0) {
		if (seteuid(0)) syslog(LOG_WARNING, "seteuid(0) failed");
	}
	if (ctx->gid != 0) {
		if (setegid(0)) syslog(LOG_WARNING, "setegid(0) failed");
	}

	initgroups_uid(0);

	// disabled, since we temporarily enforce single threading
	//pthread_mutex_unlock(&mutex);

	errno = errno_orig;
}
