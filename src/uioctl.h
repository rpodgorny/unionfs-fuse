/*
* License: BSD-style license
* Copyright: Bernd Schubert <bernd.schubert@fastmail.fm>
* 
*/

#ifndef UIOCTL_H_
#define UIOCTL_H_

#include <sys/ioctl.h>

#include "unionfs.h"


enum unionfs_ioctls {
	UNIONFS_ONOFF_DEBUG         = _IOW('E', 0, int),
	UNIONFS_SET_DEBUG_FILE      = _IOW('E', 1, char[PATHLEN_MAX]),
	UNIONFS_STATS_BYTES_READ    = _IOW('E', 2, void),
	UNIONFS_STATS_BYTES_WRITTEN = _IOW('E', 3, void),
} unionfs_ioctls_t;

#endif // UIOCTL_H_

