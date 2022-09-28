/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 *   Copyright (c) 1994 Sun Microsystems, Inc.  All Rights Reserved.
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	
 *	fpsetmask()	sets fp exception mask, and return old.
 *
 *   Syntax:	
 *
 */

	.ident "@(#)fpsetmask.s 1.9	95/01/03 SMI"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fpsetmask,function)
#include "synonyms.h"

	ENTRY(fpsetmask)
	stwu	%r1,-16(%r1)
	mffs	%f0
	mr	%r4,%r3
	stfd	%f0,8(%r1)
	lwz	%r0,8(%r1)
	rlwinm	%r3,%r0,29,27,31	/* rotate right 3, and get 5 bits */
	rlwimi	%r0,%r4,3,24,28		/* insert new 5 bits */
	! compute mask for sticky bits corresponding to the new exception
	! enable bits
	rlwinm	%r5,%r4,25,2,6
	andi.	%r6, %r4, 0x1		/* FP_X_INV enabled? */
	beq+	1f			/* no */
	lis	%r6, 0x1f8		/* or'in sticky bits for FP_X_INV */
	ori	%r6, %r6, 0x700
	or	%r5, %r5, %r6
1:
	andc	%r0, %r0, %r5		/* clear the sticky bits */
	stw	%r0,8(%r1)
	lfd	%f0,8(%r1)
	mtfsf	0xff, %f0		/* set new mask */
	addi	%r1,%r1,16
	blr				/* return old mask */
	SET_SIZE(fpsetmask)

