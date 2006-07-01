#ifndef UNIONFS_H
#define UNIONFS_H


#define ROOTS_MAX 20
#define PATHLEN_MAX 1024
#define STATS_SIZE 2048


int nroots;
char roots[ROOTS_MAX][PATHLEN_MAX];

char stats;
long bytes_read, bytes_written;


#endif
