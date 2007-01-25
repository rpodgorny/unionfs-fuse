#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "opts.h"
#include "stats.h"


char *make_absolute(char *name) {
	static char cwd[PATHLEN_MAX];
	static size_t cwdlen = 0;
	if (*name == '/') return name;

	if (!cwdlen) {
		if (!getcwd(cwd, PATHLEN_MAX)) {
			perror("Unable to get current working directory");
		} else {
			cwdlen = strlen(cwd);
		}
	}

	if (cwdlen) {
		char *save = name;
		// We do not free this one as it may go to roots
		name = (char *)malloc(cwdlen + strlen(save) + 2);
		strcpy (name, cwd);
		name[cwdlen] = '/';
		name[cwdlen+1] = '\0';
		strcat (name, save);
	}

	return name;
}

int unionfs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs) {
	(void)data;

	switch (key) {
		case FUSE_OPT_KEY_NONOPT:
			if (!nroots) {
				// We don't free the buf as parts of it may go to roots
				char *buf = strdup(arg);
				char **ptr = (char **)&buf;
				char *root;
				while ((root = strsep(ptr, ROOT_SEP)) != NULL) {
					if (strlen(root) == 0) continue;

					roots = realloc(roots, (nroots+1) * sizeof(root));
					roots[nroots++] = make_absolute(root);
				}
				return 0;
			}
			return 1;
		case KEY_STATS:
			stats_enabled = 1;
			return 0;
		case KEY_HELP:
			fprintf (stderr,
			"unionfs-fuse version 0.16\n"
			"by Radek Podgorny <radek@podgorny.cz>\n"
			"\n"
			"Usage: %s [options] root[:root...] mountpoint\n"
			"The first argument is a colon separated list of directories to merge\n"
			"\n"
			"general options:\n"
			"    -o opt,[opt...]        mount options\n"
			"    -h   --help            print help\n"
			"    -V   --version         print version\n"
			"\n"
			"UnionFS options:\n"
			"    -o stats               show statistics in the file 'stats' under the mountpoint\n"
			"\n",
			outargs->argv[0]);
			fuse_opt_add_arg(outargs, "-ho");
			doexit = 1;
			return 0;
		case KEY_VERSION:
			printf("unionfs-fuse version: 0.15\n");
			doexit = 1;
			return 1;
		default:
			return 1;
	}
}
