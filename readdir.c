/*
* Description: find a file in a branch
*
* License: BSD-style license
*
*
* Author: Radek Podgorny <radek@podgorny.cz>
*
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/statvfs.h>

#include "unionfs.h"
#include "opts.h"
#include "cache.h"
#include "stats.h"
#include "debug.h"
#include "elfhash.h"

typedef struct node {
	char fname[PATHLEN_MAX];
	int hash;
	struct node *next;
} node_t;


typedef struct list {
	node_t *first;
	node_t *last;
} list_t;


/**
 * Initialize the list.
 */
static list_t * init_list(void)
{
	list_t *list = malloc(sizeof(*list));
	
	if (list)
		memset(list, 0, sizeof(*list));

	return list;
}

/**
 * Initialize one element and add it to the list and add it to the end of 
 * the given list.
 */
static void add_to_list(list_t *list, const char *fname)
{
	node_t *new = malloc (sizeof(*new));
	
	if (!new) {
		// out of memory?
		return;
	}
	
	strncpy(new->fname, fname, PATHLEN_MAX);
	new->hash = ELFHash(fname, strlen(fname));
	new->next  = NULL;
	
	if (!list->first) {
		list->first = new;
		list->last  = new;
	} else {
		// list already initialized previously
		list->last->next = new;
		list->last = new;
	}
}

/**
 * Free the entire list.
 */
static void free_list(list_t *list)
{
	node_t *current, *next;

	current = list->first;
	while (current) {
		next = current->next;
		free (current);
		current = next;
	}
}

/**
 * Check if the given fname suffixes the hide tag
 */
static char  *hide_tag(const char *fname)
{
	char *tag = strstr(fname, HIDETAG);

	/* check if fname has tag, fname is not only the tag, file name ends 
	* with the tag
	* TODO: static strlen(HIDETAG) */
	if (tag && tag != fname && strlen(tag) == strlen(HIDETAG)) {
		return tag;
	}

	return NULL;
}

/**
 * read a directory and add files with the hiddenflag to the 
 * list of hidden files
 */
static void read_hidden(list_t *list, DIR *dp)
{
	struct dirent *de;
	char *tag;
	
	while ((de = readdir(dp)) != NULL) {
		tag = hide_tag(de->d_name);
		if (tag) {
			// ignore this file
			add_to_list(list, de->d_name);
			
			/* even more important, ignore the file without the tag!
			* hint: tag is a pointer to the flag-suffix within 
			*       de->d_name */
			*tag = '\0';
			add_to_list(list, de->d_name);
		}
	}
	rewinddir (dp);
}

/**
 * Check if fname is in the given list.
 */
static int in_list(char *fname, list_t *list)
{
	node_t *cur = list->first; // current element
	int hash   = ELFHash(fname, strlen(fname));

	while (cur) {
		if (cur->hash == hash &&
				  strncmp(cur->fname, fname, PATHLEN_MAX) == 0)
			return 1;
		cur = cur->next;
	};

	return 0;
}

/**
 * unionfs-fuse readdir function
 */
int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		    off_t offset, struct fuse_file_info *fi) 
{
	(void)offset;
	(void)fi;
	int i = 0;
	list_t *hides = init_list();
	list_t *files = init_list();

	DBG("readdir\n");

	for (i = 0; i < uopt.nroots; i++) {
		char p[PATHLEN_MAX];
		
		snprintf(p, PATHLEN_MAX, "%s%s", uopt.roots[i].path, path);

		DIR *dp = opendir(p);
		if (dp == NULL) 
			continue;

		read_hidden(hides, dp);

		struct dirent *de;
		while ((de = readdir(dp)) != NULL) {
		
			if (!in_list(de->d_name, hides) &&
						  !in_list(de->d_name, files)) {
				/* file is not hidden and file is not already 
				* added by an upper level root, add it now */
				
				add_to_list(files, de->d_name);

				struct stat st;
				memset(&st, 0, sizeof(st));
				st.st_ino = de->d_ino;
				st.st_mode = de->d_type << 12;
				
				if (filler(buf, de->d_name, &st, 0))
					break;
						  }
		}

		closedir(dp);
	}

	if (uopt.stats_enabled && strcmp(path, "/") == 0) {
		filler(buf, "stats", NULL, 0);
	}

	free_list(hides);
	free_list(files);
	return 0;
}

