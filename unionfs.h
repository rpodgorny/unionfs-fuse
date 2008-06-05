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

#endif
