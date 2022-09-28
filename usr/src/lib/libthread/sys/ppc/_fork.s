/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)_fork.s	1.3	94/11/14 SMI"

	.file	"_fork.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	__fork - create a new process
 *
 *   Syntax:	pid_t __fork(void);
 *
 *   Return values from kernel:
 *	parent:	%r4 == 0,	%r3 == child's pid
 *	child:	%r4 == 1,	%r3 == parent's pid
 */

#include <sys/asm_linkage.h>

#include "SYS.h"

	ENTRY(__fork)

	SYSTRAP(fork)
	SYSCERROR

	cmpwi	%r4, 0		! parent or child?
	beqlr-
	li	%r3, 0		! if child, return (0)

	RET			! return child's pid if parent

	SET_SIZE(__fork)
