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

.ident "@(#)sigsetjmp.s 1.17      96/03/08 SMI"

#include <sys/asm_linkage.h>
#include <sys/reg.h>
#include <../assym.s>

	ANSI_PRAGMA_WEAK(sigsetjmp,function)

#include "SYS.h"

/*
 * int
 * sigsetjmp(env, savemask)
 * sigjmp_buf env;
 * int savemask;
 */
#define	MCTXTREGS	0x28	/* XXXPPC genassym does not generate correct */
				/* offsets because of different algnmnt reqs */

	.globl	__getcontext

 	ENTRY(sigsetjmp)

	mflr	%r0
	stwu	%r1,-32(%r1)
	stw	%r0,+36(%r1)
	stw	%r3, +20(%r1)
	stw	%r4, +16(%r1)


	li	%r12, UC_SIGMASK | UC_STACK
	stw	%r12, 0(%r3)		! uc->uc_flags = UC_SIGMASK | UC_STACK

	POTENTIAL_FAR_CALL(__getcontext)

	lwz	%r4, +16(%r1)
	lwz	%r3, +20(%r1)
	cmpi	%r4, 0
	bne	.mask
	lwz	%r12, 0(%r3)
	li	%r6, UC_SIGMASK
	andc.	%r12, %r12, %r6
	stw	%r12, 0(%r3)			! uc->uc_flags &= ~UC_SIGMASK

.mask:
	addi	%r6, %r1, 32			! r6 = back_chain;
	lwz	%r12,+4(%r6)			! r12 = Saved_LR;
	stw	%r6,+MCTXTREGS+(4*R_R1)(%r3)	! gregs[R_R1] = back_chain;
	stw	%r12,+MCTXTREGS+(4*R_PC)(%r3)	! gregs[R_PC] = Saved_LR;


	li	%r12, 1
	stw	%r12,+MCTXTREGS+(4*R_R3)(%r3)	! gregs[R_R3] = 1;

	li	%r3, 0

	lwz	%r0,+36(%r1)
	mtlr	%r0
	addi	%r1,%r1,32
	blr

	SET_SIZE(sigsetjmp)


/*
 * void
 * siglongjmp(env, val)
 * sigjmp_buf env;
 * int val;
 */
	.globl	_setcontext

 	ENTRY(_libc_siglongjmp)

	mflr	%r0
	stwu	%r1,-16(%r1)
	stw	%r0,+20(%r1)


	cmpi	%r4,0
	beq	..LL35

	stw	%r4,+MCTXTREGS+(4*R_R3)(%r3)	! return val;
..LL35:

	POTENTIAL_FAR_CALL(_setcontext)

	lwz	%r0,+20(%r1)
	mtlr	%r0
	addi	%r1,%r1,16
	blr

	SET_SIZE(_libc_siglongjmp)

