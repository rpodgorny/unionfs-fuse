/*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*/

#ifndef UNIONFS_STRING_H
#define UNIONFS_STRING_H

#include <string.h>

char *whiteout_tag(const char *fname);
int build_path(char *dest, int max_len, const char *callfunc, int line, ...);
char *u_dirname(const char *path);
unsigned int string_hash(void *s);

/**
 * A wrapper for build_path(). In build_path() we test if the given number of strings does exceed
 * a maximum string length. Since there is no way in C to determine the given number of arguments, we
 * simply add NULL here.
 */
#define BUILD_PATH(dest, ...) build_path(dest, PATHLEN_MAX, __func__, __LINE__, __VA_ARGS__, NULL)

/**
  * Test if two strings are eqal.
  * Return 1 if the strings are equal and 0 if they are different.
  */
// This is left in the header file bacause gcc is too stupid to inline across object files
static inline int string_equal(void *s1, void *s2) {
	if (strcmp(s1, s2) == 0) return 1;
	return 0;
}

#endif // UNIONFS_STRING_H
