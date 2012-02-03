/*
*  C Implementation: string
*
* Description: General string functions, not directly related to file system operations
*
* original implementation by Radek Podgorny
*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*
*/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>

#include "unionfs.h"
#include "opts.h"
#include "debug.h"
#include "general.h"
#include "usyslog.h"

/**
 * Check if the given fname suffixes the hide tag
 */
char *whiteout_tag(const char *fname) {
	DBG("%s\n", fname);

	char *tag = strstr(fname, HIDETAG);

	// check if fname has tag, fname is not only the tag, file name ends with the tag
	// TODO: static strlen(HIDETAG)
	if (tag && tag != fname && strlen(tag) == strlen(HIDETAG)) {
		return tag;
	}

	return NULL;
}

/**
 * copy one or more char arrays into dest and check for maximum size
 *
 * arguments: maximal string length and one or more char* string arrays
 *
 * check if the sum of the strings is larger than PATHLEN_MAX
 *
 * This function requires a NULL as last argument!
 * 
 * path already MUST have been allocated!
 */
int build_path(char *path, int max_len, const char *callfunc, int line, ...) {
	va_list ap; // argument pointer
	int len = 0;
	char *str_ptr = path;

	(void)str_ptr; // please the compile to avoid warning in non-debug mode
	(void)line;
	(void)callfunc; 

	path[0] = '\0'; // that way can easily strcat even the first element

	va_start(ap, line);
	while (1) {
		char *str = va_arg (ap, char *); // the next path element
		if (!str) break;

		/* Prevent '//' in pathes, if len > 0 we are not in the first 
		 * loop-run. This is rather ugly, but I don't see another way to 
		 * make sure there really is a '/'. By simply cutting off
		 * the initial '/' of the added string, we could run into a bug
		 * and would not have a '/' between path elements at all
		 * if somewhere else a directory slash has been forgotten... */
		if (len > 0) {
			// walk to the end of path
			while (*path != '\0') path++;
			
			// we are on '\0', now go back to the last char
			path--;
			
			if (*path == '/') {
				int count = len;
				
				// count makes sure nobody tricked us and gave
				// slashes as first path only...
				while (*path == '/' && count > 1) {
					// possibly there are several slashes...
					// But we want only one slash
					path--;
					count--; 
				}
					
				// now we are *before* '/', walk to slash again
				path++;
				
				// eventually we walk over the slashes of the
				// next string
				while (*str == '/') str++;
			} else if (*str != '/') {
				// neither path ends with a slash, nor str
				// starts with a slash, prevent a wrong path
				strcat(path, "/");
				len++;
			}
		}
		va_end(ap);

		len += strlen(str);

		// +1 for final \0 not counted by strlen
		if (len + 1 > max_len) {
			USYSLOG (LOG_WARNING, "%s():%d Path too long \n", callfunc, line);
			errno = ENAMETOOLONG;
			RETURN(-errno);
		}

		strcat (path, str);
	}
	
	if (len == 0) {
		USYSLOG(LOG_ERR, "from: %s():%d : No argument given?\n", callfunc, line);
		errno = EIO;
		RETURN(-errno);
	}
	
	DBG("from: %s():%d path: %s\n", callfunc, line, str_ptr);
	RETURN(0);
}

/**
 * dirname() in libc might not be thread-save, at least the man page states
 * "may return pointers to statically allocated memory", so we need our own
 * implementation
 */
char *u_dirname(const char *path) {
	DBG("%s\n", path);

	char *ret = strdup(path);
	if (ret == NULL) {
		USYSLOG(LOG_WARNING, "strdup failed, probably out of memory!\n");
		return ret;
	}

	char *ri = strrchr(ret, '/'); 
	if (ri != NULL) {
		*ri = '\0'; // '/' found, so a full path
	} else {
		strcpy(ret, "."); // '/' not found, so path is only a file
	}

	return ret;
}

/**
 * general elf hash (32-bit) function
 *
 * Algorithm taken from URL: http://www.partow.net/programming/hashfunctions/index.html,
 * but rewritten from scratch due to incompatible license.
 *
 * str needs to NULL terminated
 */
static unsigned int elfhash(const char *str) {
	DBG("%s\n", str);

	unsigned int hash = 0;

	while (*str) {
		hash = (hash << 4) + (*str); // hash * 16 + c

		// 0xF is 1111 in dual system, so highbyte is the highest byte of hash (which is 32bit == 4 Byte)
		unsigned int highbyte = hash & 0xF0000000UL;

		if (highbyte != 0) hash ^= (highbyte >> 24);
		// example (if the condition is met):
		//               hash = 10110000000000000000000010100000
		//           highbyte = 10110000000000000000000000000000
		//   (highbyte >> 24) = 00000000000000000000000010110000
		// after XOR:    hash = 10110000000000000000000000010000

		hash &= ~highbyte;
		//          ~highbyte = 01001111111111111111111111111111
		// after AND:    hash = 00000000000000000000000000010000

		str++;
	}

	return hash;
}

/**
 * Just a hash wrapper function, this way we can easily exchange the default
 * hash algorith.
 */
unsigned int string_hash(void *s) {
	return elfhash(s);
}
