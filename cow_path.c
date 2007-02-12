/*
* Description: copy-on-write functions. Create a path in a RW-root,
*              that exists in in a lower level RO-root
*
*
* Author: Bernd Schubert <bernd-schubert@gmx.de>, (C) 2007
*
* Copyright: BSD style license
*
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <utime.h>
#include <syslog.h>
#include <string.h>

#include "unionfs.h"
#include "opts.h"
#include "debug.h"
#include "cache.h"
#include "general.h"

