#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "cache.h"
#include "stats.h"
#include "unionfs.h"
#include "opts.h"
#include "hash.h"
#include "hashtable.h"


struct hashtable *cache;


void cache_init() {
	cache = create_hashtable(16, string_hash, string_equal);
}

int cache_lookup(const char *path) {
	struct timeval tv;
	gettimeofday(&tv, NULL);

	cache_entry_t *e = hashtable_search(cache, (void *)path);

	if (e) {
		// found
		if ((tv.tv_sec - e->time) > uopt.cache_time) {
			// too old
			cache_invalidate_path(path);
			if (uopt.stats_enabled) stats_cache_misses++;
			return -1;
		}

		// refresh the cache entry (to improve cache hit ratio)
		e->time = tv.tv_sec;

		if (uopt.stats_enabled) stats_cache_hits++;
		return e->root;
	}

	if (uopt.stats_enabled) stats_cache_misses++;
	return -1;
}

void cache_invalidate_path(const char *path) {
	cache_entry_t *e = hashtable_remove(cache, (void *)path);
	free(e);
}

void cache_save(const char *path, int root) {
	struct timeval tv;
	gettimeofday(&tv, NULL);

	cache_entry_t *e = malloc(sizeof(cache_entry_t));
	e->root = root;
	e->time = tv.tv_sec;

	hashtable_insert(cache, strdup(path), e);
}

int cache_size() {
	return hashtable_count(cache);
}
