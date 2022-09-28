/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights reserved.
 */
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)telldir.c	1.19	96/06/17 SMI"	/* SVr4.0 1.6	*/

/*
	telldir -- C library extension routine

*/

#pragma weak telldir64 = _telldir64
#pragma weak telldir = _telldir

#include "synonyms.h"
#include <sys/types.h>
#include <dirent.h>
#include <thread.h>
#include <errno.h>
#include <limits.h>
#include <synch.h>
#include <mtlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#ifdef _REENTRANT
extern mutex_t	_dirent_lock;
#endif	/* _REENTRANT */

off64_t
telldir64(DIR *dirp)
{
	struct dirent64	*dp64;
	struct dirent	*dp32;
	off64_t		off = 0;

	_mutex_lock(&_dirent_lock);
	/* if at beginning of dir, return 0 */
	if (lseek64(dirp->dd_fd, 0, SEEK_CUR) != 0) {
		dp64 = (struct dirent64 *)&dirp->dd_buf[dirp->dd_loc];
		/* was converted by readdir and needs to be reversed */
		if (dp64->d_ino == (ino64_t)-1) {
			dp32 = (struct dirent *)
			    ((char *)dp64 + sizeof (ino64_t));
			dp64->d_ino = (ino64_t)dp32->d_ino;
			dp64->d_off = (off64_t)dp32->d_off;
			dp64->d_reclen = dp32->d_reclen +
				((char *)&dp64->d_off - (char *)dp64);
		}
		off = dp64->d_off;
	}
	_mutex_unlock(&_dirent_lock);
	return (off);
}

long
telldir(DIR *dirp)
{
	off64_t off;

	off = telldir64(dirp);
	if (off > LONG_MAX) {
		errno = EOVERFLOW;
		return (-1);
	}
	return ((long) off);
}
