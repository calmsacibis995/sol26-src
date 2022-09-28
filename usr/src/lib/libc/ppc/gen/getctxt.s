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

.ident "@(#)getctxt.s 1.12      96/01/04 SMI"

#include <sys/asm_linkage.h>
#include <sys/reg.h>
#include <../assym.s>

	ANSI_PRAGMA_WEAK(getcontext,function)

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
 * getcontext(ucp)
 * ucontext_t *ucp;
 */
#define	MCTXTREGS	0x28	/* XXXPPC genassym does not generate correct */
				/* offsets because of different algnmnt reqs */

	.globl	__getcontext

	ENTRY(getcontext)

	mflr	%r0
	stw	%r0,+4(%r1)
	stwu	%r1,-32(%r1)
	stw	%r3,+20(%r1)			! save r3

	li	%r12, UC_ALL
	stw	%r12, 0(%r3)

	POTENTIAL_FAR_CALL(__getcontext)
	cmpi	%r3,0
	beq+	..LL34

	li	%r3, -1
	b	..LL35				! return -1;
..LL34:

	lwz	%r4,+20(%r1)			! restore uc ptr
	lwz	%r12,+36(%r1)			! get Saved LR

	addi	%r5, %r1, 32			! R_R5 = back-chain

	stw	%r3,+MCTXTREGS+(4*R_R3)(%r4)	! gregs[R_R3] = 0;
	stw	%r12,+MCTXTREGS+(4*R_PC)(%r4)	! gregs[R_PC] = Saved_LR;
	stw	%r5,+MCTXTREGS+(4*R_R1)(%r4)	! gregs[R_R1] = back-chain;
						! return 0; /* r3 is 0 */
..LL35:
	lwz	%r0,+36(%r1)
	mtlr	%r0
	addi	%r1,%r1,32
	blr

	SET_SIZE(getcontext)

