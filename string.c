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
int build_path(char *dest, int max_len, ...)
{
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

