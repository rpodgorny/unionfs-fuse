/*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*/

#ifndef DEBUG_H
#define DEBUG_H


#ifdef DEBUG
	extern FILE* dbgfile;
	//#define DBG(x) fprintf(dbgfile, "%s\n", x);
	#define DBG_IN() printf("In %s()\n", __func__);

	#define DBG(...) 						\
		do {							\
			printf("%s(): %d: ", __func__, __LINE__);	\
			printf(__VA_ARGS__);				\
		} while (0);
#else
	#define DBG_IN();
	#define DBG(...);
#endif

int debug_init();


#endif
