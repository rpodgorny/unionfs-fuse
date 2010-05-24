/*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*/
#include <stdio.h>

#include "debug.h"

#ifdef DEBUG

FILE* dbgfile = NULL;

int debug_init(void) {
	char *dbgpath = "./unionfs_debug.log";
	printf("Debug mode, log will be written to %s\n", dbgpath);

	dbgfile = fopen(dbgpath, "w");
	if (!dbgfile) {
		printf("Failed to open %s for writing, exitting\n", dbgpath);
		return 2;
	}
	return 0;
}

#endif // DEBUG
