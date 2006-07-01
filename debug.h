#ifndef DEBUG_H
#define DEBUG_H


#ifdef DEBUG
	FILE* dbgfile;
	//#define DBG(x) fprintf(dbgfile, "%s\n", x);
	#define DBG(x) printf("%s\n", x);
#else
	#define DBG(x) ;
#endif


#endif
