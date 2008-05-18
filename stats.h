/*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*/

#ifndef STATS_H
#define STATS_H


#define STATS_FILENAME "/stats"
#define STATS_SIZE 2048


void stats_init();
void stats_sprint(char *s);

void stats_add_read(unsigned int);
void stats_add_written(unsigned int);


#endif
