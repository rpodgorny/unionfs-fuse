#ifndef CACHE_H
#define CACHE_H


#include "unionfs.h"


#define CACHE_SIZE 1000


struct cache_entry {
	char path[PATHLEN_MAX];
	int root;
} cache[CACHE_SIZE];

int cache_pos;


void cache_init();
int cache_lookup(const char *path);
void cache_invalidate(const char *path);
void cache_save(const char *path, int root);


#endif
