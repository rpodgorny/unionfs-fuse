#ifndef UNIONFS_STRING_H
#define UNIONFS_STRING_H

char *whiteout_tag(const char *fname);
int build_path(char *dest, int max_len, ...);
char *u_dirname(const char *path);

/**
 * A wrapper for build_path(). In build_path() we test if the given number of strings does exceed
 * a maximum string length. Since there is no way in C to determine the given number of arguments, we
 * simply add NULL here.
 */
#define BUILD_PATH(dest, ...) build_path(dest, PATHLEN_MAX, __VA_ARGS__, NULL)

#endif // UNIONFS_STRING_H
