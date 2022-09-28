/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */
 
	.ident "@(#)fork.s	1.6	96/03/08 SMI"

	.file	"fork.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	fork - create a new process
 *
 *   Syntax:	pid_t fork(void);
 *
 *   Return values from kernel:
 *	parent:	%r4 == 0,	%r3 == child's pid
 *	child:	%r4 == 1,	%r3 == parent's pid
 */

#include <sys/asm_linkage.h>

#include "SYS.h"

	ENTRY(_libc_fork)

	SYSTRAP(fork)
	SYSCERROR

	cmpwi	%r4, 0		! parent or child?
	beqlr-
	li	%r3, 0		! if child, return (0)

	RET			! return child's pid if parent

	SET_SIZE(_libc_fork)
