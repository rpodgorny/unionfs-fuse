/*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*/

#ifndef GENERAL_H
#define GENERAL_H

#include <stdbool.h>

enum  whiteout {
	WHITEOUT_FILE,
	WHITEOUT_DIR
};

bool path_hidden(const char *path, int branch);
int remove_hidden(const char *path, int maxroot);
int hide_file(const char *path, int root_rw);
int hide_dir(const char *path, int root_rw);
int path_is_dir (const char *path);
int maybe_whiteout(const char *path, int root_rw, enum whiteout mode);
void to_user(void);
void to_root(void);

#endif
