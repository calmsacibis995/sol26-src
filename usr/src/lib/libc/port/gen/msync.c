/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)msync.c	1.9	96/05/02 SMI"	/* SVr4.0 1.3	*/

#include "synonyms.h"
#include <sys/types.h>
#include <sys/mman.h>

#ifdef __STDC__
#pragma weak	_libc_msync = _msync
#endif

msync(addr, len, flags)
caddr_t	addr;
size_t	len;
int flags;
{
	return (memcntl(addr, len, MC_SYNC, (caddr_t) flags, 0, 0));
}
