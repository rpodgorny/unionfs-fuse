/*
*  C Implementation: opts.c
*
* Option parser
*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>

#include "conf.h"
#include "opts.h"
#include "version.h"
#include "string.h"


/**
 * Set debug path
 */
void set_debug_path(char *new_path, int len) {
	pthread_rwlock_wrlock(&uopt.dbgpath_lock); // LOCK path

	if (uopt.dbgpath) free(uopt.dbgpath);

	uopt.dbgpath = strndup(new_path, len);

	pthread_rwlock_unlock(&uopt.dbgpath_lock); // UNLOCK path
}


/**
 * Check if a debug path is set
 */
static bool get_has_debug_path(void) {
	pthread_rwlock_rdlock(&uopt.dbgpath_lock); // LOCK path

	bool has_debug_path = (uopt.dbgpath) ? true : false;

	pthread_rwlock_unlock(&uopt.dbgpath_lock); // UNLOCK path

	return has_debug_path;
}

/**
 * Enable or disable internal debugging
 */
bool set_debug_onoff(int value) {
	bool res = false;

	if (value) {
		bool has_debug_path = get_has_debug_path();
		if (has_debug_path) {
			uopt.debug = 1;
			res = true;
		}
	} else {
		uopt.debug = 0;
		res = true;
	}

	return res;
}


/**
 * Set the maximum number of open files
 */
int set_max_open_files(const char *arg) {
	struct rlimit rlim;
	unsigned long max_files;
	if (sscanf(arg, "max_files=%ld\n", &max_files) != 1) {
		fprintf(stderr, "%s Converting %s to number failed, aborting!\n",
			__func__, arg);
		exit(1);
	}
	rlim.rlim_cur = max_files;
	rlim.rlim_max = max_files;
	if (setrlimit(RLIMIT_NOFILE, &rlim)) {
		fprintf(stderr, "%s: Setting the maximum number of files failed: %s\n",
			__func__, strerror(errno));
		exit(1);
	}

	return 0;
}


uopt_t uopt;

void uopt_init() {
	memset(&uopt, 0, sizeof(uopt_t)); // initialize options with zeros first

	pthread_rwlock_init(&uopt.dbgpath_lock, NULL);
}

/**
 * Take a relative path as argument and return the absolute path by using the
 * current working directory. The return string is malloc'ed with this function.
 */
char *make_absolute(char *relpath) {
	// Already an absolute path
	if (*relpath == '/') return relpath;

	char cwd[PATHLEN_MAX];
	if (!getcwd(cwd, PATHLEN_MAX)) {
		perror("Unable to get current working directory");
		return NULL;
	}

	size_t cwdlen = strlen(cwd);
	if (!cwdlen) {
		fprintf(stderr, "Zero-sized length of CWD!\n");
		return NULL;
	}

	// 2 due to: +1 for '/' between cwd and relpath
	//           +1 for trailing '/'
	int abslen = cwdlen + strlen(relpath) + 2;
	if (abslen > PATHLEN_MAX) {
		fprintf(stderr, "Absolute path too long!\n");
		return NULL;
	}

	char *abspath = malloc(abslen);
	if (abspath == NULL) {
		fprintf(stderr, "%s: malloc failed\n", __func__);
		exit(1); // still at early stage, we can abort
	}

	// the ending required slash is added later by add_trailing_slash()
	snprintf(abspath, abslen, "%s/%s", cwd, relpath);

	return abspath;
}

/**
 * Add a trailing slash at the end of a branch. So functions using this
 * path don't have to care about this slash themselves.
 **/
char *add_trailing_slash(char *path) {
	int len = strlen(path);
	if (path[len - 1] == '/') {
		return path; // no need to add a slash, already there
	}

	path = realloc(path, len + 2); // +1 for '/' and +1 for '\0'
	if (!path) {
		fprintf(stderr, "%s: realloc() failed, aborting\n", __func__);
		exit(1); // still very early stage, we can abort here
	}

	strcat(path, "/");
	return path;
}

/**
 * Add a given branch and its options to the array of available branches.
 * example branch string "branch1=RO" or "/path/path2=RW"
 */
void add_branch(char *branch) {
	uopt.branches = realloc(uopt.branches, (uopt.nbranches+1) * sizeof(branch_entry_t));
	if (uopt.branches == NULL) {
		fprintf(stderr, "%s: realloc failed\n", __func__);
		exit(1); // still at early stage, we can't abort
	}

	char *res;
	char **ptr = (char **)&branch;

	res = strsep(ptr, "=");
	if (!res) return;

	// for string manipulations it is important to copy the string, otherwise
	// make_absolute() and add_trailing_slash() will corrupt our input (parse string)
	uopt.branches[uopt.nbranches].path = strdup(res);
	uopt.branches[uopt.nbranches].rw = 0;

	res = strsep(ptr, "=");
	if (res) {
		if (strcasecmp(res, "rw") == 0) {
			uopt.branches[uopt.nbranches].rw = 1;
		} else if (strcasecmp(res, "ro") == 0) {
			// no action needed here
		} else {
			fprintf(stderr, "Failed to parse RO/RW flag, setting RO.\n");
			// no action needed here either
		}
	}

	uopt.nbranches++;
}

/**
 * These options define our branch paths.
 * example arg string: "branch1=RW:branch2=RO:branch3=RO"
 */
int parse_branches(const char *arg) {
	// the last argument is  our mountpoint, don't take it as branch!
	if (uopt.nbranches) return 0;

	// We don't free the buf as parts of it may go to branches
	char *buf = strdup(arg);
	char **ptr = (char **)&buf;
	char *branch;
	while ((branch = strsep(ptr, ROOT_SEP)) != NULL) {
		if (strlen(branch) == 0) continue;

		add_branch(branch);
	}

	free(branch);
	free(buf);

	return uopt.nbranches;
}

/**
  * get_opt_str - get the parameter string
  * @arg	- option argument
  * @opt_name	- option name, used for error messages
  * fuse passes arguments with the argument prefix, e.g.
  * "-o chroot=/path/to/chroot/" will give us "chroot=/path/to/chroot/"
  * and we need to cut off the "chroot=" part
  * NOTE: If the user specifies a relative path of the branches
  *       to the chroot, it is absolutely required
  *       -o chroot=path is provided before specifying the braches!
  */
static char * get_opt_str(const char *arg, char *opt_name) {
	char *str = index(arg, '=');

	if (!str) {
		fprintf(stderr, "-o %s parameter not properly specified, aborting!\n",
		        opt_name);
		exit(1); // still early phase, we can abort
	}

	if (strlen(str) < 3) {
		fprintf(stderr, "%s path has not sufficient characters, aborting!\n",
		        opt_name);
		exit(1);
	}

	str++; // just jump over the '='

	// copy of the given parameter, just in case something messes around
	// with command line parameters later on
	str = strdup(str);
	if (!str) {
		fprintf(stderr, "strdup failed: %s Aborting!\n", strerror(errno));
		exit(1);
	}
	return str;
}

static void print_help(const char *progname) {
	printf(
	"unionfs-fuse version "VERSION"\n"
	"by Radek Podgorny <radek@podgorny.cz>\n"
	"\n"
	"Usage: %s [options] branch[=RO/RW][:branch...] mountpoint\n"
	"The first argument is a colon separated list of directories to merge\n"
	"When neither RO nor RW is specified, selection defaults to RO.\n"
	"\n"
	"general options:\n"
	"    -d                     Enable debug output\n"
	"    -o opt,[opt...]        mount options\n"
	"    -h   --help            print help\n"
	"    -V   --version         print version\n"
	"\n"
	"UnionFS options:\n"
	"    -o chroot=path         chroot into this path. Use this if you \n"
	"                           want to have a union of \"/\" \n"
	"    -o cow                 enable copy-on-write\n"
	"                           mountpoint\n"
	"    -o debug_file=<fn>     file to write debug information into\n"
	"    -o dirs=branch[=RO/RW][:branch...]\n"
	"                           alternate way to specify directories to merge\n"
	"    -o hide_meta_files     \".unionfs\" is a secret directory not\n"
	"                           visible by readdir(), and so are\n"
	"                           .fuse_hidden* files\n"
	"    -o max_files=number    Increase the maximum number of open files\n"
	"    -o relaxed_permissions Disable permissions checks, but only if\n"
	"                           running neither as UID=0 or GID=0\n"
	"    -o statfs_omit_ro      do not count blocks of ro-branches\n"
	"    -o direct_io           Enables direct io\n"
	"\n",
	progname);
}

/**
  * This method is to post-process options once we know all of them
  */
void unionfs_post_opts(void) {
	// chdir to the given chroot, we
	if (uopt.chroot) {
		int res = chdir(uopt.chroot);
		if (res) {
			fprintf(stderr, "Chdir to %s failed: %s ! Aborting!\n",
				  uopt.chroot, strerror(errno));
			exit(1);
		}
	}

	// Make the pathes absolute and add trailing slashes
	int i;
	for (i = 0; i < uopt.nbranches; i++) {
		// if -ochroot= is specified, the path has to be given absolute
		// or relative to the chroot, so no need to make it absolute
		// also won't work, since we are not yet in the chroot here
		if (!uopt.chroot) {
			uopt.branches[i].path = make_absolute(uopt.branches[i].path);
		}
		uopt.branches[i].path = add_trailing_slash(uopt.branches[i].path);

		// Prevent accidental umounts. Especially system shutdown scripts tend
		// to umount everything they can. If we don't have an open file descriptor,
		// this might cause unexpected behaviour.
		char path[PATHLEN_MAX];

		if (!uopt.chroot) {
			BUILD_PATH(path, uopt.branches[i].path);
		} else {
			BUILD_PATH(path, uopt.chroot, uopt.branches[i].path);
		}

		int fd = open(path, O_RDONLY);
		if (fd == -1) {
			fprintf(stderr, "\nFailed to open %s: %s. Aborting!\n\n",
				path, strerror(errno));
			exit(1);
		}
		uopt.branches[i].fd = fd;
		uopt.branches[i].path_len = strlen(path);
	}
}

int unionfs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs) {
	(void)data;  // prevent compiler warning

	int res = 0; // for general purposes

	switch (key) {
		case FUSE_OPT_KEY_NONOPT:
			res = parse_branches(arg);
			if (res > 0) return 0;
			uopt.retval = 1;
			return 1;
		case KEY_DIRS:
			// skip the "dirs="
			res = parse_branches(arg+5);
			if (res > 0) return 0;
			uopt.retval = 1;
			return 1;
		case KEY_CHROOT:
			uopt.chroot = get_opt_str(arg, "chroot");
			return 0;
		case KEY_DIRECT_IO:
		      uopt.direct_io = 1;
		      return 0;
		case KEY_COW:
			uopt.cow_enabled = true;
			return 0;
		case KEY_DEBUG_FILE:
			uopt.dbgpath = get_opt_str(arg, "debug_file");
			uopt.debug = true;
			return 0;
		case KEY_HELP:
			print_help(outargs->argv[0]);
			fuse_opt_add_arg(outargs, "-ho");
			uopt.doexit = 1;
			return 0;
		case KEY_HIDE_META_FILES:
		case KEY_HIDE_METADIR:
			uopt.hide_meta_files = true;
			return 0;
		case KEY_MAX_FILES:
			set_max_open_files(arg);
			return 0;
		case KEY_NOINITGROUPS:
			// option only for compatibility with older versions
			return 0;
		case KEY_STATFS_OMIT_RO:
			uopt.statfs_omit_ro = true;
			return 0;
		case KEY_RELAXED_PERMISSIONS:
			uopt.relaxed_permissions = true;
			return 0;
		case KEY_VERSION:
			printf("unionfs-fuse version: "VERSION"\n");
#ifdef HAVE_XATTR
			printf("(compiled with xattr support)\n");
#endif
			uopt.doexit = 1;
			return 1;
		default:
 			uopt.retval = 1;
			return 1;
	}
}
