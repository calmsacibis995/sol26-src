/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)getctxt.c	1.3	93/05/04 SMI"

#ifdef __STDC__
	#pragma weak getcontext = _getcontext
#endif
#include "synonyms.h"
#include <sys/types.h>
#include <sys/user.h>
#include <sys/ucontext.h>

#ifdef notdef
asm int *
_getfp()
{
	leal	0(%ebp),%eax
}

asm int  *
_getap()
{
	leal	8(%ebp),%eax
}
#else
int *_getfp(), *_getap();
#endif

int
getcontext(ucp)
ucontext_t *ucp;
{
	int error;
	register greg_t *cpup;
#if defined(PIC)
	greg_t oldebx = (greg_t) _getbx();
#endif /* defined(PIC) */

	ucp->uc_flags = UC_ALL;
	if (error = __getcontext(ucp))
		return error;  
	cpup = (greg_t *)&ucp->uc_mcontext.gregs;
	cpup[ EBP ] =  *((greg_t *)_getfp()); /* get old ebp off stack */
	cpup[ EIP ] =  *((greg_t *)_getfp()+1); /* get old eip off stack */
	cpup[ UESP ] =  (greg_t)_getap();	/* get old esp off stack */
#if defined(PIC)
	cpup[ EBX ] = oldebx;			/* get old ebx off stack */
#endif /* defined(PIC) */

	return 0;
}
