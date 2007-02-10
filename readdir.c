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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/statvfs.h>

#include "unionfs.h"
#include "opts.h"
#include "cache.h"
#include "stats.h"
#include "debug.h"


/**
 * unionfs readdir()
 */
int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, 
		    struct fuse_file_info *fi)
{
	(void)offset;
	(void)fi;

	DBG("readdir\n");

	int nadded = 0;
	char **added;
	added = malloc(1);

	int i = 0;
	for (i = 0; i < uopt.nroots; i++) {
		char p[PATHLEN_MAX];
		snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

		DIR *dp = opendir(p);
		if (dp == NULL) continue;

		struct dirent *de;
		while ((de = readdir(dp)) != NULL) {
			int j = 0;
			for (j = 0; j < nadded; j++) {
				if (strcmp(added[j], de->d_name) == 0) break;
			}
			if (j < nadded) continue;

			added = (char**)realloc(added, (nadded+1)*sizeof(char*));
			added[nadded] = malloc(PATHLEN_MAX);
			strncpy(added[nadded], de->d_name, PATHLEN_MAX);
			nadded++;

			struct stat st;
			memset(&st, 0, sizeof(st));
			st.st_ino = de->d_ino;
			st.st_mode = de->d_type << 12;
			if (filler(buf, de->d_name, &st, 0)) break;
		}

		closedir(dp);
	}

	for (i = 0; i < nadded; i++) free(added[i]);
	free(added);

	if (uopt.stats_enabled && strcmp(path, "/") == 0) {
		filler(buf, "stats", NULL, 0);
	}

	return 0;
}
