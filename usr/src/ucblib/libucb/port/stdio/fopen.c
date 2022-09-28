/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)fopen.c	1.3	96/09/11 SMI"	/* SVr4.0 1.2	*/

/*******************************************************************

		PROPRIETARY NOTICE (Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.



		Copyright Notice 

Notice of copyright on this source code product does not indicate 
publication.

	(c) 1986,1987,1988,1989,1996  Sun Microsystems, Inc
	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
	          All rights reserved.
********************************************************************/ 

/*LINTLIBRARY*/
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>

extern FILE *_findiop(void);
extern FILE *_endopen(const char *, const char *, FILE *, int);

FILE *
_endopen(const char *file, const char *mode, FILE *iop, int largefile)
{
	register int	plus, oflag, fd;

	if (iop == NULL || file == NULL || file[0] == '\0')
		return (NULL);
	plus = (mode[1] == '+');
	switch (mode[0]) {
	case 'w':
		oflag = (plus ? O_RDWR : O_WRONLY) | O_TRUNC | O_CREAT;
		break;
	case 'a':
		oflag = (plus ? O_RDWR : O_WRONLY) | O_CREAT;
		break;
	case 'r':
		oflag = plus ? O_RDWR : O_RDONLY;
		break;
	default:
		return (NULL);
	}
	if(largefile) {
		fd = open64(file, oflag, 0666);
	} else {
		fd = open(file, oflag, 0666);
	}
	if (fd < 0)
		return (NULL);
	iop->_cnt = 0;
	iop->_file = fd;
	iop->_flag = plus ? _IORW : (mode[0] == 'r') ? _IOREAD : _IOWRT;
	if (mode[0] == 'a')   {
		if ((lseek64(fd, 0L, SEEK_END)) < 0)  {
			(void) close(fd);
			return NULL;
		}
	}
	iop->_base = iop->_ptr = NULL;
	/*
	 * Sys5 does not support _bufsiz
	 *
	 * iop->_bufsiz = 0;
	 */
	return (iop);
}

FILE *
fopen(const char *file, const char *mode)
{
	return (_endopen(file, mode, _findiop(), B_FALSE));
}

FILE *
fopen64(const char *file, const char *mode)
{
	return (_endopen(file, mode, _findiop(), B_TRUE));
}

FILE *
freopen(const char *file, const char *mode, FILE *iop)
{
	(void) fclose(iop); /* doesn't matter if this fails */
	return (_endopen(file, mode, iop, B_FALSE));
}

FILE *
freopen64(const char *file, const char *mode, FILE *iop)
{
	(void) fclose(iop); /* doesn't matter if this fails */
	return (_endopen(file, mode, iop, B_TRUE));
}
