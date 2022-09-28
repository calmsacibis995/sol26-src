/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights reserved.
 */
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)fseeko.c	1.6	96/04/18 SMI"	/* SVr4.0 1.15	*/

/*LINTLIBRARY*/
/*
 * Seek for standard library.  Coordinates with buffering.
 */

#pragma weak fseeko64 = _fseeko64
#pragma weak fseeko = _fseeko

#include "synonyms.h"
#include <unistd.h>
#include "shlib.h"
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#include <thread.h>
#include <synch.h>
#include <mtlib.h>
#include "stdiom.h"

extern int _fflush_u();

int
fseeko64(register FILE *iop, off64_t offset, int ptrname)
{
	off64_t	p;
#ifdef _REENTRANT
	rmutex_t *lk;
#endif _REENTRANT

	FLOCKFILE(lk, iop);
	iop->_flag &= (unsigned short)~_IOEOF;

	if (!(iop->_flag & _IOREAD) && !(iop->_flag & (_IOWRT | _IORW))) {
		errno = EBADF;
		FUNLOCKFILE(lk);
		return (-1);
	}

	if (iop->_flag & _IOREAD) {
		if (ptrname == 1 && iop->_base && !(iop->_flag&_IONBF)) {
			offset -= iop->_cnt;
		}
	} else if (iop->_flag & (_IOWRT | _IORW)) {
		if (_fflush_u(iop) == EOF) {
			FUNLOCKFILE(lk);
			return (-1);
		}
	}
	iop->_cnt = 0;
	iop->_ptr = iop->_base;
	if (iop->_flag & _IORW) {
		iop->_flag &= (unsigned short)~(_IOREAD | _IOWRT);
	}
	p = lseek64(FILENO(iop), offset, ptrname);
	FUNLOCKFILE(lk);
	return ((p == -1)? -1: 0);
}

int
fseeko(register FILE *iop, off_t offset, int ptrname)
{
	return (fseek(iop, offset, ptrname));
}
