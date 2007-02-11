/*
* Description: copy-on-write functions. Create a path in a RW-root,
*              that exists in in a lower level RO-root
*
*
* Author: Bernd Schubert <bernd-schubert@gmx.de>, (C) 2007
*
* Copyright: BSD style license
*
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <utime.h>
#include <syslog.h>
#include <string.h>

#include "unionfs.h"
#include "opts.h"
#include "debug.h"
#include "cache.h"
#include "general.h"

/**
  * Actually create the directory here.
  */
static int do_create(const char *path, int nroot_ro, int nroot_rw)
{
	char dirp[PATHLEN_MAX]; // dir path to create
	sprintf(dirp, "%s%s", uopt.roots[nroot_rw].path, path);

	struct stat buf;
	int res = stat(dirp, &buf);
	if (res != -1) return 0; // already exists

	// root does not exist yet, create it with stat data from the root
	char o_dirp[PATHLEN_MAX]; // the pathname we want to copy

	sprintf(o_dirp, "%s%s", uopt.roots[nroot_ro].path, path);
	res = stat(o_dirp, &buf);
	if (res == -1) return 1; // lower level root removed in the mean time?

	res = mkdir(dirp, buf.st_mode);
	if (res == -1) {
		syslog (LOG_DAEMON, "Creating %s failed: \n", dirp);
		return 1;
	}
	
	res = chown(dirp, buf.st_uid, buf.st_gid);
	if (res == -1) return 1; // directory already removed by another process?

	// TODO: time, but its values are modified by the next dir/file creation steps?

	return 0;
	
}

/**
  * l_nroot (lower nroot than nroot) is write protected, create the dir path on
  * nroot for an other COW operation.
  */
int path_create(const char *path, int nroot_ro, int nroot_rw) {
	if (!uopt.cow_enabled) return 0;

	char p[PATHLEN_MAX]; // temp string, with elements of path
	int res;
	struct stat buf;

	if (strlen(path) + strlen(uopt.roots[nroot_rw].path) > PATHLEN_MAX
	|| strlen(path) + strlen(uopt.roots[nroot_ro].path) > PATHLEN_MAX) {
		// TODO: how to handle that?
		return 1;
	}

	snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[nroot_rw].path, path);
	if (!stat(p, &buf)) {
		// path does already exists, no need to create it
		return 0;
	}

	char *walk = (char *)path;

	// first slashes
	while (*walk != '\0' && *walk == '/') walk++;

	do {
		while (*walk != '\0' && *walk != '/') walk++;
	
		// +1 due to \0, which gets added automatically
		snprintf(p, (walk - path) + 1, path);
		res = do_create(p, nroot_ro, nroot_rw);
		if (res) return res; // creating the direcory failed

		// slashes between path names
		while (*walk != '\0' && *walk == '/') walk++;

	} while (*walk != '\0');

	return 0;
}

/**
 * As path_create(), but ignore the last segment in path,
 * i.e. in the calling function it might be a filename.
 **/
int path_create_cutlast(const char *path, int nroot_ro, int nroot_rw) {
	char *dname = u_dirname(path);
	int ret = path_create(u_dirname(path), nroot_ro, nroot_rw);
	free(dname);

	return ret;
}
