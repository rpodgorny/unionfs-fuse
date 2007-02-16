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
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdbool.h>
#include <syslog.h>

#include "unionfs.h"
#include "opts.h"


static uid_t daemon_uid = -1; // the uid the daemon is running as


/**
 * Check if a file with the hidden flag exists.
 */
bool file_hidden(const char *path) {
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
 * Remove a hide-file in all roots up to maxroot
 * If maxroot == -1, try to delete it in all roots.
 */
int remove_hidden(const char *path, int maxroot) {
	if (!uopt.cow_enabled) return 0;

	if (maxroot == -1) maxroot = uopt.nroots;

	int i;
	for (i = 0; i <= maxroot; i++) {
		char p[PATHLEN_MAX];
		snprintf(p, PATHLEN_MAX, "%s%s%s", uopt.roots[i].path, path, HIDETAG);

		struct stat buf;
		int res = lstat(p, &buf);
		if (res == -1) continue;

		switch (buf.st_mode & S_IFMT) {
			case S_IFDIR: rmdir(p); break;
			default: unlink(p); break;
		}
	}

	return 0;
}

/**
 * dirname() in libc might not be thread-save, at least the man page states
 * "may return pointers to statically allocated memory", so we need our own
 * implementation
 */
char *u_dirname(const char *path) {
	char *ret = strdup(path);

	char *ri = rindex(ret, '/'); //this char should always be found
	*ri = '\0';

	return ret;
}

/**
 * Create a file that hides path below root_rw
 */
int hide_file(const char *path, int root_rw) {
	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s%s", uopt.roots[root_rw].path, path, HIDETAG);

	int res = open(p, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	if (res == -1) return res;

	close(res);

	return 0;
}

/**
 * Set the euid of the user performing the fs operation.
 */
void to_user(void) {
	static bool first = true;

	if (first) daemon_uid = getuid();
	if (daemon_uid != 0) return;

	struct fuse_context *ctx = fuse_get_context();
	if (!ctx) return;

	if (setegid(ctx->gid)) syslog(LOG_WARNING, "setegid(%i) failed\n", ctx->gid);
	if (seteuid(ctx->uid)) syslog(LOG_WARNING, "seteuid(%i) failed\n", ctx->uid);
}

/**
 * Switch back to the root user.
 */
void to_root(void) {
	if (daemon_uid != 0) return;

	if (seteuid(0)) syslog(LOG_WARNING, "setegid(0) failed");
	if (setegid(0)) syslog(LOG_WARNING, "setegid(0) failed");
}
