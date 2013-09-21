/*
*
* This is offered under a BSD-style license. This means you can use the code for whatever you
* desire in any way you may want but you MUST NOT forget to give me appropriate credits when
* spreading your work which is based on mine. Something like "original implementation by Radek
* Podgorny" should be fine.
*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*/

#ifdef linux
	// For pread()/pwrite()/utimensat()
	#define _XOPEN_SOURCE 700
	
	// For chroot
	#define _BSD_SOURCE
#endif

#include <fuse.h>
#include <fuse/fuse_common.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <inttypes.h>
#ifdef linux
	#include <sys/vfs.h>
#else
	#include <sys/statvfs.h>
#endif

#ifdef HAVE_XATTR
	#include <sys/xattr.h>
#endif

#include "unionfs.h"
#include "opts.h"
#include "stats.h"
#include "debug.h"
#include "findbranch.h"
#include "general.h"

#include "unlink.h"
#include "rmdir.h"
#include "readdir.h"
#include "cow.h"
#include "string.h"
#include "usyslog.h"

static struct fuse_opt unionfs_opts[] = {
	FUSE_OPT_KEY("chroot=%s,", KEY_CHROOT),
	FUSE_OPT_KEY("cow", KEY_COW),
	FUSE_OPT_KEY("-d", KEY_DEBUG),
	FUSE_OPT_KEY("debug_file=%s", KEY_DEBUG_FILE),
	FUSE_OPT_KEY("dirs=%s", KEY_DIRS),
	FUSE_OPT_KEY("--help", KEY_HELP),
	FUSE_OPT_KEY("-h", KEY_HELP),
	FUSE_OPT_KEY("hide_meta_dir", KEY_HIDE_METADIR),
	FUSE_OPT_KEY("hide_meta_files", KEY_HIDE_META_FILES),
	FUSE_OPT_KEY("max_files=%s", KEY_MAX_FILES),
	FUSE_OPT_KEY("noinitgroups", KEY_NOINITGROUPS),
	FUSE_OPT_KEY("relaxed_permissions", KEY_RELAXED_PERMISSIONS),
	FUSE_OPT_KEY("statfs_omit_ro", KEY_STATFS_OMIT_RO),
	FUSE_OPT_KEY("stats", KEY_STATS),
	FUSE_OPT_KEY("--version", KEY_VERSION),
	FUSE_OPT_KEY("-V", KEY_VERSION),
	FUSE_OPT_END
};


static int unionfs_chmod(const char *path, mode_t mode) {
	DBG("%s\n", path);

	int i = find_rw_branch_cow(path);
	if (i == -1) RETURN(-errno);

	char p[PATHLEN_MAX];
	if (BUILD_PATH(p, uopt.branches[i].path, path)) RETURN(-ENAMETOOLONG);

	int res = chmod(p, mode);
	if (res == -1) RETURN(-errno);

	RETURN(0);
}

static int unionfs_chown(const char *path, uid_t uid, gid_t gid) {
	DBG("%s\n", path);

	int i = find_rw_branch_cow(path);
	if (i == -1) RETURN(-errno);

	char p[PATHLEN_MAX];
	if (BUILD_PATH(p, uopt.branches[i].path, path)) RETURN(-ENAMETOOLONG);

	int res = lchown(p, uid, gid);
	if (res == -1) RETURN(-errno);

	RETURN(0);
}

/**
 * unionfs implementation of the create call
 * libfuse will call this to create regular files
 */
static int unionfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
	DBG("%s\n", path);

	int i = find_rw_branch_cutlast(path);
	if (i == -1) RETURN(-errno);

	char p[PATHLEN_MAX];
	if (BUILD_PATH(p, uopt.branches[i].path, path)) RETURN(-ENAMETOOLONG);

	// NOTE: We should do:
	//       Create the file with mode=0 first, otherwise we might create
	//       a file as root + x-bit + suid bit set, which might be used for
	//       security racing!
	int res = open(p, fi->flags, 0);
	if (res == -1) RETURN(-errno);

	set_owner(p); // no error check, since creating the file succeeded

	// NOW, that the file has the proper owner we may set the requested mode
	fchmod(res, mode);

	fi->fh = res;
	remove_hidden(path, i);

	DBG("fd = %" PRIx64 "\n", fi->fh);
	RETURN(0);
}


/**
 * flush may be called multiple times for an open file, this must not really
 * close the file. This is important if used on a network filesystem like NFS
 * which flush the data/metadata on close()
 */
static int unionfs_flush(const char *path, struct fuse_file_info *fi) {
	DBG("fd = %"PRIx64"\n", fi->fh);

	if (uopt.stats_enabled && strcmp(path, STATS_FILENAME) == 0) RETURN(0);

	int fd = dup(fi->fh);

	if (fd == -1) {
		// What to do now?
		if (fsync(fi->fh) == -1) RETURN(-EIO);

		RETURN(-errno);
	}

	int res = close(fd);
	if (res == -1) RETURN(-errno);

	RETURN(0);
}

/**
 * Just a stub. This method is optional and can safely be left unimplemented
 */
static int unionfs_fsync(const char *path, int isdatasync, struct fuse_file_info *fi) {
	DBG("fd = %"PRIx64"\n", fi->fh);

	if (uopt.stats_enabled && strcmp(path, STATS_FILENAME) == 0) RETURN(0);

	int res;
	if (isdatasync) {
#if _POSIX_SYNCHRONIZED_IO + 0 > 0
		res = fdatasync(fi->fh);
#else
		res = fsync(fi->fh);
#endif
	} else {
		res = fsync(fi->fh);
	}

	if (res == -1)  RETURN(-errno);

	RETURN(0);
}

static int unionfs_getattr(const char *path, struct stat *stbuf) {
	DBG("%s\n", path);

	if (uopt.stats_enabled && strcmp(path, STATS_FILENAME) == 0) {
		memset(stbuf, 0, sizeof(*stbuf));
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = STATS_SIZE;
		RETURN(0);
	}
	
	int i = find_rorw_branch(path);
	if (i == -1) RETURN(-errno);

	char p[PATHLEN_MAX];
	if (BUILD_PATH(p, uopt.branches[i].path, path)) RETURN(-ENAMETOOLONG);

	int res = lstat(p, stbuf);
	if (res == -1) RETURN(-errno);

	/* This is a workaround for broken gnu find implementations. Actually,
	 * n_links is not defined at all for directories by posix. However, it
	 * seems to be common for filesystems to set it to one if the actual value
	 * is unknown. Since nlink_t is unsigned and since these broken implementations
	 * always substract 2 (for . and ..) this will cause an underflow, setting
	 * it to max(nlink_t).
	 */
	if (S_ISDIR(stbuf->st_mode)) stbuf->st_nlink = 1;

	RETURN(0);
}

/**
 * init method
 * called before first access to the filesystem
 */
static void * unionfs_init(struct fuse_conn_info *conn) {
	// just to prevent the compiler complaining about unused variables
	(void) conn->max_readahead;

	// we only now (from unionfs_init) may go into the chroot, since otherwise
	// fuse_main() will fail to open /dev/fuse and to call mount
	if (uopt.chroot) {
		int res = chroot(uopt.chroot);
		if (res) {
			USYSLOG(LOG_WARNING, "Chdir to %s failed: %s ! Aborting!\n",
				 uopt.chroot, strerror(errno));
			exit(1);
		}
	}
	return NULL;
}

static int unionfs_link(const char *from, const char *to) {
	DBG("from %s to %s\n", from, to);

	// hardlinks do not work across different filesystems so we need a copy of from first
	int i = find_rw_branch_cow(from);
	if (i == -1) RETURN(-errno);

	int j = __find_rw_branch_cutlast(to, i);
	if (j == -1) RETURN(-errno);

	DBG("from branch: %d to branch: %d\n", i, j);

	char f[PATHLEN_MAX], t[PATHLEN_MAX];
	if (BUILD_PATH(f, uopt.branches[i].path, from)) RETURN(-ENAMETOOLONG);
	if (BUILD_PATH(t, uopt.branches[j].path, to)) RETURN(-ENAMETOOLONG);

	int res = link(f, t);
	if (res == -1) RETURN(-errno);

	// no need for set_owner(), since owner and permissions are copied over by link()

	remove_hidden(to, i); // remove hide file (if any)
	RETURN(0);
}

/**
 * unionfs mkdir() implementation
 *
 * NOTE: Never delete whiteouts directories here, since this will just
 *       make already hidden sub-branches visible again.
 */
static int unionfs_mkdir(const char *path, mode_t mode) {
	DBG("%s\n", path);

	int i = find_rw_branch_cutlast(path);
	if (i == -1) RETURN(-errno);

	char p[PATHLEN_MAX];
	if (BUILD_PATH(p, uopt.branches[i].path, path)) RETURN(-ENAMETOOLONG);

	int res = mkdir(p, 0);
	if (res == -1) RETURN(-errno);

	set_owner(p); // no error check, since creating the file succeeded
        // NOW, that the file has the proper owner we may set the requested mode
        chmod(p, mode);

	RETURN(0);
}

static int unionfs_mknod(const char *path, mode_t mode, dev_t rdev) {
	DBG("%s\n", path);

	int i = find_rw_branch_cutlast(path);
	if (i == -1) RETURN(-errno);

	char p[PATHLEN_MAX];
	if (BUILD_PATH(p, uopt.branches[i].path, path)) RETURN(-ENAMETOOLONG);

	int file_type = mode & S_IFMT;
	int file_perm = mode & (S_PROT_MASK);

	int res = -1;
	if ((file_type) == S_IFREG) {
		// under FreeBSD, only the super-user can create ordinary files using mknod
		// Actually this workaround should not be required any more
		// since we now have the unionfs_create() method
		// So can we remove it?

		USYSLOG (LOG_INFO, "deprecated mknod workaround, tell the unionfs-fuse authors if you see this!\n");

		res = creat(p, 0);
		if (res > 0 && close(res) == -1) USYSLOG(LOG_WARNING, "Warning, cannot close file\n");
	} else {
		res = mknod(p, file_type, rdev);
	}

	if (res == -1) RETURN(-errno);

	set_owner(p); // no error check, since creating the file succeeded
	// NOW, that the file has the proper owner we may set the requested mode
	chmod(p, file_perm);

	remove_hidden(path, i);

	RETURN(0);
}

static int unionfs_open(const char *path, struct fuse_file_info *fi) {
	DBG("%s\n", path);

	if (uopt.stats_enabled && strcmp(path, STATS_FILENAME) == 0) {
		if ((fi->flags & 3) == O_RDONLY) {
			// This makes exec() fail
			//fi->direct_io = 1;
			RETURN(0);
		}
		RETURN(-EACCES);
	}

	int i;
	if (fi->flags & (O_WRONLY | O_RDWR)) {
		i = find_rw_branch_cutlast(path);
	} else {
		i = find_rorw_branch(path);
	}

	if (i == -1) RETURN(-errno);

	char p[PATHLEN_MAX];
	if (BUILD_PATH(p, uopt.branches[i].path, path)) RETURN(-ENAMETOOLONG);

	int fd = open(p, fi->flags);
	if (fd == -1) RETURN(-errno);

	if (fi->flags & (O_WRONLY | O_RDWR)) {
		// There might have been a hide file, but since we successfully
		// wrote to the real file, a hide file must not exist anymore
		remove_hidden(path, i);
	}

	// This makes exec() fail
	//fi->direct_io = 1;
	fi->fh = (unsigned long)fd;

	DBG("fd = %"PRIx64"\n", fi->fh);
	RETURN(0);
}

static int unionfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	DBG("fd = %"PRIx64"\n", fi->fh);

	if (uopt.stats_enabled && strcmp(path, STATS_FILENAME) == 0) {
		char out[STATS_SIZE] = "";
		stats_sprint(&stats, out);

		off_t s = size;
		if (offset < (off_t) strlen(out)) {
			if (s > (off_t) strlen(out) - offset) s = strlen(out)-offset;
			memcpy(buf, out+offset, s);
		} else {
			s = 0;
		}

		return s;
	}

	int res = pread(fi->fh, buf, size, offset);

	if (res == -1) RETURN(-errno);

	if (uopt.stats_enabled) stats_add_read(&stats, size);

	RETURN(res);
}

static int unionfs_readlink(const char *path, char *buf, size_t size) {
	DBG("%s\n", path);

	int i = find_rorw_branch(path);
	if (i == -1) RETURN(-errno);

	char p[PATHLEN_MAX];
	if (BUILD_PATH(p, uopt.branches[i].path, path)) RETURN(-ENAMETOOLONG);

	int res = readlink(p, buf, size - 1);

	if (res == -1) RETURN(-errno);

	buf[res] = '\0';

	RETURN(0);
}

static int unionfs_release(const char *path, struct fuse_file_info *fi) {
	DBG("fd = %"PRIx64"\n", fi->fh);

	if (uopt.stats_enabled && strcmp(path, STATS_FILENAME) == 0) RETURN(0);

	int res = close(fi->fh);
	if (res == -1) RETURN(-errno);

	RETURN(0);
}

/**
 * unionfs rename function
 * TODO: If we rename a directory on a read-only branch, we need to copy over
 *       all files to the renamed directory on the read-write branch.
 */
static int unionfs_rename(const char *from, const char *to) {
	DBG("from %s to %s\n", from, to);

	bool is_dir = false; // is 'from' a file or directory

	int j = find_rw_branch_cutlast(to);
	if (j == -1) RETURN(-errno);

	int i = find_rorw_branch(from);
	if (i == -1) RETURN(-errno);

	if (!uopt.branches[i].rw) {
		i = find_rw_branch_cow(from);
		if (i == -1) RETURN(-errno);
	}

	if (i != j) {
		USYSLOG(LOG_ERR, "%s: from and to are on different writable branches %d vs %d, which"
		       "is not supported yet.\n", __func__, i, j);
		RETURN(-EXDEV);
	}

	char f[PATHLEN_MAX], t[PATHLEN_MAX];
	if (BUILD_PATH(f, uopt.branches[i].path, from)) RETURN(-ENAMETOOLONG);
	if (BUILD_PATH(t, uopt.branches[i].path, to)) RETURN(-ENAMETOOLONG);

	filetype_t ftype = path_is_dir(f);
	if (ftype == NOT_EXISTING)
		RETURN(-ENOENT);
	else if (ftype == IS_DIR)
		is_dir = true;

	int res;
	if (!uopt.branches[i].rw) {
		// since original file is on a read-only branch, we copied the from file to a writable branch,
		// but since we will rename from, we also need to hide the from file on the read-only branch
		if (is_dir)
			res = hide_dir(from, i);
		else
			res = hide_file(from, i);
		if (res) RETURN(-errno);
	}

	res = rename(f, t);

	if (res == -1) {
		int err = errno; // unlink() might overwrite errno
		// if from was on a read-only branch we copied it, but now rename failed so we need to delete it
		if (!uopt.branches[i].rw) {
			if (unlink(f))
				USYSLOG(LOG_ERR, "%s: cow of %s succeeded, but rename() failed and now "
				       "also unlink()  failed\n", __func__, from);

			if (remove_hidden(from, i))
				USYSLOG(LOG_ERR, "%s: cow of %s succeeded, but rename() failed and now "
				       "also removing the whiteout  failed\n", __func__, from);
		}
		RETURN(-err);
	}

	if (uopt.branches[i].rw) {
		// A lower branch still *might* have a file called 'from', we need to delete this.
		// We only need to do this if we have been on a rw-branch, since we created
		// a whiteout for read-only branches anyway.
		if (is_dir)
			maybe_whiteout(from, i, WHITEOUT_DIR);
		else
			maybe_whiteout(from, i, WHITEOUT_FILE);
	}

	remove_hidden(to, i); // remove hide file (if any)
	RETURN(0);
}

/**
 * Wrapper function to convert the result of statfs() to statvfs()
 * libfuse uses statvfs, since it conforms to POSIX. Unfortunately,
 * glibc's statvfs parses /proc/mounts, which then results in reading
 * the filesystem itself again - which would result in a deadlock.
 * TODO: BSD/MacOSX
 */
static int statvfs_local(const char *path, struct statvfs *stbuf) {
#ifdef linux
	/* glibc's statvfs walks /proc/mounts and stats entries found there
	 * in order to extract their mount flags, which may deadlock if they
	 * are mounted under the unionfs. As a result, we have to do this
	 * ourselves.
	 */
	struct statfs stfs;
	int res = statfs(path, &stfs);
	if (res == -1) RETURN(res);

	memset(stbuf, 0, sizeof(*stbuf));
	stbuf->f_bsize = stfs.f_bsize;
	if (stfs.f_frsize)
		stbuf->f_frsize = stfs.f_frsize;
	else
		stbuf->f_frsize = stfs.f_bsize;
	stbuf->f_blocks = stfs.f_blocks;
	stbuf->f_bfree = stfs.f_bfree;
	stbuf->f_bavail = stfs.f_bavail;
	stbuf->f_files = stfs.f_files;
	stbuf->f_ffree = stfs.f_ffree;
	stbuf->f_favail = stfs.f_ffree; /* nobody knows */

	/* We don't worry about flags, exactly because this would
	 * require reading /proc/mounts, and avoiding that and the
	 * resulting deadlocks is exactly what we're trying to avoid
	 * by doing this rather than using statvfs.
	 */
	stbuf->f_flag = 0;
	stbuf->f_namemax = stfs.f_namelen;

	RETURN(0);
#else
	RETURN(statvfs(path, stbuf));
#endif
}



/**
 * statvs implementation
 *
 * Note: We do not set the fsid, as fuse ignores it anyway.
 */
static int unionfs_statfs(const char *path, struct statvfs *stbuf) {
	(void)path;

	DBG("%s\n", path);

	int first = 1;

	dev_t devno[uopt.nbranches];

	int i = 0;
	for (i = 0; i < uopt.nbranches; i++) {
		struct statvfs stb;
		int res = statvfs_local(uopt.branches[i].path, &stb);
		if (res == -1) continue;

		struct stat st;
		res = stat(uopt.branches[i].path, &st);
		if (res == -1) continue;
		devno[i] = st.st_dev;

		if (first) {
			memcpy(stbuf, &stb, sizeof(*stbuf));
			first = 0;
			stbuf->f_fsid = stb.f_fsid << 8;
			continue;
		}

		// Eliminate same devices
		int j = 0;
		for (j = 0; j < i; j ++) {
			if (st.st_dev == devno[j]) break;
		}

		if (j == i) {
			// Filesystem can have different block sizes -> normalize to first's block size
			double ratio = (double)stb.f_bsize / (double)stbuf->f_bsize;

			if (uopt.branches[i].rw) {
				stbuf->f_blocks += stb.f_blocks * ratio;
				stbuf->f_bfree += stb.f_bfree * ratio;
				stbuf->f_bavail += stb.f_bavail * ratio;

				stbuf->f_files += stb.f_files;
				stbuf->f_ffree += stb.f_ffree;
				stbuf->f_favail += stb.f_favail;
			} else if (!uopt.statfs_omit_ro) {
				// omitting the RO branches is not correct regarding
				// the block counts but it actually fixes the 
				// percentage of free space. so, let the user decide.
				stbuf->f_blocks += stb.f_blocks * ratio;
				stbuf->f_files  += stb.f_files;
			}

			if (!(stb.f_flag & ST_RDONLY)) stbuf->f_flag &= ~ST_RDONLY;
			if (!(stb.f_flag & ST_NOSUID)) stbuf->f_flag &= ~ST_NOSUID;

			if (stb.f_namemax < stbuf->f_namemax) stbuf->f_namemax = stb.f_namemax;
		}
	}

	RETURN(0);
}

static int unionfs_symlink(const char *from, const char *to) {
	DBG("from %s to %s\n", from, to);

	int i = find_rw_branch_cutlast(to);
	if (i == -1) RETURN(-errno);

	char t[PATHLEN_MAX];
	if (BUILD_PATH(t, uopt.branches[i].path, to)) RETURN(-ENAMETOOLONG);

	int res = symlink(from, t);
	if (res == -1) RETURN(-errno);

	set_owner(t); // no error check, since creating the file succeeded

	remove_hidden(to, i); // remove hide file (if any)
	RETURN(0);
}

static int unionfs_truncate(const char *path, off_t size) {
	DBG("%s\n", path);

	int i = find_rw_branch_cow(path);
	if (i == -1) RETURN(-errno);

	char p[PATHLEN_MAX];
	if (BUILD_PATH(p, uopt.branches[i].path, path)) RETURN(-ENAMETOOLONG);

	int res = truncate(p, size);

	if (res == -1) RETURN(-errno);

	RETURN(0);
}

static int unionfs_utimens(const char *path, const struct timespec ts[2]) {
	DBG("%s\n", path);

	if (uopt.stats_enabled && strcmp(path, STATS_FILENAME) == 0) RETURN(0);

	int i = find_rw_branch_cow(path);
	if (i == -1) RETURN(-errno);

	char p[PATHLEN_MAX];
	if (BUILD_PATH(p, uopt.branches[i].path, path)) RETURN(-ENAMETOOLONG);

	int res = utimensat(0, p, ts, AT_SYMLINK_NOFOLLOW);

	if (res == -1) RETURN(-errno);

	RETURN(0);
}

static int unionfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	(void)path;

	DBG("fd = %"PRIx64"\n", fi->fh);

	int res = pwrite(fi->fh, buf, size, offset);
	if (res == -1) RETURN(-errno);

	if (uopt.stats_enabled) stats_add_written(&stats, size);

	RETURN(res);
}

#ifdef HAVE_XATTR
static int unionfs_getxattr(const char *path, const char *name, char *value, size_t size) {
	DBG("%s\n", path);

	int i = find_rorw_branch(path);
	if (i == -1) RETURN(-errno);

	char p[PATHLEN_MAX];
	if (BUILD_PATH(p, uopt.branches[i].path, path)) RETURN(-ENAMETOOLONG);

	int res = lgetxattr(p, name, value, size);

	if (res == -1) RETURN(-errno);

	RETURN(res);
}

static int unionfs_listxattr(const char *path, char *list, size_t size) {
	DBG("%s\n", path);

	int i = find_rorw_branch(path);
	if (i == -1) RETURN(-errno);

	char p[PATHLEN_MAX];
	if (BUILD_PATH(p, uopt.branches[i].path, path)) RETURN(-ENAMETOOLONG);

	int res = llistxattr(p, list, size);

	if (res == -1) RETURN(-errno);

	RETURN(res);
}

static int unionfs_removexattr(const char *path, const char *name) {
	DBG("%s\n", path);

	int i = find_rw_branch_cow(path);
	if (i == -1) RETURN(-errno);

	char p[PATHLEN_MAX];
	if (BUILD_PATH(p, uopt.branches[i].path, path)) RETURN(-ENAMETOOLONG);

	int res = lremovexattr(p, name);

	if (res == -1) RETURN(-errno);

	RETURN(res);
}

static int unionfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags) {
	DBG("%s\n", path);

	int i = find_rw_branch_cow(path);
	if (i == -1) RETURN(-errno);

	char p[PATHLEN_MAX];
	if (BUILD_PATH(p, uopt.branches[i].path, path)) RETURN(-ENAMETOOLONG);

	int res = lsetxattr(p, name, value, size, flags);

	if (res == -1) RETURN(-errno);

	RETURN(res);
}
#endif // HAVE_XATTR

static struct fuse_operations unionfs_oper = {
	.chmod	= unionfs_chmod,
	.chown	= unionfs_chown,
	.create 	= unionfs_create,
	.flush	= unionfs_flush,
	.fsync	= unionfs_fsync,
	.getattr	= unionfs_getattr,
	.init		= unionfs_init,
	.link		= unionfs_link,
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
	.utimens	= unionfs_utimens,
	.write	= unionfs_write,
#ifdef HAVE_XATTR
	.getxattr		= unionfs_getxattr,
	.listxattr		= unionfs_listxattr,
	.removexattr	= unionfs_removexattr,
	.setxattr		= unionfs_setxattr,
#endif
};

int main(int argc, char *argv[]) {
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	init_syslog();
	uopt_init();

	if (fuse_opt_parse(&args, NULL, unionfs_opts, unionfs_opt_proc) == -1) RETURN(1);

	if (uopt.debug)	debug_init();

	if (!uopt.doexit) {
		if (uopt.nbranches == 0) {
			printf("You need to specify at least one branch!\n");
			RETURN(1);
		}

		if (uopt.stats_enabled) stats_init(&stats);
	}

	// enable fuse permission checks, we need to set this, even we we are
	// not root, since we don't have our own access() function
	int uid = getuid();
	int gid = getgid();
	bool default_permissions = true;
	
	if (uid != 0 && gid != 0 && uopt.relaxed_permissions) {
		default_permissions = false;
	} else if (uopt.relaxed_permissions) {
		// protec the user of a very critical security issue
		fprintf(stderr, "Relaxed permissions disallowed for root!\n");
		exit(1);
	}
	
	if (default_permissions) {
		if (fuse_opt_add_arg(&args, "-odefault_permissions")) {
			fprintf(stderr, "Severe failure, can't enable permssion checks, aborting!\n");
			exit(1);
		}
	}
	unionfs_post_opts();

#ifdef FUSE_CAP_BIG_WRITES
	/* libfuse > 0.8 supports large IO, also for reads, to increase performance 
	 * We support any IO sizes, so lets enable that option */
	if (fuse_opt_add_arg(&args, "-obig_writes")) {
		fprintf(stderr, "Failed to enable big writes!\n");
		exit(1);
	}
#endif

	umask(0);
	int res = fuse_main(args.argc, args.argv, &unionfs_oper, NULL);
	RETURN(uopt.doexit ? uopt.retval : res);
}
