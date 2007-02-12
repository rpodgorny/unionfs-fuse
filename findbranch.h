#ifndef FINDBRANCH_H
#define FINDBRANCH_H

int findroot(const char *path);
int find_lowest_rw_root(int root_ro);
int find_rw_root_with_cow(const char *path);

#endif
