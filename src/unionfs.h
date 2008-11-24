/*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*/

#ifndef UNIONFS_H
#define UNIONFS_H

#define PATHLEN_MAX 1024
#define HIDETAG "_HIDDEN~"
#define METADIR ".unionfs/"

typedef struct {
	char *path;
	int fd; // used to prevent accidental umounts of path
	unsigned char rw; // the writable flag
} branch_entry_t;

/**
 * structure to have information about the current union
 */
typedef struct {
	// read-writable branches
	struct rw_branches {
		int n_rw;	 // number of rw-branches
		unsigned *rw_br; // integer array of rw-branches
	} rw_branches;

	// branches used for statfs
	struct statvfs {
		int nbranches;	// number of statvfs branches
		int *branches;	// array of integers with the branch numbers
	} statvfs;
} ufeatures_t;

extern ufeatures_t ufeatures;

#endif
