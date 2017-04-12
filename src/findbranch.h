/*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*/

#ifndef FINDBRANCH_H
#define FINDBRANCH_H

typedef enum searchflag {
	RWRO,
	RWONLY
} searchflag_t;

int find_rorw_branch(const char *path, int *branch);
int find_lowest_rw_branch(int branch_ro, int *branch);
int find_rw_branch_cutlast(const char *path, int *branch);
int __find_rw_branch_cutlast(const char *path, int *branch, int rw_hint);
int find_rw_branch_cow(const char *path, int *branch);
int find_rw_branch_cow_common(const char *path, int *branch, bool copy_dir);

#endif
