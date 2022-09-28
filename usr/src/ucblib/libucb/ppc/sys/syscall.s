/*
 *   Copyright (c) 1994 Sun Microsystems, Inc.  All Rights Reserved.
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	
 *
 *   Syntax:	
 *
 */

	.ident "@(#)syscall.s 1.3	96/10/03 SMI"
	.file	"syscall.s"


#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(syscall,function)

#include "SYS.h"

	ENTRY(syscall)

	mr	%r0, %r3	! ripple all argument registers
	mr	%r3, %r4	! XXXPPC assume arguments to system
	mr	%r4, %r5	! call are less than 8
	mr	%r5, %r6
	mr	%r6, %r7
	mr	%r7, %r8
	mr	%r8, %r9
	mr	%r9, %r10

	sc			! syscall trap

	SYSCERROR

	RET

	SET_SIZE(syscall)
