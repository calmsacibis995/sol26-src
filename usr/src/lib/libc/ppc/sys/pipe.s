/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)pipe.s	1.4	94/07/04 SMI"

	.file	"pipe.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	pipe - create an interprocess channel
 *
 *   Syntax:	int pipe(int fildes[2]);
 *
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(pipe,function)

#include "SYS.h"


	ENTRY(pipe)
	mr	%r10, %r3		! save array pointer

	SYSTRAP(pipe)
	SYSCERROR
	stw	%r3, 0(%r10)
	stw	%r4, +4(%r10)

	RETZ

	SET_SIZE(pipe)
