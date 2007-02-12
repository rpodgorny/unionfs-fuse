/*
Written by Radek Podgorny

This is offered under a BSD-style license. This means you can use the code for whatever you desire in any way you may want but you MUST NOT forget to give me appropriate credits when spreading your work which is based on mine. Something like "original implementation by Radek Podgorny" should be fine.
*/

#ifdef linux
	// For pread()/pwrite()
	#define _XOPEN_SOURCE 500
#endif

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/statvfs.h>

#ifdef HAVE_SETXATTR
	#include <sys/xattr.h>
#endif

#include "unionfs.h"
#include "opts.h"
#include "cache.h"
#include "stats.h"
#include "debug.h"
#include "findbranch.h"
#include "general.h"

#include "unlink.h"
#include "readdir.h"
#include "cow.h"


static struct fuse_opt unionfs_opts[] = {
	FUSE_OPT_KEY("--help", KEY_HELP),
	FUSE_OPT_KEY("--version", KEY_VERSION),
	FUSE_OPT_KEY("-h", KEY_HELP),
	FUSE_OPT_KEY("-V", KEY_VERSION),
	FUSE_OPT_KEY("stats", KEY_STATS),
	FUSE_OPT_KEY("cache", KEY_CACHE),
	FUSE_OPT_KEY("cache-time=", KEY_CACHE_TIME),
	FUSE_OPT_KEY("cow", KEY_COW),
	FUSE_OPT_END
};


static int unionfs_access(const char *path, int mask) {
	DBG("access\n");

	int i = find_rorw_root(path);
	if (i == -1) return -errno;

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

	int res = access(p, mask);
	if (res == -1) {
		if (errno == ENOENT) {
			// The user may have moved the file among roots
			if (uopt.cache_enabled) cache_invalidate_path(path);

			i = find_rorw_root(path);
			if (i == -1) return -errno;

			snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

			res = access(p, mask);
			if (res == -1) return -errno;
		} else {
			return -errno;
		}
	}

	return 0;
}

static int unionfs_chmod(const char *path, mode_t mode) {
	DBG("chmod\n");

	int i = find_rw_root_cow_cutlast(path);
	if (i == -1) return -errno;

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

	int res = chmod(p, mode);
	if (res == -1) {
		if (errno == ENOENT) {
			// The user may have moved the file among roots
			if (uopt.cache_enabled) cache_invalidate_path(path);

			i = find_rw_root_cow_cutlast(path);
			if (i == -1) return -errno;

			snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

			res = chmod(p, mode);
			if (res == -1) return -errno;
		} else {
			return -errno;
		}
	}

	return 0;
}

static int unionfs_chown(const char *path, uid_t uid, gid_t gid) {
	DBG("chown\n");

	int i = find_rw_root_cow_cutlast(path);
	if (i == -1) return -errno;

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

	int res = lchown(p, uid, gid);
	if (res == -1) {
		if (errno == ENOENT) {
			if (uopt.cache_enabled) cache_invalidate_path(path);

			i = find_rw_root_cow_cutlast(path);
			if (i == -1) return -errno;

			snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

			res = lchown(p, uid, gid);
			if (res == -1) return -errno;
		} else {
			return -errno;
		}
	}

	return 0;
}

// flush may be called multiple times for an open file, this must not really close the file. This is important if used on a network filesystem like NFS which flush the data/metadata on close()
static int unionfs_flush(const char *path, struct fuse_file_info *fi) {
	DBG("flush\n");

	if (uopt.stats_enabled && strcmp(path, STATS_FILENAME) == 0) return 0;

	int fd = dup(fi->fh);

	if (fd == -1) {
		// What to do now?
		if (fsync(fi->fh) == -1) return -EIO;
		return 0;
	}

	if (close(fd) == -1) return -errno;

	return 0;
}

// Just a stub. This method is optional and can safely be left unimplemented
static int unionfs_fsync(const char *path, int isdatasync, struct fuse_file_info *fi) {
	DBG("fsync\n");

	if (uopt.stats_enabled && strcmp(path, STATS_FILENAME) == 0) return 0;

	int res;
	if (isdatasync) {
#ifdef _POSIX_SYNCHRONIZED_IO
		res = fdatasync(fi->fh);
#else
		res = fsync(fi->fh);
#endif
	} else {
		res = fsync(fi->fh);
	}

	if (res == -1) return -errno;

	return 0;
}

static int unionfs_getattr(const char *path, struct stat *stbuf) {
	DBG("getattr\n");

	if (uopt.stats_enabled && strcmp(path, STATS_FILENAME) == 0) {
		memset(stbuf, 0, sizeof(stbuf));
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = STATS_SIZE;
		return 0;
	}

	int i = find_rorw_root(path);
	if (i == -1) return -errno;

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

	int res = lstat(p, stbuf);
	if (res == -1) {
		if (errno == ENOENT) {
			if (uopt.cache_enabled) cache_invalidate_path(path);

			i = find_rorw_root(path);
			if (i == -1) return -errno;

			snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

			res = lstat(p, stbuf);
			if (res == -1) return -errno;
		} else {
			return -errno;
		}
	}

	return 0;
}

static int unionfs_link(const char *from, const char *to) {
	DBG("link\n");
	
	// hardlinks do not work across different filesystems,so we need a copy of from first.
	int i = find_rw_root_cow_cutlast(from);
	if (i == -1)  return -errno; // copying from failed;

	char f[PATHLEN_MAX], t[PATHLEN_MAX];
	snprintf(f, PATHLEN_MAX, "%s%s", uopt.roots[i].path, from);
	snprintf(t, PATHLEN_MAX, "%s%s", uopt.roots[i].path, to);

	int res = link(f, t);
	if (res == -1) {
		if (errno == ENOENT) {
			if (uopt.cache_enabled) cache_invalidate_path(to);

			i = find_rw_root_cow_cutlast(from);
			if (i == -1) return -errno;

			snprintf(t, PATHLEN_MAX, "%s%s", uopt.roots[i].path, to);

			res = link(f, t);
			if (res == -1) return -errno;
		} else {
			return -errno;
		}
	}

	return 0;
}

static int unionfs_mkdir(const char *path, mode_t mode) {
	DBG("mkdir\n");

	int i = find_rw_root_cow(path);
	if (i == -1) return -errno;

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

	int res = mkdir(p, mode);
	if (res == -1) {
		if (errno == ENOENT) {
			if (uopt.cache_enabled) cache_invalidate_path(path);

			i = find_rw_root_cow(path);
			if (i == -1) return -errno;

			snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

			res = mkdir(p, mode);
			if (res == -1) return -errno;
		} else {
			return -errno;
		}
	}

	return 0;
}

static int unionfs_mknod(const char *path, mode_t mode, dev_t rdev) {
	DBG("mknod\n");

	int i = find_rw_root_cow(path);
	if (i == -1) return -errno;

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

	int res = mknod(p, mode, rdev);
	if (res == -1) {
		if (errno == ENOENT) {
			if (uopt.cache_enabled) cache_invalidate_path(path);

			i = find_rw_root_cow(path);
			if (i == -1) return -errno;

			snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

			res = mknod(p, mode, rdev);
			if (res == -1) return -errno;
		} else {
			return -errno;
		}
	}
	
	remove_hidden(path, i);

	return 0;
}

static int unionfs_open(const char *path, struct fuse_file_info *fi) {
	DBG("open\n");

	if (uopt.stats_enabled && strcmp(path, STATS_FILENAME) == 0) {
		if ((fi->flags & 3) == O_RDONLY) {
			fi->direct_io = 1;
			return 0;
		}
		return -EACCES;
	}

	int i;
	if (fi->flags & (O_WRONLY | O_RDWR)) {
		i = find_rw_root_cow(path);
	} else {
		i = find_rorw_root(path);
	}
	
	if (i == -1) return -errno;
	
	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

	int fd = open(p, fi->flags);
	if (fd == -1) {
		if (errno == ENOENT) {
			// The user may have moved the file among roots
			if (uopt.cache_enabled) cache_invalidate_path(path);

			i = find_rw_root_cow(path);
			if (i == -1) return -errno;
		
			snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

			fd = open(p, fi->flags);
			if (fd == -1) return -errno;
		} else {
			return -errno;
		}
	}
	
	if (fi->flags & (O_WRONLY | O_RDWR)) {
		/* There might have been a hide file, but since we successfully 
		* wrote to the real file, a hide file must not exist anymore */
		remove_hidden(path, i);
	}


	fi->direct_io = 1;
	fi->fh = (unsigned long)fd;

	return 0;
}

static int unionfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	DBG("read\n");

	if (uopt.stats_enabled && strcmp(path, STATS_FILENAME) == 0) {
		char out[STATS_SIZE] = "";
		stats_sprint(out);

		int s = size;
		if (offset < strlen(out)) {
			if (s > strlen(out)-offset) s = strlen(out)-offset;
			memcpy(buf, out+offset, s);
		} else {
			s = 0;
		}

		return s;
	}

	int res = pread(fi->fh, buf, size, offset);
	if (res == -1) return -errno;

	if (uopt.stats_enabled) stats_add_read(size);

	return res;
}


static int unionfs_readlink(const char *path, char *buf, size_t size) {
	DBG("readlink\n");

	int i = find_rorw_root(path);
	if (i == -1) return -errno;

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

	int res = readlink(p, buf, size - 1);
	if (res == -1) {
		if (errno == ENOENT) {
			if (uopt.cache_enabled) cache_invalidate_path(path);

			i = find_rorw_root(path);
			if (i == -1) return -errno;

			snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

			res = readlink(p, buf, size - 1);
			if (res == -1) return -errno;
		} else {
			return -errno;
		}
	}

	buf[res] = '\0';

	return 0;
}

static int unionfs_release(const char *path, struct fuse_file_info *fi) {
	DBG("release\n");

	if (uopt.stats_enabled && strcmp(path, STATS_FILENAME) == 0) return 0;

	int res = close(fi->fh);
	if (res == -1) return -errno;

	return 0;
}

static int unionfs_rename(const char *from, const char *to) {
	DBG("rename\n");

	int i = find_rw_root_cow_cutlast(from);
	if (i == -1) return -errno;
	
	if (!uopt.roots[i].rw) {
		i = find_rw_root_cow_cutlast(from);
		if (i == -1) return -errno;
		
		// since original file is on a read-only root, we copied the from file to a writable root, but since we will rename from, we also need to hide the from file on the read-only root
		hide_file(from, i);
	}

	char f[PATHLEN_MAX], t[PATHLEN_MAX];
	snprintf(f, PATHLEN_MAX, "%s%s", uopt.roots[i].path, from);
	snprintf(t, PATHLEN_MAX, "%s%s", uopt.roots[i].path, to);

	int res = rename(f, t);
	if (res == -1) {
		if (errno == ENOENT) {
			if (uopt.cache_enabled) cache_invalidate_path(from);

			i = find_rw_root_cow_cutlast(from);
			if (i == -1) return -errno;

			if (!uopt.roots[i].rw) {
				i = find_rw_root_cow_cutlast(from);
				if (i == -1) return -errno;
		
				hide_file(from, i);
			}
			
			snprintf(f, PATHLEN_MAX, "%s%s", uopt.roots[i].path, from);
			snprintf(t, PATHLEN_MAX, "%s%s", uopt.roots[i].path, to);

			res = rename(f, t);
			if (res == -1) return -errno;
		} else {
			return -errno;
		}
	}

	// The path should no longer exist
	if (uopt.cache_enabled) cache_invalidate_path(from);

	return 0;
}

static int unionfs_rmdir(const char *path) {
	DBG("rmdir\n");
	
	// FIXME: no proper cow support yet

	int i = find_rorw_root(path);
	if (i == -1) return -errno;

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

	int res = rmdir(p);
	if (res == -1) {
		if (errno == ENOENT) {
			if (uopt.cache_enabled) cache_invalidate_path(path);

			i = find_rorw_root(path);
			if (i == -1) return -errno;

			snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

			res = rmdir(p);
			if (res == -1) return -errno;
		} else {
			return -errno;
		}
	}

	// The path should no longer exist
	if (uopt.cache_enabled) cache_invalidate_path(path);

	return 0;
}

static int unionfs_statfs(const char *path, struct statvfs *stbuf) {
	(void)path;

	DBG("statfs\n");

	int first = 1;

	dev_t *devno = (dev_t *)malloc(sizeof(dev_t) * uopt.nroots);

	int i = 0;
	for (i = 0; i < uopt.nroots; i++) {
		struct statvfs stb;
		int res = statvfs(uopt.roots[i].path, &stb);
		if (res == -1) continue;

		struct stat st;
		res = stat(uopt.roots[i].path, &st);
		if (res == -1) continue;
		devno[i] = st.st_dev;

		if (first) {
			memcpy(stbuf, &stb, sizeof(*stbuf));
			first = 0;
		} else {
			// Eliminate same devices
			int j = 0;
			for (j = 0; j < i; j ++) {
				if (st.st_dev == devno[j]) break;
			}

			if (j == i) {
				// Filesystem can have different block sizes -> normalize to first's block size
				double ratio = (double)stb.f_bsize / (double)stbuf->f_bsize;

				stbuf->f_blocks += stb.f_blocks * ratio;
				stbuf->f_bfree += stb.f_bfree * ratio;
				stbuf->f_bavail += stb.f_bavail * ratio;

				stbuf->f_files += stb.f_files;
				stbuf->f_ffree += stb.f_ffree;
				stbuf->f_favail += stb.f_favail;

				if (!stb.f_flag & ST_RDONLY) stbuf->f_flag &= ~ST_RDONLY;
				if (!stb.f_flag & ST_NOSUID) stbuf->f_flag &= ~ST_NOSUID;

				if (stb.f_namemax < stbuf->f_namemax) stbuf->f_namemax = stb.f_namemax;
			}
		}
	}

	stbuf->f_fsid = 0;
	free(devno);

	return 0;
}

static int unionfs_symlink(const char *from, const char *to) {
	DBG("symlink\n");

	int i = find_rw_root_cow(to);
	if (i == -1) return -errno;

	char t[PATHLEN_MAX];
	snprintf(t, PATHLEN_MAX, "%s%s", uopt.roots[i].path, to);

	int res = symlink(from, t);
	if (res == -1) {
		if (errno == ENOENT) {
			if (uopt.cache_enabled) cache_invalidate_path(to);

			i = find_rw_root_cow(to);
			if (i == -1) return -errno;

			snprintf(t, PATHLEN_MAX, "%s%s", uopt.roots[i].path, to);

			res = symlink(from, t);
			if (res == -1) return -errno;
		} else {
			return -errno;
		}
	}

	return 0;
}

static int unionfs_truncate(const char *path, off_t size) {
	DBG("truncate\n");

	int i = find_rw_root_cow_cutlast(path);
	if (i == -1) return -errno;

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

	int res = truncate(p, size);
	if (res == -1) {
		if (errno == ENOENT) {
			if (uopt.cache_enabled) cache_invalidate_path(path);

			i = find_rw_root_cow_cutlast(path);
			if (i == -1) return -errno;

			snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

			res = truncate(p, size);
			if (res == -1) return -errno;
		} else {
			return -errno;
		}
	}

	return 0;
}

static int unionfs_utime(const char *path, struct utimbuf *buf) {
	DBG("utime\n");

	if (uopt.stats_enabled && strcmp(path, STATS_FILENAME) == 0) return 0;

	int i = find_rw_root_cow_cutlast(path);
	if (i == -1) return -errno;

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

	int res = utime(p, buf);
	if (res == -1) {
		if (errno == ENOENT) {
			if (uopt.cache_enabled) cache_invalidate_path(path);

			i = find_rw_root_cow_cutlast(path);
			if (i == -1) return -errno;

			snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

			res = utime(p, buf);
			if (res == -1) return -errno;
		} else {
			return -errno;
		}
	}

	return 0;
}

static int unionfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	(void)path;

	DBG("write\n");

	int res = pwrite(fi->fh, buf, size, offset);
	if (res == -1) return -errno;

	if (uopt.stats_enabled) stats_add_written(size);

	return res;
}

#ifdef HAVE_SETXATTR
static int unionfs_getxattr(const char *path, const char *name, char *value, size_t size) {
	DBG("getxattr\n");

	int i = find_rorw_root(path);
	if (i == -1) return -errno;

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", roots[i].path, path);

	int res = lgetxattr(p, name, value, size);
	if (res == -1) {
		if (errno == ENOENT) {
			if (uopt.cache_enabled) cache_invalidate_path(path);

			i = find_rorw_root(path);
			if (i == -1) return -errno;

			snprintf(p, PATHLEN_MAX, "%s%s", roots[i].path, path);

			res = lgetxattr(p, name, value, size);
			if (res == -1) return -errno;
		} else {
			return -errno;
		}
	}

	return 0;
}

static int unionfs_listxattr(const char *path, char *list, size_t size) {
	DBG("listxattr\n");

	int i = find_rorw_root(path);
	if (i == -1) return -errno;

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", roots[i].path, path);

	int res = llistxattr(p, list, size);
	if (res == -1) {
		if (errno == ENOENT) {
			if (uopt.cahce_enabled) cache_invalidate_path(path);

			i = find_rorw_root(path);
			if (i == -1) return -errno;

			snprintf(p, PATHLEN_MAX, "%s%s", roots[i].path, path);

			res = llistxattr(p, list, size);
			if (res == -1) return -errno;
		} else {
			return -errno;
		}
	}

	return 0;
}

static int unionfs_removexattr(const char *path, const char *name) {
	DBG("removexattr\n");
	
	int i = find_rw_root_cow_cutlast(path);
	if (i == -1) return -errno;

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", roots[i].path, path);

	int res = lremovexattr(p, name);
	if (res == -1) {
		if (errno == ENOENT) {
			if (uopt.cache_enabled) cache_invalidate_path(path);

			i = find_rw_root_cow_cutlast(path);
			if (i == -1) return -errno;

			snprintf(p, PATHLEN_MAX, "%s%s", roots[i].path, path);

			res = lremovexattr(p, name);
			if (res == -1) return -errno;
		} else {
			return -errno;
		}
	}

	return 0;
}

static int unionfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags) {
	DBG("sexattr\n");

	int i = find_rw_root_cow_cutlast(path);
	if (i == -1) return -errno;

	char p[PATHLEN_MAX];
	snprintf(p, PATHLEN_MAX, "%s%s", roots[i].path, path);

	int res = lsetxattr(p, name, value, size, flags);
	if (res == -1) {
		if (errno == ENOENT) {
			if (uopt.cache_enabled) cache_invalidate_path(path);

			i = find_rw_root_cow_cutlast(path);
			if (i == -1) return -errno;

			snprintf(p, PATHLEN_MAX, "%s%s", roots[i].path, path);

			res = lsetxattr(p, name, value, size, flags);
			if (res == -1) return -errno;
		} else {
			return -errno;
		}
	}

	return 0;
}
#endif // HAVE_SETXATTR

static struct fuse_operations unionfs_oper = {
	.access	= unionfs_access,
	.chmod	= unionfs_chmod,
	.chown	= unionfs_chown,
	.flush	= unionfs_flush,
	.fsync	= unionfs_fsync,
	.getattr	= unionfs_getattr,
	.link	= unionfs_link,
	.mkdir	= unionfs_mkdir,
	.mknod	= unionfs_mknod,
	.open	= unionfs_open,
	.read	= unionfs_read,
	.readlink	= unionfs_readlink,
	.readdir	= unionfs_readdir,
	.release	= unionfs_release,
	.rename	= unionfs_rename,
	.rmdir	= unionfs_rmdir,
	.statfs	= unionfs_statfs,
	.symlink	= unionfs_symlink,
	.truncate	= unionfs_truncate,
	.unlink	= unionfs_unlink,
	.utime	= unionfs_utime,
	.write	= unionfs_write,
#ifdef HAVE_SETXATTR
	.getxattr	= unionfs_getxattr,
	.listxattr	= unionfs_listxattr,
	.removexattr	= unionfs_removexattr,
	.setxattr	= unionfs_setxattr,
#endif
};

int main(int argc, char *argv[]) {
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	int res = debug_init();
	if (res != 0) return res;

	uopt_init();

	if (fuse_opt_parse(&args, NULL, unionfs_opts, unionfs_opt_proc) == -1) return 1;

	if (!uopt.doexit) {
		if (uopt.nroots == 0) {
			printf("You need to specify at least one root!\n");
			return 1;
		}

		if (uopt.stats_enabled) stats_init();
		if (uopt.cache_enabled) cache_init();
	}

	umask(0);
	return fuse_main(args.argc, args.argv, &unionfs_oper, NULL);
}
