/*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*/

#ifndef STATS_H
#define STATS_H


#define STATS_FILENAME "/stats"
#define STATS_SIZE 2048


struct stats_t {
	// (bytes, kilo, mega, giga, tera)
	unsigned int r_b, r_k, r_m, r_g, r_t; // read
	unsigned int w_b, w_k, w_m, w_g, w_t; // written
};


// global statistics holder
struct stats_t stats;


void stats_init(struct stats_t*);
void stats_sprint(struct stats_t*, char *);

void stats_add_read(struct stats_t*, unsigned int);
void stats_add_written(struct stats_t*, unsigned int);


#endif
