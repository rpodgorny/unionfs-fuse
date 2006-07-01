#ifndef STATS_H


#define STATS_SIZE 2048


char stats_enabled;

unsigned long stats_read_b, stats_read_k, stats_read_m, stats_read_g;
unsigned long stats_written_b, stats_written_k, stats_written_m, stats_written_g;

unsigned long stats_cache_hits, stats_cache_misses;


void stats_init();
void stats_sprint(char *s);


#endif
