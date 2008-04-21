#ifndef GENERAL_H
#define GENERAL_H

#include <stdbool.h>

char *whiteout_tag(const char *fname);
bool path_hidden(const char *path);
char *u_dirname(const char *path);
int remove_hidden(const char *path, int maxroot);
int hide_file(const char *path, int root_rw);
int hide_dir(const char *path, int root_rw);
int path_is_dir (const char *path);
void to_user(void);
void to_root(void);
bool string_too_long(int max_len, ...);

/**
 * Wrapper macro for string_too_long(). In string_too_long we test if the given number of strings does exceed
 * a maximum string length. Since there is no way in C to determine the given number of arguments, we
 * simply add NULL here.
 */
#define STR_TOO_LONG(len, ...) string_too_long(len, __VA_ARGS__, NULL)

#endif
