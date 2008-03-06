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
	uopt.nroots = 0;
	uopt.stats_enabled = false;
	uopt.cow_enabled = false; // copy-on-write
	uopt.initgroups = true;
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

	int abslen = cwdlen + strlen(relpath) + 2;
	if (abslen > PATHLEN_MAX) {
		fprintf(stderr, "Absolute path too long!\n");
		return NULL;
	}

	char *abspath = malloc(abslen);
	sprintf(abspath, "%s/%s", cwd, relpath);

	return abspath;
}

/**
 * Add a given root and its options to the array of available roots.
 * example root string "root1=RO" or "/path/path2=RW"
 */
static void add_root(char *root) {
	uopt.roots = realloc(uopt.roots, (uopt.nroots+1) * sizeof(root_entry_t));

	char *res;
	char **ptr = (char **)&root;

	res = strsep(ptr, "=");
	if (!res) return;

	uopt.roots[uopt.nroots].path = make_absolute(res);
	uopt.roots[uopt.nroots].rw = 0;

	res = strsep(ptr, "=");
	if (res) {
		if (strcasecmp(res, "rw") == 0) {
			uopt.roots[uopt.nroots].rw = 1;
		} else if (strcasecmp(res, "ro") == 0) {
			// no action needed here
		} else {
			fprintf(stderr, "Failed to parse RO/RW flag, setting RO.\n");
			// no action needed here either
		}
	}

	uopt.nroots++;
}

/**
 * Options without any -X prefix, so these options define our root pathes.
 * example arg string: "root1=RW:root2=RO:root3=RO"
 */
static int parse_roots(const char *arg) {
	if (uopt.nroots) return 0;

	// We don't free the buf as parts of it may go to roots
	char *buf = strdup(arg);
	char **ptr = (char **)&buf;
	char *root;
	while ((root = strsep(ptr, ROOT_SEP)) != NULL) {
		if (strlen(root) == 0) continue;

		add_root(root);
	}

	return uopt.nroots;
}

static void print_help(const char *progname) {
	fprintf (stderr,
	"unionfs-fuse version "VERSION"\n"
	"by Radek Podgorny <radek@podgorny.cz>\n"
	"\n"
	"Usage: %s [options] root[=RO/RW][:root...] mountpoint\n"
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
	"    -o disable_initgroups  initgroups are enabled by default to supply\n"
	"                           supplementary user groups, but will cause a deadlock\n"
	"                           of unions including /etc\n"
	"\n",
	progname);
}

int unionfs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs) {
	(void)data;

	int res = 0; // for general purposes

	switch (key) {
		case FUSE_OPT_KEY_NONOPT:
			res = parse_roots(arg);
			if (res > 0) return 0;
			return 1;
		case KEY_STATS:
			uopt.stats_enabled = 1;
			return 0;
		case KEY_COW:
			uopt.cow_enabled = true;
			return 0;
		case KEY_NO_INITGROUPS:
			uopt.initgroups = false;
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
			return 1;
	}
}
