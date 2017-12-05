/*
 * License: BSD-style license
 * Copyright: Bernd Schubert <bernd.schubert@fastmail.fm>
 *
 * Details: 
 *   Log files might be located on our own filesystem. If we then want to log
 *   a message to syslog, we would need to log to ourself, which easily ends up
 *   in a deadlock. Initializing openlog() using the flags 
 *   LOG_NDELAY | LOG_NOWAIT should prevent that, but real live has shown that
 *   this does not work reliable and systems 'crashed' just because we 
 *   tried to log a harmless message. 
 *   So this file introduces a syslog thread and a syslog buffer. usyslog() 
 *   calls write without a risk to deadlock into the syslog buffer (chained 
 *   list) and then the seperate syslog_thread call syslog(). That way our
 *   our filesystem thread(s) cannot stall from syslog() calls.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#if __APPLE__
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

#include <pthread.h>
#include <stdarg.h>

#include "usyslog.h"
#include "debug.h"

static ulogs_t *free_head, *free_bottom; // free chained list log entries
static ulogs_t *used_head = NULL, *used_bottom = NULL; //used chained list pointers

static pthread_mutex_t list_lock; 	// locks the entire chained list
static pthread_cond_t cond_message;		// used to wake up the syslog thread

// Only used for debugging, protected by list_lock
static int free_entries;  
static int used_entries = 0;

//#define USYSLOG_DEBUG

#ifdef USYSLOG_DEBUG
static void verify_lists()
{
	pthread_mutex_lock(&list_lock);
	
	ulogs_t *entry = free_head;
	int free_count = -1;
	bool first_free = true;
	while (entry) {
		if (first_free) {
			first_free = false;
			free_count = 1;
		} else
			free_count++;
		entry = entry->next;
	}
	if (free_count != free_entries && free_entries != 0) 
		DBG("usyslog list error detected: number of free entries inconsistent!"
		     " %d vs. %d", free_count, free_entries);
		     
	entry = used_head;
	int used_count = -1;
	bool first_used = true;
	while (entry) {
		if (first_used) {
			first_used = false;
			used_count = 1;
		} else
			used_count++;
		entry = entry->next;
	}
	if (used_count != used_entries && used_entries != 0) 
		DBG("usyslog list error detected: number of used entries inconsistent!"
		     " (used: %d vs. %d) (free: %d vs. %d) \n", 
		     used_count, used_entries, free_count, free_entries);
		     
	pthread_mutex_unlock(&list_lock);
}
#else
#define verify_lists()
#endif


/**
 * Walks the chained used-list and calls syslog()
 */
static void do_syslog(void)
{
	pthread_mutex_lock(&list_lock); // we MUST ensure not to keep that forever

	ulogs_t *log_entry = used_head;

	while (log_entry) {
		pthread_mutex_t *entry_lock = &log_entry->lock;
		int res = pthread_mutex_trylock(entry_lock);
		if (res) {
			if (res != EBUSY) 
				DBG("Entirely unexpected locking error %s\n", 
				    strerror(res));
			// If something goes wrong with the log_entry we do not
			// block the critical list_lock forever!
			// EBUSY might come up rarely, if we race with usyslog()
			pthread_mutex_unlock(&list_lock);
			sleep(1);
			pthread_mutex_lock(&list_lock);
			log_entry = used_head;
			continue;
		}
		pthread_mutex_unlock(&list_lock);
		
		// This syslog call and so this lock might block, so be
		// carefull on using locks! The filesystem IO thread
		// *MUST* only try to lock it using pthread_mutex_trylock()
		syslog(log_entry->priority, "%s", log_entry->message);
		log_entry->used = false;

		// NOTE: The list is only locked now, after syslog() succeeded!
		pthread_mutex_lock(&list_lock);
		ulogs_t *next_entry = log_entry->next; // just to save the pointer
		
		used_head = log_entry->next;
		if (!used_head)
			used_bottom = NULL; // no used entries left
		
		if (free_bottom)
			free_bottom->next = log_entry;
		free_bottom = log_entry;
		free_bottom->next = NULL; 
		
		if (!free_head)
			free_head = log_entry;
			
		free_entries++;
		used_entries--;
			
		pthread_mutex_unlock(&list_lock); // unlock ist ASAP
		
		log_entry = next_entry;
		pthread_mutex_unlock(entry_lock);
	}
	
	verify_lists();
}

/**
 * syslog backgroung thread that tries to to empty the syslog buffer
 */
static void * syslog_thread(void *arg)
{
	// FIXME: What is a better way to prevent a compiler warning about
	//        unused variable 'arg'
	int tinfo = *((int *) arg);
	if (tinfo == 0 || tinfo == 1)
		printf("Starting thread %d", tinfo);

	pthread_mutex_t sleep_mutex; 

	pthread_mutex_init(&sleep_mutex, NULL);
	pthread_mutex_lock(&sleep_mutex);
	while (1) {
		pthread_cond_wait(&cond_message, &sleep_mutex);
		do_syslog();
	}

	return NULL;
}

/**
 * usyslog - function to be called if something shall be logged to syslog
 */
void usyslog(int priority, const char *format, ...)
{
	int res;
	ulogs_t *log;

	// Lock the entire list first, which means the syslog thread MUST NOT
	// lock it if there is any chance it might be locked forever.
	pthread_mutex_lock(&list_lock);
	
	// Some sanity checks. If we fail here, we will leak a log entry,
	// but will not lock up.

	if (free_head == NULL) {
		DBG("All syslog entries already busy\n");
		pthread_mutex_unlock(&list_lock);
		return;
	}
	
	log = free_head;
	free_head = log->next;

	res = pthread_mutex_trylock(&log->lock);
	if (res == EBUSY) {
			// huh, that never should happen!
			DBG("Critical log error, log entry is BUSY, but should not\n");
			pthread_mutex_unlock(&list_lock);
			return;
	} else if (res) {
		// huh, that never should happen either!
		DBG("Never should happen, can get lock: %s\n", strerror(res));
		pthread_mutex_unlock(&list_lock);
		return;
	}

	if (log->used) {
		// huh, that never should happen either!
		DBG("Never should happen, entry is busy, but should not!\n");
		pthread_mutex_unlock(&log->lock);
		pthread_mutex_unlock(&list_lock);
		return;
	}

	if (!used_head)
		used_head = used_bottom = log;
	else {
		used_bottom->next = log;
		used_bottom = log;
	}
	
	if (log->next) {
		// from free_list to end of used_list, so next is NULL now
		log->next = NULL; 
	} else {
		// so the last entry in free_list
		free_bottom = NULL;
	}
	
	
	free_entries--;
	used_entries++;
		
	// Everything below is log entry related, so we can free the list_lock
	pthread_mutex_unlock(&list_lock);
	
	va_list ap;
	va_start(ap, format);
	vsnprintf(log->message, MAX_MSG_SIZE, format, ap);
	log->priority = priority;
	log->used = 1;

	pthread_mutex_unlock(&log->lock);

	pthread_cond_signal(&cond_message); // wake up the syslog thread
}

/**
 * Initialize syslogs
 */
void init_syslog(void)
{
	openlog("unionfs-fuse: ", LOG_CONS | LOG_NDELAY | LOG_NOWAIT | LOG_PID, LOG_DAEMON);

	pthread_mutex_init(&list_lock, NULL);
	pthread_cond_init(&cond_message, NULL);
	pthread_t thread;
	pthread_attr_t attr;
	int t_arg = 0; // thread argument, not required for us

	int i;
	ulogs_t *log, *last = NULL;
	for (i = 0; i < MAX_SYSLOG_MESSAGES; i++) {
		log = malloc(sizeof(ulogs_t));
		if (log == NULL) {
			fprintf(stderr, "\nLog initialization failed: %s\n", strerror(errno));
			fprintf(stderr, "Aborting!\n");
			// Still initialazation phase, we can abort.
			exit (1);	
		}

		log->used = false;
		pthread_mutex_init(&log->lock, NULL);
		
		if (last) {
			last->next = log;
		} else {
			// so the very first entry
			free_head = log;
		}
		last = log;
	}
	last->next = NULL;
	free_bottom = last;
	
	free_entries = MAX_SYSLOG_MESSAGES;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	int res = pthread_create(&thread, &attr, syslog_thread, (void *) &t_arg);
	if (res != 0) {
		fprintf(stderr, "Failed to initialize the syslog threads: %s\n",
			strerror(res));
		exit(1);
	}
}

