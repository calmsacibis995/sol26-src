/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights reserved.
 */
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)_endopen.c	1.4	96/04/18 SMI"	/* SVr4.0 1.20	*/

/*LINTLIBRARY*/

/*
 *	This routine is a special case, in that it is aware of 
 *	both small and large file interfaces. It must be built
 *	in the small compilation environment.
 */

#define	close		_close
#define	lseek		_lseek
#define	lseek64		_lseek64
#define	open		_open
#define	open64		_open64
	
#include <stdio.h>
#include "stdiom.h"
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <thread.h>
#include <synch.h>
#include <mtlib.h>

/*
 * open UNIX file name, associate with iop
 */

FILE *
_endopen(const char *name, const char *type, FILE * iop, int largefile)
{
	register int oflag, plus, fd;
#ifdef _REENTRANT
	rmutex_t *lk;
#endif _REENTRANT

	if (iop == NULL)
		return (NULL);
	switch (type[0])
	{
	default:
		return (NULL);
	case 'r':
		oflag = O_RDONLY;
		break;
	case 'w':
		oflag = O_WRONLY | O_TRUNC | O_CREAT;
		break;
	case 'a':
		oflag = O_WRONLY | O_APPEND | O_CREAT;
		break;
	}
	/* UNIX ignores 'b' and treats text and binary the same */
	if ((plus = type[1]) == 'b')
		plus = type[2];
	if (plus == '+')
		oflag = (oflag & ~(O_RDONLY | O_WRONLY)) | O_RDWR;

	/* select small or large file open based on flag */
	if(largefile) {
		fd = open64(name, oflag, 0666);
	} else {
		fd = open(name, oflag, 0666);
	}
	if (fd < 0)
		return (NULL);
	if (fd > UCHAR_MAX) {
		(void) close(fd);
		return (NULL);
	}
	iop->_file = (unsigned char)fd; /* assume fits in unsigned char */
	FLOCKFILE(lk, iop);		/* this lock may be unnecessary */
	if (plus == '+')
		iop->_flag = _IORW;
	else if (type[0] == 'r')
		iop->_flag = _IOREAD;
	else
		iop->_flag = _IOWRT;
	FUNLOCKFILE(lk);
	if (oflag == (O_WRONLY | O_APPEND | O_CREAT)) {	/* type == "a" */
		if (lseek64(fd, (off64_t)0, SEEK_END) < (off64_t)0) {
			close(fd);
			return (NULL);
		}
	}
	return (iop);
}
