/*
 *	Copyright (c) 1995 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 * Return the pc of the calling routine.
 */
#pragma ident	"@(#)caller.s	1.4	95/08/23 SMI"

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
	retl
	mov	%i7, %o0

	SET_SIZE(caller)
#endif
