#include <string.h>
#include <sys/time.h>

#include "cache.h"
#include "stats.h"
#include "unionfs.h"


#define CACHE_SIZE 1000
#define MAX_CACHE_TIME 60


static struct cache_entry {
	char path[PATHLEN_MAX];
	int root;
	time_t time; // timestamp of cache entry creation
} cache[CACHE_SIZE];

static int cache_pos;


void cache_init() {
	int i = 0;
	for (i = 0; i < CACHE_SIZE; i++) {
		cache[i].path[0] = 0;
		cache[i].root = -1;
	}

	cache_pos = 0;
}

int cache_lookup(const char *path) {
	struct timeval tv;
	gettimeofday(&tv, NULL);

	int i = 0;
	for (i = 0; i < CACHE_SIZE; i++) {
		int pos = cache_pos - i;
		if (pos < 0) pos += CACHE_SIZE;

		if ((tv.tv_sec - cache[pos].time) > MAX_CACHE_TIME) {
			// too old
			cache_invalidate(pos);
			continue;
		}

		if (strcmp(cache[pos].path, path) == 0) {
			// found in cache
			if (stats_enabled) stats_cache_hits++;
			return cache[pos].root;
		}
	}

	if (stats_enabled) stats_cache_misses++;

	return -1;
}

void cache_invalidate(int i) {
	cache[i].path[0] = 0;
	cache[i].root = -1;
	cache[i].time = 0;
}

void cache_invalidate_path(const char *path) {
	int i = 0;
	for (i = 0; i < CACHE_SIZE; i++) {
		int pos = cache_pos - i;
		if (pos < 0) pos += CACHE_SIZE;

		if (strcmp(cache[pos].path, path) == 0) {
			cache_invalidate(pos);
		}
	}
}

void cache_save(const char *path, int root) {
	cache_pos++;
	cache_pos %= CACHE_SIZE;

	strncpy(cache[cache_pos].path, path, PATHLEN_MAX);
	cache[cache_pos].root = root;

	struct timeval tv;
	gettimeofday(&tv, NULL);
	cache[cache_pos].time = tv.tv_sec;
}
