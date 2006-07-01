/*
Written by Radek Podgorny

This is offered under a BSD-style license. This means you can use the code for whatever you desire in any way you may want but you MUST NOT forget to give me appropriate credits when spreading your work which is based on mine. Something like "original implementation by Radek Podgorny" should be fine.
*/

#ifdef linux
	/* For pread()/pwrite() */
	#define _XOPEN_SOURCE 500
#endif

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/statfs.h>

#ifdef HAVE_SETXATTR
	#include <sys/xattr.h>
#endif

#include "unionfs.h"
#include "cache.h"


int findroot(const char *path) {
	int i = 0;
	for (i = 0; i < nroots; i++) {
		char p[PATHLEN_MAX];
		strcpy(p, roots[i]);
		strcat(p, path);

		struct stat stbuf;
		int res = lstat(p, &stbuf);

		if (res == 0) return i;
	}

	return -1;
}

static int unionfs_getattr(const char *path, struct stat *stbuf) {
	if (stats && strcmp(path, "/stats") == 0) {
		memset(stbuf, 0, sizeof(stbuf));
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = STATS_SIZE;
		return 0;
	}

	int i = cache_lookup(path);
	if (i == -1) i = findroot(path);
	if (i == -1) return -errno;
	cache_save(path, i);

	char p[PATHLEN_MAX];
	strcpy(p, roots[i]);
	strcat(p, path);

	int res = lstat(p, stbuf);

	if (res == -1) {
		cache_invalidate(path);
		return unionfs_getattr(path, stbuf);
	}

	return 0;
}

static int unionfs_readlink(const char *path, char *buf, size_t size) {
	int i = cache_lookup(path);
	if (i == -1) i = findroot(path);
	if (i == -1) return -errno;
	cache_save(path, i);

	char p[PATHLEN_MAX];
	strcpy(p, roots[i]);
	strcat(p, path);

	int res = readlink(p, buf, size - 1);

	if (res == -1) {
		cache_invalidate(path);
		return unionfs_readlink(path, buf, size);
	}

	buf[res] = 0;

	return 0;
}


static int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;

	int nadded = 0;
	char **added;
	added = malloc(1);

	int i = 0;
	for (i = 0; i < nroots; i++) {
		char p[PATHLEN_MAX];
		strcpy(p, roots[i]);
		strcat(p, path);

		dp = opendir(p);
		if (dp == NULL) continue;

		while ((de = readdir(dp)) != NULL) {
			int j = 0;
			for (j = 0; j < nadded; j++)
				if (strcmp(added[j], de->d_name) == 0) break;
			if (j < nadded) continue;

			added = (char**)realloc(added, (nadded+1)*sizeof(char*));
			added[nadded] = malloc(strlen(de->d_name)+1);
			strcpy(added[nadded], de->d_name);
			nadded++;

			struct stat st;
			memset(&st, 0, sizeof(st));
			st.st_ino = de->d_ino;
			st.st_mode = de->d_type << 12;
			if (filler(buf, de->d_name, &st, 0)) break;
		}

		closedir(dp);
	}

	for (i = 0; i < nadded; i++) free(added[i]);
	free(added);

	if (stats && strcmp(path, "/") == 0) {
		filler(buf, "stats", NULL, 0);
	}

	return 0;
}

static int unionfs_mknod(const char *path, mode_t mode, dev_t rdev) {
	int res;

	int i = 0;
	for (i = 0; i < nroots; i++) {
		char p[PATHLEN_MAX];
		strcpy(p, roots[i]);
		strcat(p, path);

		res = mknod(p, mode, rdev);

		if (res == -1) {
			res = -errno; continue;
		} else {
			res = 0; break;
		}
	}

	return res;
}

/**/
static int unionfs_mkdir(const char *path, mode_t mode) {
	int res;

	res = mkdir(path, mode);
	if (res == -1) return -errno;

	return 0;
}

static int unionfs_unlink(const char *path) {
	int i = cache_lookup(path);
	if (i == -1) i = findroot(path);
	if (i == -1) return -errno;
	cache_save(path, i);

	char p[PATHLEN_MAX];
	strcpy(p, roots[i]);
	strcat(p, path);

	int res = unlink(p);

	if (res == -1) {
		cache_invalidate(path);
		return unionfs_unlink(path);
	}

	// The path should no longer exist
	cache_invalidate(path);

	return 0;
}

static int unionfs_rmdir(const char *path) {
	int i = cache_lookup(path);
	if (i == -1) i = findroot(path);
	if (i == -1) return -errno;
	cache_save(path, i);

	char p[PATHLEN_MAX];
	strcpy(p, roots[i]);
	strcat(p, path);

	int res = rmdir(p);

	if (res == -1) {
		cache_invalidate(path);
		return unionfs_rmdir(path);
	}

	// The path should no longer exist
	cache_invalidate(path);

	return 0;
}

/**/
static int unionfs_symlink(const char *from, const char *to) {
	int i = cache_lookup(from);
	if (i == -1) i = findroot(from);
	if (i == -1) return -errno;
	cache_save(from, i);

	char f[PATHLEN_MAX];
	strcpy(f, roots[i]);
	strcat(f, from);

	int res = symlink(f, to);
	if (res == -1) {
		cache_invalidate(from);
		return unionfs_symlink(from, to);
	}

	return 0;
}

static int unionfs_rename(const char *from, const char *to) {
	int i = cache_lookup(from);
	if (i == -1) i = findroot(from);
	if (i == -1) return -errno;
	cache_save(from, i);

	char f[PATHLEN_MAX];
	strcpy(f, roots[i]);
	strcat(f, from);

	char t[PATHLEN_MAX];
	strcpy(t, roots[i]);
	strcat(t, to);

	int res = rename(f, t);

	if (res == -1) {
		cache_invalidate(from);
		/* TODO: This is way too complicated so just fail */
		res = -errno;
	}

	// The path should no longer exist
	cache_invalidate(from);

	return 0;
}

/**/
static int unionfs_link(const char *from, const char *to) {
	int res;

	res = link(from, to);
	if (res == -1) return -errno;

	return 0;
}

static int unionfs_chmod(const char *path, mode_t mode) {
	int i = cache_lookup(path);
	if (i == -1) i = findroot(path);
	if (i == -1) return -errno;
	cache_save(path, i);

	char p[PATHLEN_MAX];
	strcpy(p, roots[i]);
	strcat(p, path);

	int res = chmod(p, mode);

	if (res == -1) {
		cache_invalidate(path);
		return unionfs_chmod(path, mode);
	}

	return 0;
}

static int unionfs_chown(const char *path, uid_t uid, gid_t gid) {
	int i = cache_lookup(path);
	if (i == -1) i = findroot(path);
	if (i == -1) return -errno;
	cache_save(path, i);

	char p[PATHLEN_MAX];
	strcpy(p, roots[i]);
	strcat(p, path);

	int res = lchown(p, uid, gid);

	if (res == -1) {
		cache_invalidate(path);
		return unionfs_chown(path, uid, gid);
	}

	return 0;
}

static int unionfs_truncate(const char *path, off_t size) {
	int i = cache_lookup(path);
	if (i == -1) i = findroot(path);
	if (i == -1) return -errno;
	cache_save(path, i);

	char p[PATHLEN_MAX];
	strcpy(p, roots[i]);
	strcat(p, path);

	int res = truncate(p, size);

	if (res == -1) {
		cache_invalidate(path);
		return unionfs_truncate(path, size);
	}

	return 0;
}

static int unionfs_utime(const char *path, struct utimbuf *buf) {
	int i = cache_lookup(path);
	if (i == -1) i = findroot(path);
	if (i == -1) return -errno;
	cache_save(path, i);

	char p[PATHLEN_MAX];
	strcpy(p, roots[i]);
	strcat(p, path);

	int res = utime(p, buf);

	if (res == -1) {
		cache_invalidate(path);
		return unionfs_utime(path, buf);
	}

	return 0;
}


static int unionfs_open(const char *path, struct fuse_file_info *fi) {
	if (stats && strcmp(path, "/stats") == 0) return 0;

	int i = cache_lookup(path);
	if (i == -1) i = findroot(path);
	if (i == -1) return -errno;
	cache_save(path, i);

	char p[PATHLEN_MAX];
	strcpy(p, roots[i]);
	strcat(p, path);

	int fd = open(p, fi->flags);

	if (fd == -1) {
		cache_invalidate(path);
		return unionfs_open(path, fi);
	}

	close(fd);

	return 0;
}

static int unionfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	if (stats && strcmp(path, "/stats") == 0) {
		char out[STATS_SIZE] = "";
		sprintf(out, "Cache hits/misses/ratio: %d/%d/%.3f\n", cache_hits, cache_misses, (double)cache_hits/(double)cache_misses);
		sprintf(out+strlen(out), "Bytes read/written: %.3fM/%.3fM\n", (double)bytes_read/1000000, (double)bytes_written/1000000);

		int s = size;
		if (offset < strlen(out)) {
			if (s > strlen(out)-offset) s = strlen(out)-offset;
			memcpy(buf, out+offset, s);
		} else
			s = 0;

		return s;
	}

	int i = cache_lookup(path);
	if (i == -1) i = findroot(path);
	if (i == -1) return -errno;
	cache_save(path, i);

	char p[PATHLEN_MAX];
	strcpy(p, roots[i]);
	strcat(p, path);

	int fd = open(p, O_RDONLY);

	if (fd == -1) {
		cache_invalidate(path);
		return unionfs_read(path, buf, size, offset, fi);
	}

	int res = pread(fd, buf, size, offset);
	if (res == -1) res = -errno;

	if (stats) bytes_read += size;

	close(fd);

	return res;
}

static int unionfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	int i = cache_lookup(path);
	if (i == -1) i = findroot(path);
	if (i == -1) return -errno;
	cache_save(path, i);

	char p[PATHLEN_MAX];
	strcpy(p, roots[i]);
	strcat(p, path);

	int fd = open(p, O_WRONLY);

	if (fd == -1) {
		cache_invalidate(path);
		return unionfs_write(path, buf, size, offset, fi);
	}

	int res = pwrite(fd, buf, size, offset);
	if (res == -1) res = -errno;

	if (stats) bytes_written += size;

	close(fd);

	return res;
}

/**/
static int unionfs_statfs(const char *path, struct statfs *stbuf) {
	int i = 0;
	for (i = 0; i < nroots; i++) {
		struct statfs stb;
		int res = statfs(roots[i], &stb);

		if (i == 0) {
			memcpy(stbuf, &stb, sizeof(stb));
		} else {
			stbuf->f_blocks += stb.f_blocks;
			stbuf->f_bfree += stb.f_bfree;
			stbuf->f_bavail += stb.f_bavail;
			stbuf->f_files += stb.f_files;
			stbuf->f_ffree += stb.f_ffree;

			if (stb.f_namelen < stbuf->f_namelen) stbuf->f_namelen = stb.f_namelen;
		}

		if (res == -1) continue;
	}

	return 0;
}

static int unionfs_release(const char *path, struct fuse_file_info *fi) {
	// Just a stub. This method is optional and can safely be left unimplemented

	(void) path;
	(void) fi;
	return 0;
}

static int unionfs_fsync(const char *path, int isdatasync, struct fuse_file_info *fi) {
	// Just a stub. This method is optional and can safely be left unimplemented

	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}

#ifdef HAVE_SETXATTR
// xattr operations are optional and can safely be left unimplemented
static int unionfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags) {
	int i = cache_lookup(path);
	if (i == -1) i = findroot(path);
	if (i == -1) return -errno;
	cache_save(path, i);

	char p[PATHLEN_MAX];
	strcpy(p, roots[i]);
	strcat(p, path);

	int res = lsetxattr(p, name, value, size, flags);

	if (res == -1) {
		cache_invalidate(path);
		return unionfs_setxattr(path, name, value, size, flags);
	}

	return 0;
}

static int unionfs_getxattr(const char *path, const char *name, char *value, size_t size) {
	int i = cache_lookup(path);
	if (i == -1) i = findroot(path);
	if (i == -1) return -errno;
	cache_save(path, i);

	char p[PATHLEN_MAX];
	strcpy(p, roots[i]);
	strcat(p, path);

	int res = lgetxattr(p, name, value, size);

	if (res == -1) {
		cache_invalidate(path);
		return unionfs_getxattr(path, name, value, size);
	}

	return 0;
}

static int unionfs_listxattr(const char *path, char *list, size_t size) {
	int i = cache_lookup(path);
	if (i == -1) i = findroot(path);
	if (i == -1) return -errno;
	cache_save(path, i);

	char p[PATHLEN_MAX];
	strcpy(p, roots[i]);
	strcat(p, path);

	int res = llistxattr(p, list, size);

	if (res == -1) {
		cache_invalidate(path);
		return unionfs_listxattr(path, list, size);
	}

	return 0;
}

static int unionfs_removexattr(const char *path, const char *name) {
	int i = cache_lookup(path);
	if (i == -1) i = findroot(path);
	if (i == -1) return -errno;
	cache_save(path, i);

	char p[PATHLEN_MAX];
	strcpy(p, roots[i]);
	strcat(p, path);

	int res = lremovexattr(p, name);

	if (res == -1) {
		cache_invalidate(path);
		return unionfs_removexattr(path, name);
	}

	return 0;
}
#endif /* HAVE_SETXATTR */

static struct fuse_operations unionfs_oper = {
	.getattr	= unionfs_getattr,
	.readlink	= unionfs_readlink,
	.readdir	= unionfs_readdir,
	.mknod	= unionfs_mknod,
//	.mkdir	= unionfs_mkdir,
//	.symlink	= unionfs_symlink,
	.unlink	= unionfs_unlink,
	.rmdir	= unionfs_rmdir,
	.rename	= unionfs_rename,
//	.link	= unionfs_link,
	.chmod	= unionfs_chmod,
	.chown	= unionfs_chown,
	.truncate	= unionfs_truncate,
	.utime	= unionfs_utime,
	.open	= unionfs_open,
	.read	= unionfs_read,
	.write	= unionfs_write,
	.statfs	= unionfs_statfs,
	.release	= unionfs_release,
	.fsync	= unionfs_fsync,
#ifdef HAVE_SETXATTR
	.setxattr	= unionfs_setxattr,
	.getxattr	= unionfs_getxattr,
	.listxattr	= unionfs_listxattr,
	.removexattr= unionfs_removexattr,
#endif
};

int main(int argc, char *argv[]) {
	printf("unionfs-fuse by Radek Podgorny\n");
	printf("version 0.9\n");

	stats = 0;
	bytes_read = bytes_written = 0;


	int argc_new = 0;
	char *argv_new[argc];

	int i = 0;
	for (i = 0; i < argc; i++) {
		if (strncmp(argv[i], "--roots=", strlen("--roots=")) == 0) {
			char tmp[strlen(argv[i])];
			strcpy(tmp, argv[i]+strlen("--roots="));

			while (strlen(tmp) > 0) {
				char *ri = rindex(tmp, ',');
				if (ri != NULL) {
					strcpy(roots[nroots++], ri+1);
					ri[0] = 0;
				} else {
					strcpy(roots[nroots++], tmp);
					tmp[0] = 0;
				}

				printf("root %d is %s\n", nroots, roots[nroots-1]);
			}
		} else if (strcmp(argv[i], "--stats") == 0) {
			stats = 1;
		} else {
			argv_new[argc_new++] = argv[i];
		}
	}

	if (nroots == 0) {
		printf("You need to specify at least one root!");
		return 1;
	}

	printf("Stats %s\n", stats?"enabled":"disabled");

	cache_init();

	umask(0);
	return fuse_main(argc_new, argv_new, &unionfs_oper);
}
