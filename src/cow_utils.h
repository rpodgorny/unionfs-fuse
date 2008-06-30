/*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*/

#ifndef COW_UTILS_H
#define COW_UTILS_H

#define VM_AND_BUFFER_CACHE_SYNCHRONIZED
#define MAXBSIZE 4096

struct cow {
	mode_t umask;
	uid_t uid;

	// source file
	char  *from_path;
	struct stat *stat;

	// destination file
	char *to_path;
};

int copy_special(struct cow *cow);
int copy_fifo(struct cow *cow);
int copy_link(struct cow *cow);
int copy_file(struct cow *cow);

#endif
