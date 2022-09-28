/*
 * Copyright (c) 1993-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)swap_int.s	1.2	95/05/07 SMI"

#include	<sys/asm_linkage.h>

#if defined(lint) || defined(__lint)
/*ARGSUSED*/
u_int
swap_int(u_int *addr)
{
	return (0);
}
#else	/* lint */
	.globl	swap_int

	ENTRY(swap_int)
	lwbrx	%r3,0,%r3
	blr-
	SET_SIZE(swap_int)
#endif
