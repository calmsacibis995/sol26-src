/*	Copyright (c) 1988 AT&T	*/
/*	All Rights Reserved	*/
/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)fsync.c	1.5	96/05/02	SMI"
/*
 * fsync(int fd)
 *
 */
#include "synonyms.h"
#include "sys/file.h"

extern int __fdsync();

#ifdef __STDC__
#pragma weak	_libc_fsync = _fsync
#endif

fsync(fd)
	int fd;
{

	return (__fdsync(fd, FSYNC));
}
