/*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*/

#ifndef DEBUG_H
#define DEBUG_H

extern FILE* dbgfile;

#define DBG_IN() DBG("\n");

#ifdef DEBUG
#define DBG(format, ...) 						\
	do {								\
		fprintf(stderr, "%s(): %d: ", __func__, __LINE__);	\
		fprintf(dbgfile, "%s(): %d: ", __func__, __LINE__);	\
		fprintf(stderr, format, ##__VA_ARGS__);			\
		fprintf(dbgfile, format, ##__VA_ARGS__);		\
		fflush(stderr);						\
		fflush(stdout);						\
	} while (0)
#else
#define DBG(...) do {} while (0)
#endif


/**
 * Calling syslog() will deadlock if the filesystem is for /etc or /var.
 * This is a bit unexpected, since I thought it would just write into a buffer
 * and then the syslog-daemon would then independetely of the unionfs thread
 * write it's log entry. However, it seems the syslog() call waits for until
 * the log entry is written, which will cause a deadlock, of course.
 * Until we find a solution for that, we simply disable syslogs.
 * The only sane solution comming presently into my mind is to write into
 * a buffer and then to have an independent thread which will flush this buffer
 * to syslog. Other suggestions are welcome, of course!
 */
#define usyslog(priority, format, ...)	DBG(format, ##__VA_ARGS__)


/* In order to prevent useless function calls and to make the compiler
 * to optimize those out, debug.c will only have definitions if DEBUG 
 * is defined. So if DEBUG is NOT defined, we define empty functions here */
#ifdef DEBUG
int debug_init();
void dbg_in(const char *function);
#else
static inline int debug_init(void) {return 0;};
static inline void dbg_in(void) {};
#endif // DEBUG


#endif // DEBUG_H

