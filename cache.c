#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>

#include "cache.h"
#include "stats.h"
#include "unionfs.h"
#include "opts.h"
#include "hash.h"
#include "hashtable.h"
#include "hashtable_itr.h"


static struct hashtable *cache;
static pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;



// Thow away the old records
static void cache_clean_old() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	
	pthread_mutex_lock (&mutex1);

	struct hashtable_itr *itr = hashtable_iterator(cache);
	int res = 1;
	do {
		// Maybe a bug in hastable library? We solve it this way
		if (!itr->e) break;

		cache_entry_t *e = hashtable_iterator_value(itr);

		if ((tv.tv_sec - e->time) > uopt.cache_time) {
			// too old
			res = hashtable_iterator_remove(itr);
			free(e);
		} else {
			res = hashtable_iterator_advance(itr);
		}
	} while (res != 0);
	free(itr);

	pthread_mutex_unlock (&mutex1);
}

void cache_init() {
	cache = create_hashtable(16, string_hash, string_equal);
}

int cache_lookup(const char *path) {
	struct timeval tv;
	gettimeofday(&tv, NULL);

	static time_t last_clean_time = 0; // when we last threw away old entries
	if ((tv.tv_sec - last_clean_time) > uopt.cache_time) {
		// it's time for the cleanup
		cache_clean_old();
		last_clean_time = tv.tv_sec;
	}

	pthread_mutex_lock (&mutex1);
	cache_entry_t *e = hashtable_search(cache, (void *)path);
	pthread_mutex_unlock (&mutex1);

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
	pthread_mutex_lock (&mutex1);
	cache_entry_t *e = hashtable_remove(cache, (void *)path);
	pthread_mutex_unlock (&mutex1);
	free(e);
}

void cache_save(const char *path, int root) {
	struct timeval tv;
	gettimeofday(&tv, NULL);

	cache_entry_t *e = malloc(sizeof(cache_entry_t));
	e->root = root;
	e->time = tv.tv_sec;

	pthread_mutex_lock (&mutex1);
	hashtable_insert(cache, strdup(path), e);
	pthread_mutex_unlock (&mutex1);
}

unsigned int cache_size() {
	pthread_mutex_lock (&mutex1);
	return hashtable_count(cache);
	pthread_mutex_unlock (&mutex1);
}
