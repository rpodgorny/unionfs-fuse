#ifndef GENERAL_H
#define GENERAL_H

#include <stdbool.h>

bool file_hidden(const char *path);
char* u_dirname(char *path);
int remove_hidden(const char *path, int maxroot, uopt_t *uopt);
int hide_file(const char *path, int root_rw, uopt_t *uopt);

#endif

