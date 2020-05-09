/*
 * License: BSD-style license
 * Copyright: Radek Podgorny <radek@podgorny.cz>,
 *            Bernd Schubert <bernd.schubert@fastmail.fm
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <errno.h>
#include <sys/time.h>
#include "opts.h"
#include "general.h"

#define DBG_IN() DBG("\n");

#define DBG(format, ...) \
	do { \
		if (!uopt.debug) break; \
		int _errno = errno; \
		FILE* dbgfile = get_dbgfile(); \
		struct timeval _tv_dbg; \
		gettimeofday(&_tv_dbg, NULL); \
		print_iso8601(stderr, _tv_dbg); \
		print_iso8601(dbgfile, _tv_dbg); \
		fprintf(stderr, " %s(): %d: ", __func__, __LINE__); \
		fprintf(dbgfile, " %s(): %d: ", __func__, __LINE__); \
		fprintf(stderr, format, ##__VA_ARGS__); \
		fprintf(dbgfile, format, ##__VA_ARGS__); \
		fflush(stderr); \
		fflush(stdout); \
		put_dbgfile(); \
		errno = _errno; \
	} while (0)

#define RETURN(returncode) \
	do { \
		if (uopt.debug) DBG("return %d\n", returncode); \
		return returncode; \
	} while (0)


/* In order to prevent useless function calls and to make the compiler
 * to optimize those out, debug.c will only have definitions if DEBUG
 * is defined. So if DEBUG is NOT defined, we define empty functions here */
int debug_init();
void dbg_in(const char *function);

FILE* get_dbgfile(void);
void put_dbgfile(void);

#endif // DEBUG_H

