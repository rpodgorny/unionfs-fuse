/*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*/

#ifndef COW_UTILS_H
#define COW_UTILS_H

#define VM_AND_BUFFER_CACHE_SYNCHRONIZED

// this is defined on bsd systems (thus, also on macos) but linux lacks it
#ifndef MAXBSIZE
	#define MAXBSIZE 4096  // TODO: isn't this too low? some bsds have this at 16k, macos seems to have this at (256 * 4096)
#endif

struct cow {
	mode_t umask;
	uid_t uid;

	// source file
	char  *from_path;
	struct stat *stat;

	// destination file
	char *to_path;
};

int setfile(const char *path, struct stat *fs);
int copy_special(struct cow *cow);
int copy_fifo(struct cow *cow);
int copy_link(struct cow *cow);
int copy_file(struct cow *cow);

#endif
