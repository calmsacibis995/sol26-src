/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 *	Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#pragma	ident "@(#)v86locore.s	1.3	93/11/29 SMI"

#ifdef _VPIX

#include <sys/asm_linkage.h>

#if !(defined(lint) || defined(__lint))
#include "assym.s"
#endif	/* lint */

#if defined(lint) || defined(__lint)

/*
 * v86rtt() - return to a dual mode process (VPIX process).
 */

/* ARGSUSED */
void
v86rtt()
{}

#else	/* lint */

	.set	VMFLAG, 0x20000		/ virtual 86 mode flag
	.set	NTBIT, 0x4000
	.set	TASK_BUSY, 0x2
	.set	XTSS_ACCESS_BYTE,GDT_XTSSSEL+0x5 / access byte in the descriptor

	.globl v86vint
	.globl KIDT2ptr

	/*
	 * We are going to user mode of a dual mode process (v86 or
	 * 386 mode). Call v86vint() to process virtual interrupts.
	 * (Note: We get here thru 'call' instruction (in locore.s) but we
	 *	  do 'iret' to go back to user mode. We could have used 
	 *	  jump to get here but MERGE386 needs to return to the
	 *	  caller.)
	 */
	ENTRY_NP(v86rtt)
	popl	%eax			/ pop off the return pc from stack
	movl	$1, %eax		/ assume v86 mode
	testl	$VMFLAG, REGS_EFL(%esp)	/ going to v86 mode?
	jnz	was_v86			/ yes

	/
	/ Set the NT bit of 386 mode task. XXX
	/
	orl	$NTBIT, REGS_EFL(%esp)

	/
	/ Set the BUSY bit in the XTSS descriptor (of v86 mode task). XXX
	/
	movl	%gs:CPU_GDT, %eax
	orb	$TASK_BUSY, XTSS_ACCESS_BYTE(%eax)
	xor	%eax, %eax		/ flag 386 user mode
was_v86:
	movl	%esp, %ecx		/ get &regs[] into ecx
	pushl	%eax			/ 1->v86mode, 0->386mode
	pushl	%ecx			/ pointer to regs
	call	v86vint			/ process virtual interrupts
	addl	$8, %esp
	
	cli
	popl	%gs 			/ restore user segment selectors
	popl	%fs
	popl	%es
	popl	%ds
	popa				/ restore general registers
	addl	$8, %esp		/ get TRAPNO and ERR off the stack
	lidt	%cs:KIDT2ptr		/ use the IDT2
	iret
	SET_SIZE(v86rtt)

#endif	/* lint */

#endif	/* _VPIX */
