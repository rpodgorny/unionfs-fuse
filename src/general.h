/*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*/

#ifndef GENERAL_H
#define GENERAL_H

#include <stdbool.h>
#include <syslog.h>

enum  whiteout {
	WHITEOUT_FILE,
	WHITEOUT_DIR
};

typedef enum filetype {
	NOT_EXISTING=-1,
	IS_DIR=0,
	IS_FILE=1,
} filetype_t;

bool path_hidden(const char *path, int branch);
int remove_hidden(const char *path, int maxbranch);
int hide_file(const char *path, int branch_rw);
int hide_dir(const char *path, int branch_rw);
filetype_t path_is_dir (const char *path);
int maybe_whiteout(const char *path, int branch_rw, enum whiteout mode);
int set_owner(const char *path);
int initialize_features(void);

/**
 * Calling syslog() will deadlock if the filesystem is for /etc or /var.
 * This is a bit unexpected, since I thought it would just write into a buffer
 * and then the syslog-daemon would then independetely of the unionfs thread
 * write it's log entry. However, it seems the syslog() call waits for until
 * the log entry is written, which will cause a deadlock, of course.
 * Until we find a solution for that, we simply disable syslogs.
 * The only sane solution comming presently into my mind is to write into
 * a buffer and then to have an independent thread which will flush this buffer
 * to syslog. Other suggestions are welcome, of course!
 */
#define usyslog(...) do {} while(0)


#endif
