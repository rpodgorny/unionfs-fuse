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

#endif
