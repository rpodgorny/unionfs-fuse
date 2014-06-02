#include <unistd.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>


#include "uioctl.h"

static void print_help(char* progname)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "     %s <parameter1> [<parameter2>] [file-path] \n", progname);
	fprintf(stderr, "\n");
	fprintf(stderr, "     List of parameters\n");
	fprintf(stderr, "       -p </path/to/debug/file>\n");
	fprintf(stderr, "       -d <on/off\n");
	fprintf(stderr, "          Enable or disable debugging.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Example: ");
	fprintf(stderr, " %s -p /tmp/unionfs.debug -d on /mnt/unionfs/union\n", progname);
	fprintf(stderr, "\n");
	fprintf(stderr, "\n");
	
}

int main(int argc, char **argv)
{
	char *progname = basename(argv[0]);

	if (argc < 3) {
		print_help(progname);
		exit(1);
	}

	// file_name is the last argument
	const char *file_name = argv[argc - 1];
	const int fd = open(file_name, O_RDONLY );
	if (fd == -1) {
		fprintf(stderr, "Failed to open file: %s: %s\n\n", 
			file_name, strerror(errno) );
		exit(1);
	}
	argc--;

	int opt;
	const char* argument_param;
	int debug_on_off;
	int ioctl_res;
	while ((opt = getopt(argc, argv, "d:p:")) != -1) {
		switch (opt) {
		case 'p': 
			argument_param = optarg;
			if (strlen(argument_param) < 1) {
				fprintf(stderr, 
					"Not a valid debug path given!\n");
				print_help(progname);
				exit(1);
			}

			if (strlen(argument_param) > PATHLEN_MAX) {
				fprintf(stderr, "Debug path too long!\n");
				exit(1);
			}

			ioctl_res = ioctl(fd, UNIONFS_SET_DEBUG_FILE, argument_param);
			if (ioctl_res == -1) {
				fprintf(stderr, "debug-file ioctl failed: %s\n",
					strerror(errno) );
				exit(1);
			}
			break;
				

		case 'd':
			argument_param = optarg;
			if (strlen(argument_param) < 1) {
				fprintf(stderr,
					"invalid \"-d %s\" option given, valid is"
					"-d on/off!\n", argument_param);
				exit(1);
			}

			if (strncmp(argument_param, "on", 2) == 0)
				debug_on_off = 1;
			else if ((strncmp(argument_param, "off", 3) == 0) )
				debug_on_off = 0;
			else {
				fprintf(stderr,
					"invalid \"-d %s\" option given, valid is "
					"\"-d on/off\"!\n", argument_param);
				exit(1);
			}

			ioctl_res = ioctl(fd, UNIONFS_ONOFF_DEBUG, &debug_on_off);
			if (ioctl_res == -1) {
				fprintf(stderr, "debug-on/off ioctl failed: %s\n",
					strerror(errno) );
				exit(1);
			}
			break;
				
		default:
			fprintf(stderr, "Unhandled option %c given.\n", opt);
			break;
		}

	}

	return 0;
}
