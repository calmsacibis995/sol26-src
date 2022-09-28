/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)rewind.c	1.15	96/02/26 SMI"	/* SVr4.0 1.7	*/

/*	3.0 SID #	1.2	*/
/*LINTLIBRARY*/

#include "synonyms.h"
#include <stdio.h>
#include <unistd.h>
#include <synch.h>
#include <thread.h>
#include <mtlib.h>
#include "stdiom.h"

#ifdef _REENTRANT

extern void _rewind_unlocked(FILE *);

void
rewind(FILE *iop)
{
	rmutex_t *lk;

	FLOCKFILE(lk, iop);
	_rewind_unlocked(iop);
	FUNLOCKFILE(lk);
}
#else
#define	_rewind_unlocked rewind
#endif

void
_rewind_unlocked(FILE *iop)
{
	(void) _fflush_u(iop);
	(void) lseek64(FILENO(iop), 0, SEEK_SET);
	iop->_cnt = 0;
	iop->_ptr = iop->_base;
	iop->_flag &= (unsigned short)~(_IOERR | _IOEOF);
	if (iop->_flag & _IORW)
		iop->_flag &= (unsigned short)~(_IOREAD | _IOWRT);
}
