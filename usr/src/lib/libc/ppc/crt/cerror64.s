/*
 *   Copyright (c) 1994 Sun Microsystems, Inc.  All Rights Reserved.
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	_cerror64 - C return sequence which sets errno, returns -1.
 *		(64-bit version)
 *
 *   Syntax:	
 *
 */

.ident "@(#)cerror64.s 1.7    94/09/22 SMI"

#include "SYS.h"

! C return sequence which sets errno, returns -1.
! This code should only be called by system calls which have done the prologue
 
        .globl  ___errno
 
	ENTRY(_cerror64)

        mflr    %r0
        stwu    %r1, -16(%r1)		! set back chain, allocate stack
        stw     %r0,+20(%r1)		! save LR contents
	stw	%r31,+8(%r1)
 
	mr	%r31, %r3
	cmpwi	%r31, ERESTART
	bne	.L1
	li	%r31, EINTR
.L1:
	POTENTIAL_FAR_CALL(___errno)	! call ___errno()
	stw	%r31, 0(%r3)
	li	%r3, -1
	li	%r4, -1

	lwz	%r0,+20(%r1)
	lwz	%r31,+8(%r1)
	mtlr	%r0	
	addi	%r1, %r1, 16
	blr

	SET_SIZE(_cerror64)

