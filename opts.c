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
		name = (char *)malloc(cwdlen + strlen(save) + 2);
		strcpy (name, cwd);
		name[cwdlen] = '/';
		name[cwdlen+1] = '\0';
		strcat (name, save);
	}

	return name;
}

int unionfs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs) {
	switch (key) {
		case FUSE_OPT_KEY_NONOPT:
			if (!nroots) {
				char* tmp = strdup (arg);
				char* scan = tmp;
				size_t rootnum = 0;
				nroots = 1;
				while (1) {
					char* ri = rindex(scan, ROOT_SEP);
					if (ri) {
						nroots ++;
						scan = ri + 1;
					} else {
						break;
					}
				}
				roots = (char **)malloc(nroots * sizeof(char *));
				scan = tmp;
				for (;;) {
					char *ri = rindex(scan, ROOT_SEP);
					if (ri) *ri = '\0';
					roots[rootnum] = make_absolute(scan);
					if (! ri) break;
					scan = ri + 1;
					rootnum++;
				}
				return 0;
			}
			return 1;
		case KEY_STATS:
			stats_enabled = 1;
			return 0;
		case KEY_HELP:
			fprintf (stderr,
			"unionfs-fuse version 0.13 by Radek Podgorny\n"
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
			"    --stats                equivalent to -o stats, for backward compatibility\n"
			"    --roots                equivalent to the first argument, for backward compatibility\n"
			"\n",
			outargs->argv[0]);
			fuse_opt_add_arg(outargs, "-ho");
			doexit = 1;
			return 0;
		case KEY_VERSION:
			printf("unionfs-fuse version: 0.13\n");
			doexit = 1;
			return 1;
		default:
			return 1;
	}
}
