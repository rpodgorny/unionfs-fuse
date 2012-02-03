/*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>, 
*            Bernd Schubert <bernd.schubert@fastmail.fm>
*/
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "opts.h"
#include "debug.h"

static char default_debug_path[] = "./unionfs_debug.log";

FILE* dbgfile = NULL;

int debug_init(void) {
	char *dbgpath = uopt.dbgpath;

	if (!dbgpath) dbgpath = default_debug_path;

	printf("Debug mode, log will be written to %s\n", dbgpath);

	dbgfile = fopen(dbgpath, "w");
	if (!dbgfile) {
		printf("Failed to open %s for writing: %s.\nAborting!\n", 
		       dbgpath, strerror(errno));
		RETURN(2);
	}
	RETURN(0);
}
