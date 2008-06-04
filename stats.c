/*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*/

#include <stdio.h>
#include <string.h>

#include "stats.h"
#include "opts.h"


void stats_init(struct stats_t *s) {
	memset(s, 0, sizeof(struct stats_t));
}

void stats_sprint(struct stats_t *s, char *out) {
	strcpy(out, "");

	sprintf(out+strlen(out), "Bytes read: %u,%03u,%03u,%03u,%03u\n", s->r_t, s->r_g, s->r_m, s->r_k, s->r_b);
	sprintf(out+strlen(out), "Bytes written: %u,%03u,%03u,%03u,%03u\n", s->w_t, s->w_g, s->w_m, s->w_k, s->w_b);
}

void stats_add_read(struct stats_t *s, unsigned int bytes) {
	s->r_b += bytes;

	while (s->r_b >= 1000) { s->r_k++; s->r_b -= 1000; }
	while (s->r_k >= 1000) { s->r_m++; s->r_k -= 1000; }
	while (s->r_m >= 1000) { s->r_g++; s->r_m -= 1000; }
	while (s->r_g >= 1000) { s->r_t++; s->r_g -= 1000; }
}

void stats_add_written(struct stats_t *s, unsigned int bytes) {
	s->w_b += bytes;

	while (s->w_b >= 1000) { s->w_k++; s->w_b -= 1000; }
	while (s->w_k >= 1000) { s->w_m++; s->w_k -= 1000; }
	while (s->w_m >= 1000) { s->w_g++; s->w_m -= 1000; }
	while (s->w_g >= 1000) { s->w_t++; s->w_g -= 1000; }
}
