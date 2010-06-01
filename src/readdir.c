/*
* Description: find a file in a branch
*
* License: BSD-style license
*
* original implementation by Radek Podgorny
*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*
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
#include "general.h"
#include "string.h"


/**
  * Hide metadata. As is causes a slight slowndown this is optional
  * 
  */
static bool hide_meta_files(int branch, const char *path, struct dirent *de)
{

	if (uopt.hide_meta_files == false) return false;

	fprintf(stderr, "uopt.branches[branch].path = %s path = %s\n", uopt.branches[branch].path, path);
	fprintf(stderr, "METANAME = %s, de->d_name = %s\n", METANAME, de->d_name);

	// TODO Would it be faster to add hash comparison?

	// HIDE out .unionfs directory
	if (strcmp(uopt.branches[branch].path, path) == 0
	&&  strcmp(METANAME, de->d_name) == 0) {
		return true;
	}

	// HIDE fuse META files
	if  (strncmp(FUSE_META_FILE, de->d_name, FUSE_META_LENGTH) == 0) 
		return true;

	return false;
}

/**
 * Check if fname has a hiding tag and return its status.
 * Also, add this file and to the hiding hash table.
 * Warning: If fname has the tag, fname gets modified.
 */
static bool is_hiding(struct hashtable *hides, char *fname) {
	DBG_IN();

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
 * Read whiteout files
 */
static void read_whiteouts(const char *path, struct hashtable *whiteouts, int branch) {
	DBG_IN();

	char p[PATHLEN_MAX];
	if (BUILD_PATH(p, uopt.branches[branch].path, METADIR, path)) return;

	DIR *dp = opendir(p);
	if (dp == NULL) return;

	struct dirent *de;
	while ((de = readdir(dp)) != NULL) {
		is_hiding(whiteouts, de->d_name);
	}

	closedir(dp);
}

/**
 * unionfs-fuse readdir function
 */
int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	DBG_IN();

	(void)offset;
	(void)fi;
	int i = 0;
	int rc = 0;
	
	// we will store already added files here to handle same file names across different branches
	struct hashtable *files = create_hashtable(16, string_hash, string_equal);

	struct hashtable *whiteouts = NULL;

	if (uopt.cow_enabled) whiteouts = create_hashtable(16, string_hash, string_equal);

	bool subdir_hidden = false;

	for (i = 0; i < uopt.nbranches; i++) {
		if (subdir_hidden) break;

		char p[PATHLEN_MAX];
		if (BUILD_PATH(p, uopt.branches[i].path, path)) {
			rc = -ENAMETOOLONG;
			goto out;
		}

		// check if branches below this branch are hidden
		int res = path_hidden(path, i);
		if (res < 0) {
			rc = res; // error
			goto out;
		}

		if (res > 0) subdir_hidden = true;

		DIR *dp = opendir(p);
		if (dp == NULL) {
			if (uopt.cow_enabled) read_whiteouts(path, whiteouts, i);
			continue;
		}

		struct dirent *de;
		while ((de = readdir(dp)) != NULL) {
			// already added in some other branch
			if (hashtable_search(files, de->d_name) != NULL) continue;

			// check if we need file hiding
			if (uopt.cow_enabled) {
				// file should be hidden from the user
				if (hashtable_search(whiteouts, de->d_name) != NULL) continue;
			}

			if (hide_meta_files(i, p, de) == true) continue;

			// fill with something dummy, we're interested in key existence only
			hashtable_insert(files, strdup(de->d_name), malloc(1));

			struct stat st;
			memset(&st, 0, sizeof(st));
			st.st_ino = de->d_ino;
			st.st_mode = de->d_type << 12;

			if (filler(buf, de->d_name, &st, 0)) break;
		}

		closedir(dp);
		if (uopt.cow_enabled) read_whiteouts(path, whiteouts, i);
	}

out:
	hashtable_destroy(files, 1);

	if (uopt.cow_enabled) hashtable_destroy(whiteouts, 1);

	if (uopt.stats_enabled && strcmp(path, "/") == 0) {
		filler(buf, "stats", NULL, 0);
	}

	return rc;
}

/**
 * check if a directory on all paths is empty
 * return 0 if empty, 1 if not and negative value on error
 *
 * TODO: This shares lots of code with unionfs_readdir(), can we merge 
 *       both functions?
 */
int dir_not_empty(const char *path) {

	DBG_IN();

	int i = 0;
	int rc = 0;
	int not_empty = 0;
	
	struct hashtable *whiteouts = NULL;

	if (uopt.cow_enabled) whiteouts = create_hashtable(16, string_hash, string_equal);

	bool subdir_hidden = false;

	for (i = 0; i < uopt.nbranches; i++) {
		if (subdir_hidden) break;

		char p[PATHLEN_MAX];
		if (BUILD_PATH(p, uopt.branches[i].path, path)) {
			rc = -ENAMETOOLONG;
			goto out;
		}

		// check if branches below this branch are hidden
		int res = path_hidden(path, i);
		if (res < 0) {
			rc = res; // error
			goto out;
		}

		if (res > 0) subdir_hidden = true;

		DIR *dp = opendir(p);
		if (dp == NULL) {
			if (uopt.cow_enabled) read_whiteouts(path, whiteouts, i);
			continue;
		}

		struct dirent *de;
		while ((de = readdir(dp)) != NULL) {
			
			// Ignore . and ..
			if ((strcmp(de->d_name, ".") == 0) ||  (strcmp(de->d_name, "..") == 0)) 
				continue;

			// check if we need file hiding
			if (uopt.cow_enabled) {
				// file should be hidden from the user
				if (hashtable_search(whiteouts, de->d_name) != NULL) continue;
			}

			if (hide_meta_files(i, p, de) == true) continue;

			// When we arrive here, a valid entry was found
			not_empty = 1;
			closedir(dp);
			goto out;
		}

		closedir(dp);
		if (uopt.cow_enabled) read_whiteouts(path, whiteouts, i);
	}

out:
	if (uopt.cow_enabled) hashtable_destroy(whiteouts, 1);

	if (rc) return rc;
	
	return not_empty;
}


