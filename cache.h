#ifndef CACHE_H
#define CACHE_H


void cache_init();
int cache_lookup(const char *path);
void cache_invalidate(int);
void cache_invalidate_path(const char *path);
void cache_save(const char *path, int root);


#endif
