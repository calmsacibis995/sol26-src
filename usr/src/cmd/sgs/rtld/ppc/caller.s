/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 * Return the pc of the calling routine.
 * (actually returns the return address of the caller's caller)
 */
#pragma ident	"@(#)caller.s	1.7	96/06/07 SMI"

#if	defined(lint)

unsigned long
caller()
{
	return (0);
}

#else

#include	<sys/asm_linkage.h>

	.file	"caller.s"

	ENTRY(caller)
	lwz	%r3,0(%r1)		# go to previous frame
	lwz	%r3,4(%r3)		# get return address out of that frame
	blr
	SET_SIZE(caller)
#endif
