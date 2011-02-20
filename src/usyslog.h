/*
 * License: BSD-style license
 * Copyright: Bernd Schubert <bernd.schubert@fastmail.fm>
 */

#include <syslog.h>
#include <stdbool.h>

#define MAX_SYSLOG_MESSAGES 32	// max number of buffered syslog messages
#define MAX_MSG_SIZE 256	// max string length for syslog messages

/* chained buffer list of syslog entries  */
typedef struct ulogs {
	int priority; // first argument for syslog()
	char message[MAX_MSG_SIZE]; // 2nd argument for syslog() 
	bool used;		// is this entry used?
	pthread_mutex_t lock;	// lock a single entry
	struct ulogs *next;	// pointer to the next entry
} ulogs_t;


void init_syslog(void);
void usyslog(int priority, const char *format, ...);


#define USYSLOG(priority, format, ...)  			\
	do {							\
		DBG(format, ##__VA_ARGS__);			\
		usyslog(priority, format, ##__VA_ARGS__);	\
	} while (0);

