/*
 * License: BSD-style license
 * Copyright: Bernd Schubert <bernd.schubert@fastmail.fm>
 *
 */

#ifndef CONF_H_
#define CONF_H_

#ifdef _XOPEN_SOURCE

#if !defined (DISABLE_AT) && (_XOPEN_SOURCE >= 700 && _POSIX_C_SOURCE >= 200809L) \
	&& defined (AT_SYMLINK_NOFOLLOW)
#define UNIONFS_HAVE_AT
#endif

#endif // _XOPEN_SOURCE

#endif // CONF_H_

