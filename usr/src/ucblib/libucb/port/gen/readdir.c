/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)readdir.c	1.5	96/08/27 SMI"	/* SVr4.0 1.1	*/

/*
******************************************************************

		PROPRIETARY NOTICE(Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.



		Copyright Notice

Notice of copyright on this source code product does not indicate
publication.

	(c) 1986, 1987, 1988, 1989, 1996  Sun Microsystems, Inc
	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
			All rights reserved.
******************************************************************* */

/*
	readdir -- C library extension routine

*/

#include	<sys/types.h>
#include	<sys/dir.h>
#include 	<sys/dirent.h>
#include 	<limits.h>
#include 	<string.h>
#include 	<errno.h>

struct direct	dc;
struct direct64	dc64;

#define	NULL 0
struct direct64 *
readdir64(DIR *dirp)
{
	register struct dirent64	*dp;	/* -> directory data */
	int saveloc = 0;

	if (dirp->dd_size != 0) {
		dp = (struct dirent64 *)&dirp->dd_buf[dirp->dd_loc];
		saveloc = dirp->dd_loc;   /* save for possible EOF */
		dirp->dd_loc += dp->d_reclen;
	}
	if (dirp->dd_loc >= dirp->dd_size)
		dirp->dd_loc = dirp->dd_size = 0;

	if (dirp->dd_size == 0 && 	/* refill buffer */
	    (dirp->dd_size = getdents64(dirp->dd_fd,
	    (struct dirent64 *)dirp->dd_buf, DIRBUF)) <= 0) {
		if (dirp->dd_size == 0)	/* This means EOF */
			dirp->dd_loc = saveloc;  /* EOF so save for telldir */
		return (NULL);	/* error or EOF */
	}

	dp = (struct dirent64 *)&dirp->dd_buf[dirp->dd_loc];

	/* Copy dirent into direct */
	dc64.d_ino = dp->d_ino;
	dc64.d_reclen = dp->d_reclen;
	dc64.d_namlen = strlen(dp->d_name);
	if (dc64.d_namlen > MAXNAMLEN) {
		errno = ENAMETOOLONG;
		return (NULL);
	}
	strcpy(dc64.d_name, dp->d_name);

	return (&dc64);
}

struct direct *
readdir(DIR *dirp)
{
	if (readdir64(dirp) == NULL)
		return (NULL);

	/* Check for overflows */
	if (dc64.d_ino > SIZE_MAX) {
		errno = EOVERFLOW;
		return (NULL);
	}

	/* Copy dirent into direct */
	dc.d_ino = dc64.d_ino;
	dc.d_reclen = dc64.d_reclen - 4;
	dc.d_namlen = dc64.d_namlen;
	strcpy(dc.d_name, dc64.d_name);

	return (&dc);
}
