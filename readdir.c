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
#include <stdbool.h>

#include "unionfs.h"
#include "opts.h"
#include "stats.h"
#include "debug.h"
#include "hashtable.h"
#include "hash.h"
#include "general.h"

/**
 * Check if fname has a hiding tag and return its status.
 * Also, add this file and to the hiding hash table.
 * Warning: If fname has the tag, fname gets modified.
 */
static bool is_hiding(struct hashtable *hides, char *fname) {
	char *tag;
	
	tag = whiteout_tag(fname);
	if (tag) {
		// even more important, ignore the file without the tag!
		// hint: tag is a pointer to the flag-suffix within de->d_name
		*tag = '\0'; // this modifies fname!

		// add to hides (only if not there already)
		if (!hashtable_search(hides, fname)) {
			hashtable_insert(hides, strdup(fname), malloc(1));
		}

		
		return true;
	}
	return false;
}

/**
 * unionfs-fuse readdir function
 */
int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	(void)offset;
	(void)fi;
	int i = 0;

	DBG("readdir\n");
	
	to_user();

	// we will store already added files here to handle same file names across different roots
	struct hashtable *files = create_hashtable(16, string_hash, string_equal);
	
	struct hashtable *hides;
	
	if (uopt.cow_enabled) hides = create_hashtable(16, string_hash, string_equal);

	for (i = 0; i < uopt.nroots; i++) {
		char p[PATHLEN_MAX];
		snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

		DIR *dp = opendir(p);
		if (dp == NULL) continue;

		struct dirent *de;
		while ((de = readdir(dp)) != NULL) {
			// already added in some other root
			if (hashtable_search(files, de->d_name) != NULL) continue;

			// check if we need file hiding
			if (uopt.cow_enabled) {
				// file should be hidden from the user
				if (hashtable_search(hides, de->d_name) != NULL) continue;
	
				// file itself has the hiding tag
				if (is_hiding(hides, de->d_name)) continue;
			}

			// fill with something dummy, we're interested in key existence only
			hashtable_insert(files, strdup(de->d_name), malloc(1));

			struct stat st;
			memset(&st, 0, sizeof(st));
			st.st_ino = de->d_ino;
			st.st_mode = de->d_type << 12;
			
			if (filler(buf, de->d_name, &st, 0)) break;
		}

		closedir(dp);
		
		// check if branches below this branch are hidden
		if (path_hidden(p)) break;
	}

	hashtable_destroy(files, 1);
	
	if (uopt.cow_enabled) hashtable_destroy(hides, 1);
	
	if (uopt.stats_enabled && strcmp(path, "/") == 0) {
		filler(buf, "stats", NULL, 0);
	}

	to_root();
	return 0;
}
