#ifndef DEBUG_H
#define DEBUG_H


#ifdef DEBUG
	FILE* dbgfile;
	#define DBG(x) fprintf(dbgfile, "%s", x);
#else
	#define DBG(x) ;
#endif


#endif
