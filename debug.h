#ifndef DEBUG_H
#define DEBUG_H


#ifdef DEBUG
	extern FILE* dbgfile;
	//#define DBG(x) fprintf(dbgfile, "%s\n", x);
	#define DBG(x) printf("%s\n", x);
#else
	#define DBG(x) ;
#endif

int debug_init();


#endif
