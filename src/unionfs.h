/*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*/

#ifndef UNIONFS_H
#define UNIONFS_H

#define PATHLEN_MAX 1024
#define HIDETAG "_HIDDEN~"

#define METANAME ".unionfs"
#define METADIR (METANAME  "/") // string concetanation!

// fuse meta files, we might want to hide those
#define FUSE_META_FILE ".fuse_hidden"
#define FUSE_META_LENGTH 12

// file access protection mask
#define S_PROT_MASK (S_ISUID| S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO)

typedef struct {
	char *path;
	int path_len;		// strlen(path)
	int fd;			 // used to prevent accidental umounts of path
	unsigned char rw;	 // the writable flag
} branch_entry_t;

#endif
