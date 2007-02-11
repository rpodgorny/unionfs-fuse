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
#include "hashtable.h"
#include "hashtable_itr.h"
#include "hash.h"

/**
 * Check if the given fname suffixes the hide tag
 */
static char *hide_tag(const char *fname) {
	char *tag = strstr(fname, HIDETAG);

	// check if fname has tag, fname is not only the tag, file name ends with the tag
	// TODO: static strlen(HIDETAG)
	if (tag && tag != fname && strlen(tag) == strlen(HIDETAG)) {
		return tag;
	}

	return NULL;
}

/**
 * unionfs-fuse readdir function
 *
 * We first read the directory contents for all roots and filter it on the run (for hidden files and stuff). This could be connected into a single loop even with the hiding but then the hiding file has to be on a root prior to the one that contains the file we are to hide. This two-round soulution gives an oportunity to hide anything from anywhere...
 */
int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	(void)offset;
	(void)fi;
	int i = 0;

	DBG("readdir\n");

	// we will store already added files here to handle same file names across different roots
	struct hashtable *files = create_hashtable(16, string_hash, string_equal);
	struct hashtable *hides = create_hashtable(16, string_hash, string_equal);

	for (i = 0; i < uopt.nroots; i++) {
		char p[PATHLEN_MAX];
		snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

		DIR *dp = opendir(p);
		if (dp == NULL) continue;

		struct dirent *de;
		while ((de = readdir(dp)) != NULL) {
			char *tag = hide_tag(de->d_name);
			if (tag) {
				// a file indicating we should hide something
				*tag = '\0';

				// add to hides (only if not there already)
				if (!hashtable_search(hides, de->d_name)) {
					hashtable_insert(hides, strdup(de->d_name), malloc(1));
				}

				// try to remove from files to be displayed
				struct stat *st = hashtable_remove(files, de->d_name);
				free(st);

				continue;
			}

			// already added in some other root
			if (hashtable_search(files, de->d_name) != NULL) continue;

			// file should be hidden from the user
			if (hashtable_search(hides, de->d_name) != NULL) continue;

			struct stat *st = malloc(sizeof(struct stat));
			memset(st, 0, sizeof(struct stat));
			st->st_ino = de->d_ino;
			st->st_mode = de->d_type << 12;

			hashtable_insert(files, strdup(de->d_name), st);
		}

		closedir(dp);
	}

	// now really return filtered the entries
	struct hashtable_itr *itr = hashtable_iterator(files);
	do {
		// The hashtable routines are somewhat broken so we need to handle this ourselves
		if (itr->e == NULL) break;

		char *fname = hashtable_iterator_key(itr);
		struct stat *st = hashtable_iterator_value(itr);

		if (filler(buf, fname, st, 0)) break;
	} while (hashtable_iterator_advance(itr) != 0);
	free(itr);

	hashtable_destroy(files, 1);
	hashtable_destroy(hides, 1);

	if (uopt.stats_enabled && strcmp(path, "/") == 0) {
		filler(buf, "stats", NULL, 0);
	}

	return 0;
}
