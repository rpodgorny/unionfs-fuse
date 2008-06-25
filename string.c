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
#include <syslog.h>
#include <errno.h>

#include "unionfs.h"
#include "opts.h"


/**
 * Check if the given fname suffixes the hide tag
 */
char *whiteout_tag(const char *fname) {
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
 */
int build_path(char *dest, int max_len, ...) {
	va_list ap; // argument pointer
	int len = 0;

	dest[0] = '\0';

	va_start(ap, max_len);
	while (1) {
		char *str = va_arg (ap, char *);
		if (!str) break;

		len += strlen(str) + 1; // + 1 for '/' between the pathes

		if (len > max_len)
			return -ENAMETOOLONG;

		strcat (dest, str);
	}

	return 0;
}

/**
 * dirname() in libc might not be thread-save, at least the man page states
 * "may return pointers to statically allocated memory", so we need our own
 * implementation
 */
char *u_dirname(const char *path) {
	char *ret = strdup(path);

	char *ri = rindex(ret, '/'); //this char should always be found
	*ri = '\0';

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
	unsigned int hash = 0;

	while (*str) {
		hash = (hash << 4) + (*str); // hash * 16 + c

		// 0xF is 1111 in dual system, so highbyte is the highest byte of hash (which is 32bit == 4 Byte)
		unsigned int highbyte = hash & 0xF0000000UL;
		if (highbyte != 0) {
			// hash = hash ^ (highbyte / 2^24)
			// example: hash             =         10110000000000000000000010100000
			//          highbyte         =         10110000000000000000000000000000
			//          (highbyte >> 24) =         00000000000000000000000010110000
			hash ^= (highbyte >> 24); // XOR both: 11110000000000000000000000010000
		}
		
		// Finally an AND operation with ~highbyte(01001111111111111111111111111111)
		hash &= ~highbyte; //                      01000000000000000000000000010000

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

/**
  * Test if two strings are eqal.
  * Return 1 if the strings are equal and 0 if they are different.
  */ 
int string_equal(void *s1, void *s2) {
	if (strcmp(s1, s2) == 0) return 1;
	return 0;
}

