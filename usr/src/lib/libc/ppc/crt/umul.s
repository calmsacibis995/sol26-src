/*
 *   Copyright (c) 1994 Sun Microsystems, Inc.  All Rights Reserved.
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	
 *
 *   Syntax:	unsigned long long _umul32x32to64(long l1, long l2)	
 *
 */

	.ident "@(#)umul.s 1.3	94/11/29 SMI"

#include <sys/asm_linkage.h>


	ENTRY(_umul32x32to64)

	mr	%r5, %r3
	mr	%r6, %r4
	mullw	%r3, %r5, %r6		! low  order word in %r3
	mulhwu	%r4, %r5, %r6		! high order word in %r4
	blr

	SET_SIZE(_umul32x32to64)
