#ifndef READDIR_H
#define READDIR_H

#include <fuse3/fuse.h>

int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);
int dir_not_empty(const char *path);

#endif
