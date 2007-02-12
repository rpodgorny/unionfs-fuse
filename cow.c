#include <sys/types.h>
#include <unistd.h>
#include <syslog.h>
#include <locale.h>
#include <errno.h>
#include <stdio.h>

#include "opts.h"
#include "findbranch.h"
#include "cache.h"
#include "general.h"
#include "cow.h"


/**
 * initiate the cow-copy action
 */
static int cow_cp(const char *path, int root_ro, int root_rw) {
	char from[PATHLEN_MAX], to[PATHLEN_MAX];
	snprintf(from, PATHLEN_MAX, "%s%s", uopt.roots[root_ro].path, path);
	snprintf(to, PATHLEN_MAX, "%s%s", uopt.roots[root_rw].path, path);

	setlocale(LC_ALL, "");

	struct cow cow;

	cow.uid = getuid();

	// Copy the umask for explicit mode setting.
	cow.umask = umask(0);
	umask(cow.umask);

	cow.from_path = from;
	cow.to_path = to;

	struct stat buf;
	lstat(cow.from_path, &buf);
	cow.stat = &buf;

	switch (buf.st_mode & S_IFMT) {
		case S_IFLNK:
			return copy_link(&cow);
		case S_IFDIR:
			return path_create(path, root_ro, root_rw);
		case S_IFBLK:
		case S_IFCHR:
			return copy_special(&cow);
		case S_IFIFO:
			return copy_fifo(&cow);
		case S_IFSOCK:
			syslog(LOG_WARNING, "COW of sockets not supported: %s\n", cow.from_path);
			return 1;
		default:
			return copy_file(&cow);
	}	
}

/**
 * copy-one-write
 * Find path in a union branch and if this branch is read-only, 
 * copy the file to a read-write branch.
 */
int cow(const char *path) {
	int root_ro = findroot(path);

	// not found anywhere
	if (root_ro < 0) return -1;

	// the found root is writable, good!
	if (uopt.roots[root_ro].rw) return root_ro;

	// cow is disabled, return whatever was found
	if (!uopt.cow_enabled) return root_ro;

	int root_rw = wroot_from_list(root_ro);
	if (root_rw < 0) {
		// no writable root found
		errno = EACCES;
		return -1;
	}

	// create the path to the file
	path_create_cutlast(path, root_ro, root_rw);

	// copy the file from root_ro to root_rw
	if (cow_cp(path, root_ro, root_rw)) {
		// some error
		return -1;
	}

	// remove a file that might hide the copied file
	remove_hidden(path, root_rw);

	if (uopt.cache_enabled) {
		// update the cache
		cache_invalidate_path(path);
		cache_save(path, root_rw);
	}

	return root_rw;
}
