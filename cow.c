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
#include "cow_utils.h"


/**
 * initiate the cow-copy action
 */
int cow_cp(const char *path, int root_ro, int root_rw) {
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
