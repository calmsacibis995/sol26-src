/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)getcontext.s	1.3	94/07/04 SMI"

	.file	"getcontext.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	getcontext - get current user context.
 *
 *   Syntax:	int getcontext(ucontext_t *ucp);
 *
 */

#define	SUBSYS_getcontext	0

#include "SYS.h"

	ENTRY(__getcontext)

	mr	%r4, %r3
	li	%r3, SUBSYS_getcontext

	SYSTRAP(context)
	SYSCERROR

	RET

	SET_SIZE(__getcontext)
