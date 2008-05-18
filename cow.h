/*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*/

#ifndef COW_H
#define COW_H

#include <sys/stat.h>

int cow_cp(const char *path, int root_ro, int root_rw);
int path_create(const char *path, int nroot_ro, int nroot_rw);
int path_create_cutlast(const char *path, int nroot_ro, int nroot_rw);

#endif
