#ifndef GENERAL_H
#define GENERAL_H

#include <stdbool.h>

#define RETURN_ERROR {to_root(); return -errno;}

bool file_hidden(const char *path);
char *u_dirname(const char *path);
int remove_hidden(const char *path, int maxroot);
int hide_file(const char *path, int root_rw);
void to_user(void);
void to_root(void);

#endif
