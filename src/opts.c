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

#include "opts.h"
#include "stats.h"
#include "version.h"


uopt_t uopt;


void uopt_init() {
	uopt.doexit = 0;
	uopt.retval = 0;
	uopt.nbranches = 0;
	uopt.stats_enabled = false;
	uopt.cow_enabled = false; // copy-on-write
}

/**
 * Take a relative path as argument and return the absolute path by using the 
 * current working directory. The return string is malloc'ed with this function.
 */
static char *make_absolute(char *relpath) {
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

	// 3 due to: +1 for '/' between cwd and relpath
	//           +1 for trailing '/'
	//           +1 for terminating '\0'
	int abslen = cwdlen + strlen(relpath) + 3;
	if (abslen > PATHLEN_MAX) {
		fprintf(stderr, "Absolute path too long!\n");
		return NULL;
	}

	char *abspath = malloc(abslen);
	if (abspath == NULL) {
		fprintf(stderr, "%s: malloc failed\n", __func__);
		exit (1); // still at early stage, we can abort
	}

	// the trailing '/' is important so that we are sure later on the
	// directory components are properly seperated
	snprintf(abspath, abslen, "%s/%s/", cwd, relpath);

	return abspath;
}

/**
 * Add a trailing slash at the end of a branch. So functions using this
 * path don't have to care about this slash themselves.
 **/
static char *add_trailing_slash(char *path) {
	int len = strlen(path);
	if (path[len - 1] == '/') {
		return path; // no need to add a slash, already there
	}

	path = realloc(path, len + 2); // +1 for '/' and +1 for '\0'
	if (!path) {
		fprintf(stderr, "%s: realloc() failed, aborting\n", __func__);
		exit (1); // still very early stage, we can abort here
	}

	strcat (path, "/");
	return (path);
}

/**
 * Add a given branch and its options to the array of available branches.
 * example branch string "branch1=RO" or "/path/path2=RW"
 */
static void add_branch(char *branch) {
	uopt.branches = realloc(uopt.branches, (uopt.nbranches+1) * sizeof(branch_entry_t));
	if (uopt.branches == NULL) {
		fprintf(stderr, "%s: realloc failed\n", __func__);
		exit (1); // still at early stage, we can't abort
	}

	char *res;
	char **ptr = (char **)&branch;

	res = strsep(ptr, "=");
	if (!res) return;

	// for string manipulations it is important to copy the string, otherwise
	// make_absolute() and add_trailing_slash() will corrupt our input (parse string)
	uopt.branches[uopt.nbranches].path = strdup(res);
	uopt.branches[uopt.nbranches].path = make_absolute(uopt.branches[uopt.nbranches].path);
	uopt.branches[uopt.nbranches].path = add_trailing_slash(uopt.branches[uopt.nbranches].path);
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
 * Options without any -X prefix, so these options define our branch paths.
 * example arg string: "branch1=RW:branch2=RO:branch3=RO"
 */
static int parse_branches(const char *arg) {
	if (uopt.nbranches) return 0;

	// We don't free the buf as parts of it may go to branches
	char *buf = strdup(arg);
	char **ptr = (char **)&buf;
	char *branch;
	while ((branch = strsep(ptr, ROOT_SEP)) != NULL) {
		if (strlen(branch) == 0) continue;

		add_branch(branch);
	}

	free (buf);
	return uopt.nbranches;
}

static void print_help(const char *progname) {
	printf (
	"unionfs-fuse version "VERSION"\n"
	"by Radek Podgorny <radek@podgorny.cz>\n"
	"\n"
	"Usage: %s [options] branch[=RO/RW][:branch...] mountpoint\n"
	"The first argument is a colon separated list of directories to merge\n"
	"\n"
	"general options:\n"
	"    -o opt,[opt...]        mount options\n"
	"    -h   --help            print help\n"
	"    -V   --version         print version\n"
	"\n"
	"UnionFS options:\n"
	"    -o cow                 enable copy-on-write\n"
	"    -o stats               show statistics in the file 'stats' under the\n"
	"                           mountpoint\n"
	"    -o correct_statfs      also count blocks of ro-branches\n"
	"\n",
	progname);
}

int unionfs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs) {
	(void)data;

	int res = 0; // for general purposes

	switch (key) {
		case FUSE_OPT_KEY_NONOPT:
			res = parse_branches(arg);
			if (res > 0) return 0;
			uopt.retval = 1;
			return 1;
		case KEY_STATS:
			uopt.stats_enabled = 1;
			return 0;
		case KEY_COW:
			uopt.cow_enabled = true;
			return 0;
		case KEY_CORRECT_STATFS:
			uopt.correct_statfs = true;
			return 0;
		case KEY_NOINITGROUPS:
			// option only for compatibility with older versions
			return 0;
		case KEY_HELP:
			print_help(outargs->argv[0]);
			fuse_opt_add_arg(outargs, "-ho");
			uopt.doexit = 1;
			return 0;
		case KEY_VERSION:
			printf("unionfs-fuse version: "VERSION"\n");
			uopt.doexit = 1;
			return 1;
		default:
 			uopt.retval = 1;
			return 1;
	}
}
