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
#include <strings.h>
#include <unistd.h>
#include <stdbool.h>

#include "unionfs.h"
#include "opts.h"

/**
 * Check if a file with the hidden flag exists.
 */
bool file_hidden(const char *path)
{
	if (!uopt.cow_enabled) {
		// cow mode disabled, no need for hidden files
		return false;
	}
	
	char p[PATHLEN_MAX];
	struct stat stbuf;
	int res;

	snprintf(p, PATHLEN_MAX, "%s%s", path, HIDETAG);

	res = lstat(p, &stbuf);
	if (res == 0) return true;

	return false;
}

/**
 * Remove a hide-file in all roots up to maxroot
 * If maxroot == -1, try to delete it in all roots.
 */
int remove_hidden(const char *path, int maxroot)
{
	if (!uopt.cow_enabled) return 0;
	
	int i;
	char p[PATHLEN_MAX];
	struct stat buf;
	int res;

	if (maxroot == -1) maxroot = uopt.nroots;

	for (i = 0; i <= maxroot; i++) {
		snprintf(p, PATHLEN_MAX, "%s%s%s", uopt.roots[i].path, path, HIDETAG);

		res = lstat(p, &buf);
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
char* u_dirname(char *path) 
{
	char* ri = rindex(path, '/'); //this char should always be found
	int len = ri - path;

	path[len] = '\0';

	return path;
}

/**
 * Create a file that hides path below root_rw
 */
int hide_file(const char *path, int root_rw)
{
	char p[PATHLEN_MAX];
	int res;
	
	snprintf(p, PATHLEN_MAX, "%s%s%s", uopt.roots[root_rw].path, path, HIDETAG);

	res = open(p, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);

	if (res == -1) return res;

	close(res);

	return 0;
}
