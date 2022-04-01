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

typedef enum filetype {
	NOT_EXISTING=-1,
	IS_DIR=0,
	IS_FILE=1,
} filetype_t;

int path_hidden(const char *path, int branch);
int remove_hidden(const char *path, int maxbranch);
int hide_file(const char *path, int branch_rw);
int hide_dir(const char *path, int branch_rw);
filetype_t path_is_dir (const char *path);
int maybe_whiteout(const char *path, int branch_rw, enum whiteout mode);
int set_owner(const char *path);
int path_create(const char *path, int nbranch_ro, int nbranch_rw);
int path_create_cutlast(const char *path, int nbranch_ro, int nbranch_rw);


#endif
