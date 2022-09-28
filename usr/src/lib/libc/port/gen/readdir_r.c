/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights reserved.
 */
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)readdir_r.c	1.16	96/08/06 SMI"	/* SVr4.0 1.11  */

/*
	readdir_r -- C library extension routine

*/
#pragma weak readdir64_r = _readdir64_r
#pragma weak readdir_r = _readdir_r

#include	"synonyms.h"
#include	<sys/types.h>
#include	<sys/dirent.h>
#include	<dirent.h>
#include	<thread.h>
#include	<string.h>
#include	<synch.h>
#include	<mtlib.h>
#include	<stdio.h>
#include	<limits.h>
#include	<errno.h>

#define	NULL 0

#ifdef _REENTRANT
extern mutex_t	_dirent_lock;
#endif	/* _REENTRANT */

/*
 * POSIX.1c Draft-6 version of the function readdir_r.
 * It was implemented by Solaris 2.3.
 */

int
readdir64_r(DIR *dirp, struct dirent64 *entry, struct dirent64 **result)
{
	struct dirent64	*dp64;	/* -> directory data */
	struct dirent	*dp32;	/* -> 32 bit directory data */
	int saveloc = 0;

	_mutex_lock(&_dirent_lock);
	if (dirp->dd_size != 0) {
		dp64 = (struct dirent64 *)&dirp->dd_buf[dirp->dd_loc];
		/* was converted by readdir and needs to be reversed */
		if (dp64->d_ino == (ino64_t)-1) {
			dp32 = (struct dirent *)(&dp64->d_off);
			dp64->d_ino = (ino64_t)dp32->d_ino;
			dp64->d_off = (off64_t)dp32->d_off;
			dp64->d_reclen = dp32->d_reclen +
				((char *)&dp64->d_off - (char *)dp64);
		}
		saveloc = dirp->dd_loc;   /* save for possible EOF */
		dirp->dd_loc += dp64->d_reclen;
	}

	if (dirp->dd_loc >= dirp->dd_size)
		dirp->dd_loc = dirp->dd_size = 0;

	if (dirp->dd_size == 0 &&	/* refill buffer */
	    (dirp->dd_size = getdents64(dirp->dd_fd,
			(struct dirent64 *)dirp->dd_buf, DIRBUF)) <= 0) {
		if (dirp->dd_size == 0)	/* This means EOF */
			dirp->dd_loc = saveloc;  /* EOF so save for telldir */
		_mutex_unlock(&_dirent_lock);
		return (-1);	/* error or EOF */
	}

	dp64 = (struct dirent64 *)&dirp->dd_buf[dirp->dd_loc];
	memcpy(entry, dp64, dp64->d_reclen);
	*result = entry;
	_mutex_unlock(&_dirent_lock);
	return (0);
}

struct dirent *
readdir_r(DIR *dirp, struct dirent *entry)
{
	char buf[sizeof (struct dirent64) + _POSIX_PATH_MAX + 1];
	struct dirent64	*dp64;

	if (readdir64_r(dirp, (struct dirent64 *)buf, &dp64) == -1)
		return (NULL);

	if ((dp64->d_ino > SIZE_MAX) || (dp64->d_off > LONG_MAX)) {
		errno = EOVERFLOW;
		return (NULL);
	}

	entry->d_ino = (ino_t)dp64->d_ino;
	entry->d_off = (off_t)dp64->d_off;
	entry->d_reclen = ((((char *)entry->d_name - (char *)entry) +
				strlen(dp64->d_name) + 1 + 3) & ~3);
	(void) strcpy(entry->d_name, dp64->d_name);

	return (entry);
}


/*
 * POSIX.1c standard version of the thr function readdir_r.
 * User gets it via static readdir_r from header file.
 */

int
__posix_readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result)
{
	int nerrno = 0;
	int oerrno = errno;

	errno = 0;
	if ((*result = readdir_r(dirp, entry)) == NULL) {
		if (errno == 0)
			nerrno = EINVAL;
		else
			nerrno = errno;
	}
	errno = oerrno;
	return (nerrno);
}
