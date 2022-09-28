/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights reserved.
 */
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)ftello.c	1.5	96/04/18 SMI"	/* SVr4.0 1.13	*/

/*LINTLIBRARY*/
/*
 * Return file offset.
 * Coordinates with buffering.
 */

#pragma weak ftello64 = _ftello64
#pragma weak ftello = _ftello

#include "synonyms.h"
#include <unistd.h>
#include <stdio.h>
#include <thread.h>
#include <synch.h>
#include <mtlib.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <stdio.h>
#include <stddef.h>
#include "stdiom.h"

off64_t
ftello64(register FILE *iop)
{
	ptrdiff_t adjust;
	off64_t	tres;
#ifdef _REENTRANT
	rmutex_t *lk;
#endif _REENTRANT

	FLOCKFILE(lk, iop);
	if (iop->_cnt < 0)
		iop->_cnt = 0;
	if (iop->_flag & _IOREAD)
		adjust = (ptrdiff_t) -iop->_cnt;
	else if (iop->_flag & (_IOWRT | _IORW)) {
		adjust = 0;
		if (((iop->_flag & (_IOWRT | _IONBF)) == _IOWRT) &&
						(iop->_base != 0))
			adjust = iop->_ptr - iop->_base;
	} else {
		errno = EBADF;	/* file descriptor refers to no open file */
		FUNLOCKFILE(lk);
		return ((off64_t) EOF);
	}
	tres = lseek64(FILENO(iop), 0, SEEK_CUR);
	if (tres >= 0)
		tres += (off64_t)adjust;
	FUNLOCKFILE(lk);
	return (tres);
}

off_t
ftello(register FILE *iop)
{
	return ((off_t)ftell(iop));
}

