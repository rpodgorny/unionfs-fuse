#ifndef STATS_H


#define STATS_SIZE 2048


char stats_enabled;

unsigned int stats_read_b, stats_read_k, stats_read_m, stats_read_g, stats_read_t;
unsigned int stats_written_b, stats_written_k, stats_written_m, stats_written_g, stats_written_t;

unsigned int stats_cache_hits, stats_cache_misses;


void stats_init();
void stats_sprint(char *s);

void stats_add_read(unsigned int);
void stats_add_written(unsigned int);


#endif
