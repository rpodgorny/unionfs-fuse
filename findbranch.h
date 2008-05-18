/*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*/

#ifndef FINDBRANCH_H
#define FINDBRANCH_H

int find_rorw_root(const char *path);
int find_lowest_rw_root(int root_ro);
int find_rw_root_cutlast(const char *path);
int find_rw_root_cow(const char *path);

#endif
