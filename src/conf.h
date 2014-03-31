/*
 * License: BSD-style license
 * Copyright: Bernd Schubert <bernd.schubert@fastmail.fm>
 *
 */

#ifndef CONF_H_
#define CONF_H_

#ifdef _XOPEN_SOURCE

// *at support, such as openat, utimensat, etc (see man 2 openat)
#include <fcntl.h>
#include <sys/stat.h>
#if !defined (DISABLE_AT) && (_XOPEN_SOURCE >= 700 && _POSIX_C_SOURCE >= 200809L) \
	&& defined (AT_SYMLINK_NOFOLLOW)
	#define UNIONFS_HAVE_AT
#endif

#endif // _XOPEN_SOURCE

// xattr support
#if !defined (DISABLE_XATTR)
	#if defined (LIBC_XATTR)
		#include <sys/xattr.h>
	#elif defined (LIBATTR_XATTR)
		#include <attr/xattr.h>
	#else
		#error // neither libc attr nor libattr xattr defined
	#endif

	#if defined (XATTR_CREATE) && defined (XATTR_REPLACE)
		#define HAVE_XATTR
	#endif
#endif


#endif // CONF_H_

