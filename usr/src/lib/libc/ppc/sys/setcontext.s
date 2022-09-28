/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)setcontext.s	1.3	94/07/04 SMI"

	.file	"setcontext.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	setcontext - set current user context.
 *
 *   Syntax:	int setcontext(ucontext_t *ucp);
 *
 */

#include <sys/asm_linkage.h>

#define	SUBSYS_setcontext	1

	ANSI_PRAGMA_WEAK(setcontext,function)

#include "SYS.h"

	ENTRY(setcontext)

	mr	%r4, %r3
	li	%r3, SUBSYS_setcontext

	SYSTRAP(context)
	SYSCERROR

	RET

	SET_SIZE(setcontext)
