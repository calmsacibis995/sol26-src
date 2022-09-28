/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)_wrtchk.c	1.7	96/04/17 SMI"	/* SVr4.0 1.5	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include "shlib.h"
#include <stdio.h>
#include "stdiom.h"
#include <errno.h>

#undef _wrtchk

int
_wrtchk(iop)	/* check permissions, correct for read & write changes */
	register FILE *iop;
{
	if ((iop->_flag & (_IOWRT | _IOEOF)) != _IOWRT) {
		if (!(iop->_flag & (_IOWRT | _IORW))) {
			iop->_flag |= _IOERR;
			errno = EBADF;
			return EOF; /* stream is not writeable */
		}
		iop->_flag = (iop->_flag & ~_IOEOF) | _IOWRT;
	}

	/* if first I/O to the stream get a buffer */
	if (iop->_base == 0 && _findbuf(iop) == 0)
		return (EOF);
	else if ((iop->_ptr == iop->_base) && !(iop->_flag & (_IOLBF | _IONBF)))
	{
		iop->_cnt = _bufend(iop) - iop->_ptr;
	}
	return 0;
}
