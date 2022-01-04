#ifndef READDIR_H
#define READDIR_H

#include <fuse.h>

#if FUSE_USE_VERSION < 30
int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t off, struct fuse_file_info *fi);
#else
int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t off, struct fuse_file_info *fi, enum fuse_readdir_flags flags);
#endif

int dir_not_empty(const char *path);

#endif
