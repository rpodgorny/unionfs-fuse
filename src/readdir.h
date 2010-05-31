#ifndef READDIR_H
#define READDIR_H

#include <fuse.h>

int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
int dir_not_empty(const char *path);

#endif
