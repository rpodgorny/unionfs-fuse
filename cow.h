#ifndef COW_H
#define COW_H

int path_create(const char *path, int nroot_ro, int nroot_rw);
int path_create_cutlast(const char *path, int nroot_ro, int nroot_rw);

#endif
