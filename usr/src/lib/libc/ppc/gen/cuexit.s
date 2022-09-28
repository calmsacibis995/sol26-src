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

.ident "@(#)cuexit.s 1.7      96/07/23 SMI"

#include "SYS.h"

	ENTRY(exit)

	mflr	%r0		! Get the return addr in register 0
	stwu	%r1, -16(%r1)	! Save the old SP on stack and update SP
	stw	%r0, 20(%r1)	! Store return addr in stack frame
	stw	%r3, 8(%r1)	! Store arg #1 in stack frame

	POTENTIAL_FAR_CALL(_exithandle)	

	lwz	%r3, 8(%r1)	! Restore arg #1 from stack frame
	lwz	%r0, 20(%r1)	! Get return addr into register 0
	mtlr	%r0		! Put return addr back into link register
	addi	%r1, %r1, 16 	! Restore stack pointer

	SYSTRAP(exit)

	SET_SIZE(exit)
