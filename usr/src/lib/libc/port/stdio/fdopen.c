/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)fdopen.c	1.12	96/01/30 SMI"	/* SVr4.0 1.16	*/

/*	3.0 SID #	1.2	*/
/*LINTLIBRARY*/
/*
 * Unix routine to do an "fopen" on file descriptor
 * The mode has to be repeated because you can't query its
 * status
 */

#define	_LARGEFILE64_SOURCE 1

#pragma weak fdopen = _fdopen

#define	fdopen		_fdopen
#define	lseek64		_lseek64

#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include "stdiom.h"
#include <thread.h>
#include <synch.h>
#include <mtlib.h>

FILE *
fdopen(int fd, const char *type) /* associate file desc. with stream */
{
	/* iop doesn't need locking since this function is creating it */
	register FILE *iop;
	register int plus;
	register unsigned char flag;

	if (fd > UCHAR_MAX || (iop = _findiop()) == 0)
		return (0);
	iop->_file = (Uchar)fd;
	switch (type[0])
	{
	default:
#ifdef _REENTRANT
		iop->_flag = 0; /* release iop */
#endif
		return (0);
	case 'r':
		flag = _IOREAD;
		break;
	case 'a':
		(void) lseek64(fd, (off64_t)0 , SEEK_END);
		/*FALLTHROUGH*/
	case 'w':
		flag = _IOWRT;
		break;
	}
	if ((plus = type[1]) == 'b')	/* Unix ignores 'b' ANSI std */
		plus = type[2];
	if (plus == '+')
		flag = _IORW;
	iop->_flag = flag;
	return (iop);
}
