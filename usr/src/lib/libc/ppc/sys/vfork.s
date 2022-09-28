/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)vfork.s	1.4	94/07/04 SMI"

	.file	"vfork.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	vfork - spawn new process in a virtual memory efficient
 *		way
 *
 *   Syntax:	pid_t vfork(void);
 *
 *   Return values from kernel:
 *	parent:	%r4 == 0,	%r3 == child's pid
 *	child:	%r4 == 1,	%r3 == parent's pid
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(vfork,function)

#include "SYS.h"

	ENTRY(vfork)

	SYSTRAP(vfork)
	SYSCERROR

	cmpwi	%r4, 0		! parent or child?
	beqlr-
	li	%r3, 0		! if child, return (0)

	RET			! return child's pid if parent

	SET_SIZE(vfork)
