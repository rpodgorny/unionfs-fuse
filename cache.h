#ifndef CACHE_H
#define CACHE_H

#include <time.h>

typedef struct {
	int root;
	time_t time; // timestamp of cache entry creation
} cache_entry_t;


extern struct hashtable *cache;


void cache_init();
int cache_lookup(const char *path);
void cache_invalidate_path(const char *path);
void cache_save(const char *path, int root);
unsigned int cache_size();


#endif
