/*
 * License: BSD-style license
 * Copyright: Radek Podgorny <radek@podgorny.cz>,
 *            Bernd Schubert <bernd.schubert@fastmail.fm
 */

#ifndef DEBUG_H
#define DEBUG_H

#include "opts.h"

#define DBG_IN() DBG("\n");

#define DBG(format, ...) 						\
	do {								\
		debug_operation(__func__, __LINE__, format, ##__VA_ARGS__);	\
	} while (0)

#define RETURN(returncode) 						\
	do {								\
		return debug_return(__func__, __LINE__, returncode);		\
	} while (0)


/* In order to prevent useless function calls and to make the compiler
 * to optimize those out, debug.c will only have definitions if DEBUG
 * is defined. So if DEBUG is NOT defined, we define empty functions here */
int debug_init();
FILE* get_dbgfile(void);
void put_dbgfile(void);
void debug_operation(const char *func, int line, const char *format, ...);
int debug_return(const char *func, int line, int returncode);

#endif // DEBUG_H

