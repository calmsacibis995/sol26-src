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

.ident "@(#)swapctxt.s 1.12      96/01/04 SMI"

#include <sys/asm_linkage.h>
#include <sys/reg.h>
#include <../assym.s>

	ANSI_PRAGMA_WEAK(swapcontext,function)

#include "SYS.h"

/*	       +--------------+	LOW
 *	sp  -->|  back-chain  |
 *	       +--------------+
 *	       |  LR Save     |
 *	       +--------------+
 *	       |  Parms,save  |
 *	       +--------------+
 *	oldsp->|  back-chain  |
 *	       +--------------+
 *	       |  Saved LR    |
 *	       +--------------+	HIGH
 */
/*
 * int
 * swapcontext(oucp, nucp)
 * ucontext_t *oucp, *nucp;
 */
#define	MCTXTREGS	0x28	/* XXXPPC genassym does not generate correct */
				/* offsets because of different algnmnt reqs */

	.globl	__getcontext

 	ENTRY(swapcontext)

	mflr	%r0
	stwu	%r1,-32(%r1)
	stw	%r0,+36(%r1)
	stw	%r3, +20(%r1)
	stw	%r4, +16(%r1)

	li	%r12, UC_ALL
	stw	%r12, 0(%r3)
	POTENTIAL_FAR_CALL(__getcontext)
	cmpi	%r3,0
	beq	..LL34

	li	%r3, -1
	b	..LL35
..LL34:
	lwz	%r5,+20(%r1)			! restore r3 into r5
	lwz	%r12,+36(%r1)			! get Saved_LR in r12
	lwz	%r4,+16(%r1)			! restore r4
	stw	%r3,+MCTXTREGS+(4*R_R3)(%r5)	! gregs[R_R3] = 0;
	stw	%r12,+MCTXTREGS+(4*R_PC)(%r5)	! gregs[R_PC] = Saved_LR;

	addi	%r12, %r1, 32
	stw	%r12,+MCTXTREGS+(4*R_R1)(%r5)	! gregs[R_R1] = back-chain;

	mr	%r3, %r4
	POTENTIAL_FAR_CALL(_setcontext)		! setcontext(nucp);
..LL35:

	lwz	%r0,+36(%r1)
	mtlr	%r0
	addi	%r1,%r1,32
	blr

	SET_SIZE(swapcontext)

