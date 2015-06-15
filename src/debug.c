/*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd.schubert@fastmail.fm>
*/
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include "opts.h"
#include "debug.h"

static char default_debug_path[] = "./unionfs_debug.log";
static pthread_rwlock_t file_lock = PTHREAD_RWLOCK_INITIALIZER;

FILE* dbgfile = NULL;

int debug_init(void) {
	FILE* old_dbgfile = dbgfile;

	pthread_rwlock_wrlock(&file_lock);         // LOCK file
	pthread_rwlock_rdlock(&uopt.dbgpath_lock); // LOCK string

	char *dbgpath = uopt.dbgpath;

	if (!dbgpath) dbgpath = default_debug_path;

	printf("Debug mode, log will be written to %s\n", dbgpath);

	dbgfile = fopen(dbgpath, "w");

	int res = 0;
	if (!dbgfile) {
		printf("Failed to open %s for writing: %s.\nAborting!\n",
		       dbgpath, strerror(errno));

		dbgfile = old_dbgfile; // we can re-use the old file

		res = 2;
	} else if (old_dbgfile)
		fclose(old_dbgfile);

	setlinebuf(dbgfile); // line buffering

	pthread_rwlock_unlock(&uopt.dbgpath_lock); // UNLOCK string
	pthread_rwlock_unlock(&file_lock);         // UNLOCK file

	return res;
}

/**
 * Read-lock dbgfile and return it.
 */
FILE* get_dbgfile(void)
{
	pthread_rwlock_rdlock(&file_lock);
	return dbgfile;
}

/**
 * Unlock dbgfile
 */
void put_dbgfile(void)
{
	pthread_rwlock_unlock(&file_lock);
}

