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

	// 2 due to: +1 for '/' between cwd and relpath
	//           +1 for terminating '\0'
	int abslen = cwdlen + strlen(relpath) + 2;
	if (abslen > PATHLEN_MAX) {
		fprintf(stderr, "Absolute path too long!\n");
		return NULL;
	}

	char *abspath = malloc(abslen);
	if (abspath == NULL) {
		fprintf(stderr, "%s: malloc failed\n", __func__);
		exit (1); // still at early stage, we can't abort
	}
	// the terminating '/' is important so that we are sure later on the
	// diroctory components are properly seperated
	sprintf(abspath, "%s/%s/", cwd, relpath);

	return abspath;
}

/**
 * Add a trailing slash at the end of a branch (root). So functions using this
 * path don't have to care about this slash themselves.
 **/
static char *add_trailing_slash(char *path)
{
	int len = strlen(path);
	if (path[len - 1] == '/')
		return path; // no need to add a slash, already there
	
	path = realloc(path, len + 2); // +1 for '/' and +1 for '\0'
	if (!path) {
		fprintf(stderr, "%s: realloc() failed, aborting\n", __func__);
		exit (1); // still very early stage, we can abort here
	}
	
	strcat (path, "/");
	return (path);
}

/**
 * Add a given root and its options to the array of available roots.
 * example root string "root1=RO" or "/path/path2=RW"
 */
static void add_root(char *root) {
	uopt.roots = realloc(uopt.roots, (uopt.nroots+1) * sizeof(root_entry_t));
	if (uopt.roots == NULL) {
		fprintf(stderr, "%s: realloc failed\n", __func__);
		exit (1); // still at early stage, we can't abort
	}


	char *res;
	char **ptr = (char **)&root;

	res = strsep(ptr, "=");
	if (!res) return;

	// for string manipulations it is important to copy the string, otherwise
	// make_absolute() and add_trailing_slash() will corrupt our input (parse string)
	uopt.roots[uopt.nroots].path = strdup(res);
	uopt.roots[uopt.nroots].path = make_absolute(uopt.roots[uopt.nroots].path);
	uopt.roots[uopt.nroots].path = add_trailing_slash(uopt.roots[uopt.nroots].path);
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

	free (buf);
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
	"    -o noinitgroups        disable initgroups\n"
	"                           initgroups are enabled by default to supply\n"
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
			uopt.retval = 1;
			return 1;
		case KEY_STATS:
			uopt.stats_enabled = 1;
			return 0;
		case KEY_COW:
			uopt.cow_enabled = true;
			return 0;
		case KEY_NOINITGROUPS:
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
 			uopt.retval = 1;
			return 1;
	}
}
