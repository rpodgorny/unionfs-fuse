/*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*/

#ifndef COW_H
#define COW_H

#include <sys/stat.h>

int cow_cp(const char *path, int branch_ro, int branch_rw);
int path_create(const char *path, int nbranch_ro, int nbranch_rw);
int path_create_cutlast(const char *path, int nbranch_ro, int nbranch_rw);
int copy_directory(const char *path, int branch_ro, int branch_rw);

#endif
