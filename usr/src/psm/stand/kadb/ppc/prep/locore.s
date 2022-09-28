/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#if !defined(lint)
	.file	"locore.s"
	.ident	"@(#)locore.s	1.24	96/07/03 SMI"
#endif

#include <sys/reg.h>
#include <sys/psw.h>
#include <sys/asm_linkage.h>
#include <sys/stack.h>
#include <sys/machparam.h>
#define	BAT3U	534	/* 601-BAT3U or IBAT3U */
#define	BAT3L	535	/* 601-BAT3L or IBAT3L */
#define	DBAT3U	542
#define	DBAT3L	543
#define	L0_BAT3U_VAL	0x14	/* for 601 BAT3 Upper */
#define	L0_BAT3L_VAL	0x7f	/* for 601 BAT3 Lower */
#define	L0_IBAT3U_VAL	0xfe	/* for PowerPC IBAT3 Upper */
#define	L0_IBAT3L_VAL	0x12	/* for PowerPC IBAT3 Lower */

/*
 * The kadb stack's are used as follows:
 *  The 12k area that starts at rstk up till mstk-1 is our running kadb stack
 *	ie our context uses this.
 *  The mstk is our tmp setup stack used for kadb initialization and that is
 *	used by the app we will debug until it sets up its own stack.
 *	We could reuse this area, but we do not know when its safe to do-so.
 *  The value in estack is the top of the kadb stack. This is used for fault
 *	checking in the exception handler.
 */

#if !defined(lint)

	.comm	rstk,0x3000,4	/* set up our real 12K stack */
	.comm	mstk,0x1000,4	/* our setup stack and apps start up stack */

	.data
	.align	2
	.globl	estack
	.globl	CIFp
	.globl	bopp
	.globl	initstk
	.globl	oflag
	.type	oflag,@object
	.size	oflag,4
estack:	.long	rstk+0x2f00	/* The end of our stack (its empty here) */
CIFp:	.long	0		/* boot services - common entry point */
bopp:	.long	0		/* boot services - common entry point */
initstk:.long	mstk+0xf00
oflag:	.long	0		/* NO octal */

/*
 * offsets into the reg structure from the stack pointer
 */
#define	REGS_R0  (R_R0  * 4 + MINFRAME)
#define	REGS_R1  (R_R1  * 4 + MINFRAME)
#define	REGS_R2  (R_R2  * 4 + MINFRAME)
#define	REGS_R3  (R_R3  * 4 + MINFRAME)
#define	REGS_R4  (R_R4  * 4 + MINFRAME)
#define	REGS_R5  (R_R5  * 4 + MINFRAME)
#define	REGS_R6  (R_R6  * 4 + MINFRAME)
#define	REGS_R7  (R_R7  * 4 + MINFRAME)
#define	REGS_R8  (R_R8  * 4 + MINFRAME)
#define	REGS_R9  (R_R9  * 4 + MINFRAME)
#define	REGS_R10 (R_R10 * 4 + MINFRAME)
#define	REGS_R11 (R_R11 * 4 + MINFRAME)
#define	REGS_R12 (R_R12 * 4 + MINFRAME)
#define	REGS_R13 (R_R13 * 4 + MINFRAME)
#define	REGS_R14 (R_R14 * 4 + MINFRAME)
#define	REGS_R15 (R_R15 * 4 + MINFRAME)
#define	REGS_R16 (R_R16 * 4 + MINFRAME)
#define	REGS_R17 (R_R17 * 4 + MINFRAME)
#define	REGS_R18 (R_R18 * 4 + MINFRAME)
#define	REGS_R19 (R_R19 * 4 + MINFRAME)
#define	REGS_R20 (R_R20 * 4 + MINFRAME)
#define	REGS_R21 (R_R21 * 4 + MINFRAME)
#define	REGS_R22 (R_R22 * 4 + MINFRAME)
#define	REGS_R23 (R_R23 * 4 + MINFRAME)
#define	REGS_R24 (R_R24 * 4 + MINFRAME)
#define	REGS_R25 (R_R25 * 4 + MINFRAME)
#define	REGS_R26 (R_R26 * 4 + MINFRAME)
#define	REGS_R27 (R_R27 * 4 + MINFRAME)
#define	REGS_R28 (R_R28 * 4 + MINFRAME)
#define	REGS_R29 (R_R29 * 4 + MINFRAME)
#define	REGS_R30 (R_R30 * 4 + MINFRAME)
#define	REGS_R31 (R_R31 * 4 + MINFRAME)
#define	REGS_PC  (R_PC  * 4 + MINFRAME)
#define	REGS_CR  (R_CR  * 4 + MINFRAME)
#define	REGS_LR  (R_LR  * 4 + MINFRAME)
#define	REGS_MSR (R_MSR * 4 + MINFRAME)
#define	REGS_CTR (R_CTR * 4 + MINFRAME)
#define	REGS_XER (R_XER * 4 + MINFRAME)

/*
 * This is where it all starts, first code run in kadb.
 *
 * This code assumes that the secondary boot has passed in the
 * following arguments:
 *
 *	r3 - &CIFp (address of a "void *p1275cookie")
 *	r4 - dvec  (0 when called from boot)
 *	r5 - &bopp (address of a pointer to a struct bootops)
 */

	.text
	.globl  _start
	.globl  start
_start:
start:
! save CIFp
	lwz	%r8,0(%r3)
	lis	%r7,CIFp@ha
	stw	%r8,CIFp@l(%r7)

! save bopp

	lwz	%r8,0(%r5)
	lis	%r7,bopp@ha
	stw	%r8,bopp@l(%r7)
	
/*	At this point, we are still running on the boot stack. */
/*	We will now find our stack and switch to it. */

! set up kadb's current stack to be mstk
	lis	%r1,mstk@ha
	la	%r1,mstk@l(%r1)
	addi	%r1,%r1,0x1000-0x100

! XXXPPC - Clear out our bss space.  Needed?  Do it earlier ?

! Initialize interrupt handling
	addi	%r3,%r1,MINFRAME	! an "OK" register save area
	bl	main

! Start the application here, REALLY!
	addi	%r1,%r1,-0x100		! create a big enough frame
	mfmsr	%r3
	stw	%r3,REGS_MSR(%r1)	! save an "OK" value for msr
	mfcr	%r3
	stw	%r3,REGS_CR(%r1)
	mfxer	%r3
	stw	%r3,REGS_XER(%r1)	! save an "OK" value for msr
	mfctr	%r3
	stw	%r3,REGS_CTR(%r1)	! save an "OK" value for msr
	li	%r11,0
	stw	%r11,REGS_LR(%r1)
	li	%r12,0
	li	%r13,0
	li	%r14,0
	li	%r15,0
	li	%r16,0
	li	%r17,0
	li	%r18,0
	li	%r19,0
	li	%r20,0
	li	%r21,0
	li	%r22,0
	li	%r23,0
	li	%r24,0
	li	%r25,0
	li	%r26,0
	li	%r27,0
	li	%r28,0
	li	%r29,0
	li	%r30,0
	li	%r31,0
	b	.level1_common		! call fault() the 1st time.

/*
 * This is the low level call to exit from the debugger into the
 * requested standalone application.
 * The argument to this call is the destination function pointer.
 */
	.globl	_exitto
_exitto:
	blr		! just return to main(), which will return
			! to our call to main above.

/* get our current stack pointer for a c routine */
/* to check which stack we are using */
	.globl	getsp
getsp:
	mr	%r3,%r1
	blr

	.globl	delayl
delayl:
	blr

/*
 * Entry point from the level 1 kernel trap handler for exceptions
 * that KADB has need to examine.  Upon entry, a normal level 1
 * stack frame has been created, and the only registers of that have
 * unsaved values are r1, r2, r4, r5, r6, and r20.  We need to save
 * these registers (in nonvolatile registers) to handle the case
 * where the kernel needs to service this trap instead of KADB.
 * Upon entry here, r3 has the pointer to the regs structure
 * (argument to fault()), and the link register is our return address.
 */

	.text
	.globl	from_the_kernel
from_the_kernel:
	mr	%r21,%r1
	mr	%r22,%r2
	mr	%r24,%r4
	mr	%r25,%r5
	mr	%r26,%r6
	mr	%r30,%r20
	mflr	%r31

	bl	enable_bat0
	bl	steal_fault_handlers
	isync
	bl	restore_bat0

	bl	fault

	bl	enable_bat0
	bl	restore_fault_handlers
	isync
	bl	restore_bat0

	cmpi	%r3,0
	mtlr	%r31
	bne	rtn_to_kernel	! branch if kernel is to handle fault

/*
 * kadb has handled the fault, so the kernel should just restore
 * the state and return from interrupt.
 */

	li	%r3,0	! kadb handled this.
	blr		! continues at "kadb_returns_here_always"
			! in the kernel's locore.s

! restore saved registers, then return to the kernel
rtn_to_kernel:
	mr	%r1,%r21
	mr	%r2,%r22
	mr	%r4,%r24
	mr	%r5,%r25
	mr	%r6,%r26
	mr	%r20,%r30
	li	%r3,1	! kernel should handle this.
	blr		! continues at "kadb_returns_here_always"
			! in the kernel's locore.s

/*
 * Entry point for exception handling before the kernel has installed
 * its exception handlers.  This code is needed to debug early kernel
 * code.
 *
 * Level 1 trap handler.  This is used until the kernel installs its
 * level 0 handlers.  This is needed for debugging early kernel code.
 */

	.data
	.comm	.level1_msr,4,2	! for transfer from level 0 to level 1

	.text
.level1_common:
/*
 * save state
 *
 *    assumptions:
 *	r1, r2, r3, r4, r5, r6, pc, msr are saved
 *	r1 is new stack pointer, r4 is trap type, r5 is dar, r6 is dsisr
 */
	stw	%r0,REGS_R0(%r1)
	stw	%r7,REGS_R7(%r1)
	stw	%r8,REGS_R8(%r1)
	stw	%r9,REGS_R9(%r1)
	mfcr	%r7
	stw	%r10,REGS_R10(%r1)
	stw	%r11,REGS_R11(%r1)
	stw	%r12,REGS_R12(%r1)
	mflr	%r8
	stw	%r13,REGS_R13(%r1)
	stw	%r14,REGS_R14(%r1)
	stw	%r15,REGS_R15(%r1)
	mfctr	%r9
	stw	%r16,REGS_R16(%r1)
	stw	%r17,REGS_R17(%r1)
	stw	%r18,REGS_R18(%r1)
	mfxer	%r10
	stw	%r19,REGS_R19(%r1)
	stw	%r20,REGS_R20(%r1)
	stw	%r21,REGS_R21(%r1)
	stw	%r22,REGS_R22(%r1)
	stw	%r23,REGS_R23(%r1)
	stw	%r24,REGS_R24(%r1)
	stw	%r25,REGS_R25(%r1)
	stw	%r26,REGS_R26(%r1)
	stw	%r27,REGS_R27(%r1)
	stw	%r28,REGS_R28(%r1)
	stw	%r29,REGS_R29(%r1)
	stw	%r30,REGS_R30(%r1)
	stw	%r31,REGS_R31(%r1)

	stw	%r7,REGS_CR(%r1)
	stw	%r8,REGS_LR(%r1)
	stw	%r9,REGS_CTR(%r1)
	stw	%r10,REGS_XER(%r1)

! enter the debugger
	addi	%r3,%r1,MINFRAME
	bl	fault		! fault(&regs, type, dar, dsisr)

! restore state and return through low memory and an rfi
	mfpvr	%r3
	sri	%r3,%r3,16
	cmpi	%r3,1
	bne	1f
	li	%r3, L0_BAT3U_VAL
	li	%r4, L0_BAT3L_VAL
	mtspr	BAT3U, %r3		! BATU3 or IBATU3
	mtspr	BAT3L, %r4		! BATL3 or IBATL3
	b	2f

1:	li	%r3, L0_IBAT3U_VAL
	li	%r4, L0_IBAT3L_VAL
	mtspr	BAT3U, %r3		! BATU3 or IBATU3
	mtspr	BAT3L, %r4		! BATL3 or IBATL3
	mtspr	DBAT3U, %r3		! BATU3 or IBATU3
	mtspr	DBAT3L, %r4		! BATL3 or IBATL3
2:
	isync
	mfsprg2	%r6		! physical save area
	lwz	%r3,REGS_R3(%r1)
	lwz	%r4,REGS_PC(%r1)
	lwz	%r5,REGS_MSR(%r1)
	stw	%r3,0(%r6)
	stw	%r4,4(%r6)
	stw	%r5,8(%r6)
	lwz	%r6,REGS_LR(%r1)
	lwz	%r7,REGS_CR(%r1)
	mtlr	%r6
	lwz	%r8,REGS_CTR(%r1)
	mtcrf	0xff,%r7
	lwz	%r9,REGS_XER(%r1)
	mtctr	%r8
	mtxer	%r9
	lwz	%r0,REGS_R0(%r1)
	lwz	%r2,REGS_R2(%r1)
	lwz	%r4,REGS_R4(%r1)
	lwz	%r5,REGS_R5(%r1)
	lwz	%r6,REGS_R6(%r1)
	lwz	%r7,REGS_R7(%r1)
	lwz	%r8,REGS_R8(%r1)
	lwz	%r9,REGS_R9(%r1)
	lwz	%r10,REGS_R10(%r1)
	lwz	%r11,REGS_R11(%r1)
	lwz	%r12,REGS_R12(%r1)
	lwz	%r13,REGS_R13(%r1)
	lwz	%r14,REGS_R14(%r1)
	lwz	%r15,REGS_R15(%r1)
	lwz	%r16,REGS_R16(%r1)
	lwz	%r17,REGS_R17(%r1)
	lwz	%r18,REGS_R18(%r1)
	lwz	%r19,REGS_R19(%r1)
	lwz	%r20,REGS_R20(%r1)
	lwz	%r21,REGS_R21(%r1)
	lwz	%r22,REGS_R22(%r1)
	lwz	%r23,REGS_R23(%r1)
	lwz	%r24,REGS_R24(%r1)
	lwz	%r25,REGS_R25(%r1)
	lwz	%r26,REGS_R26(%r1)
	lwz	%r27,REGS_R27(%r1)
	lwz	%r28,REGS_R28(%r1)
	lwz	%r29,REGS_R29(%r1)
	lwz	%r30,REGS_R30(%r1)
	lwz	%r31,REGS_R31(%r1)
	lwz	%r1,REGS_R1(%r1)
	ba	0x2100



/*
 * Level 0 code for trap 0x300, data storage
 */
.300:
	mtsprg	3,%r2		! save r2
	mfsprg	%r2,2		! physical save area
	stw	%r3,0(%r2)	! save r3
	mfsrr0	%r3
	stw	%r3,4(%r2)	! save srr0
	mfsrr1	%r3
	stw	%r3,8(%r2)	! save srr1
	mfmsr	%r3
	ori	%r3,%r3,MSR_DR	! enable virtual addressing for data
	mtmsr	%r3		! enabled
	isync
	subi	%r1,%r1,0x100	! allocate stack frame
	mfsprg	%r3,3		! fetch original r2
	stw	%r3,REGS_R2(%r1)
	addi	%r3,%r1,0x100	! compute original r1
	stw	%r3,REGS_R1(%r1)
	stw	%r4,REGS_R4(%r1)
	stw	%r5,REGS_R5(%r1)
	stw	%r6,REGS_R6(%r1)

	mfmsr	%r3
	rlwinm	%r3,%r3,0,28,26	! disable virtual addressing for data
	mtmsr	%r3		! disabled
	isync
	lwz	%r4,0(%r2)	! fetch original r3
	lwz	%r5,4(%r2)	! fetch original srr0
	lwz	%r6,8(%r2)	! fetch original srr1
	mfmsr	%r3
	ori	%r3,%r3,MSR_DR	! enable virtual addressing for data
	mtmsr	%r3		! enabled
	isync

	stw	%r4,REGS_R3(%r1)
	stw	%r5,REGS_PC(%r1)
	stw	%r6,REGS_MSR(%r1)
	lis	%r3,.level1_msr@ha
	lwz	%r3,.level1_msr@l(%r3)

	mfmsr	%r4
	rlwinm	%r4,%r4,0,28,26	! disable virtual addressing for data
	mtmsr	%r4		! disabled
	isync

	mtsrr1	%r3
	lis	%r3,.level1_common@ha
	la	%r3,.level1_common@l(%r3)
	mtsrr0	%r3
	li	%r4,0x300
	mfdar	%r5
	mfdsisr	%r6
	rfi

/*
 * Level 0 code for trap 0x400, instruction storage
 */
.400:
	mtsprg	3,%r2		! save r2
	mfsprg	%r2,2		! physical save area
	stw	%r3,0(%r2)	! save r3
	mfsrr0	%r3
	stw	%r3,4(%r2)	! save srr0
	mfsrr1	%r3
	stw	%r3,8(%r2)	! save srr1
	mfmsr	%r3
	ori	%r3,%r3,MSR_DR	! enable virtual addressing for data
	mtmsr	%r3		! enabled
	isync
	subi	%r1,%r1,0x100	! allocate stack frame
	mfsprg	%r3,3		! fetch original r2
	stw	%r3,REGS_R2(%r1)
	addi	%r3,%r1,0x100	! compute original r1
	stw	%r3,REGS_R1(%r1)
	stw	%r4,REGS_R4(%r1)
	stw	%r5,REGS_R5(%r1)
	stw	%r6,REGS_R6(%r1)

	mfmsr	%r3
	rlwinm	%r3,%r3,0,28,26	! disable virtual addressing for data
	mtmsr	%r3		! disabled
	isync
	lwz	%r4,0(%r2)	! fetch original r3
	lwz	%r5,4(%r2)	! fetch original srr0
	lwz	%r6,8(%r2)	! fetch original srr1
	mfmsr	%r3
	ori	%r3,%r3,MSR_DR	! enable virtual addressing for data
	mtmsr	%r3		! enabled
	isync

	stw	%r4,REGS_R3(%r1)
	stw	%r5,REGS_PC(%r1)
	stw	%r6,REGS_MSR(%r1)
	lis	%r3,.level1_msr@ha
	lwz	%r3,.level1_msr@l(%r3)

	mfmsr	%r4
	rlwinm	%r4,%r4,0,28,26	! disable virtual addressing for data
	mtmsr	%r4		! disabled
	isync

	mtsrr1	%r3
	lis	%r3,.level1_common@ha
	la	%r3,.level1_common@l(%r3)
	mtsrr0	%r3
	li	%r4,0x400
	mfdar	%r5
	mfdsisr	%r6
	rfi

/*
 * Level 0 code for trap 0x600, alignment exception
 */
.600:
	mtsprg	3,%r2		! save r2
	mfsprg	%r2,2		! physical save area
	stw	%r3,0(%r2)	! save r3
	mfsrr0	%r3
	stw	%r3,4(%r2)	! save srr0
	mfsrr1	%r3
	stw	%r3,8(%r2)	! save srr1
	mfmsr	%r3
	ori	%r3,%r3,MSR_DR	! enable virtual addressing for data
	mtmsr	%r3		! enabled
	isync
	subi	%r1,%r1,0x100	! allocate stack frame
	mfsprg	%r3,3		! fetch original r2
	stw	%r3,REGS_R2(%r1)
	addi	%r3,%r1,0x100	! compute original r1
	stw	%r3,REGS_R1(%r1)
	stw	%r4,REGS_R4(%r1)
	stw	%r5,REGS_R5(%r1)
	stw	%r6,REGS_R6(%r1)

	mfmsr	%r3
	rlwinm	%r3,%r3,0,28,26	! disable virtual addressing for data
	mtmsr	%r3		! disabled
	isync
	lwz	%r4,0(%r2)	! fetch original r3
	lwz	%r5,4(%r2)	! fetch original srr0
	lwz	%r6,8(%r2)	! fetch original srr1
	mfmsr	%r3
	ori	%r3,%r3,MSR_DR	! enable virtual addressing for data
	mtmsr	%r3		! enabled
	isync

	stw	%r4,REGS_R3(%r1)
	stw	%r5,REGS_PC(%r1)
	stw	%r6,REGS_MSR(%r1)
	lis	%r3,.level1_msr@ha
	lwz	%r3,.level1_msr@l(%r3)

	mfmsr	%r4
	rlwinm	%r4,%r4,0,28,26	! disable virtual addressing for data
	mtmsr	%r4		! disabled
	isync

	mtsrr1	%r3
	lis	%r3,.level1_common@ha
	la	%r3,.level1_common@l(%r3)
	mtsrr0	%r3
	li	%r4,0x600
	mfdar	%r5
	mfdsisr	%r6
	rfi

/*
 * Level 0 code for trap 0x700, program check
 */
.700:
	mtsprg	3,%r2		! save r2
	mfsprg	%r2,2		! physical save area
	stw	%r3,0(%r2)	! save r3
	mfsrr0	%r3
	stw	%r3,4(%r2)	! save srr0
	mfsrr1	%r3
	stw	%r3,8(%r2)	! save srr1
	mfmsr	%r3
	ori	%r3,%r3,MSR_DR	! enable virtual addressing for data
	mtmsr	%r3		! enabled
	isync
	subi	%r1,%r1,0x100	! allocate stack frame
	mfsprg	%r3,3		! fetch original r2
	stw	%r3,REGS_R2(%r1)
	addi	%r3,%r1,0x100	! compute original r1
	stw	%r3,REGS_R1(%r1)
	stw	%r4,REGS_R4(%r1)
	stw	%r5,REGS_R5(%r1)
	stw	%r6,REGS_R6(%r1)

	mfmsr	%r3
	rlwinm	%r3,%r3,0,28,26	! disable virtual addressing for data
	mtmsr	%r3		! disabled
	isync
	lwz	%r4,0(%r2)	! fetch original r3
	lwz	%r5,4(%r2)	! fetch original srr0
	lwz	%r6,8(%r2)	! fetch original srr1
	mfmsr	%r3
	ori	%r3,%r3,MSR_DR	! enable virtual addressing for data
	mtmsr	%r3		! enabled
	isync

	stw	%r4,REGS_R3(%r1)
	stw	%r5,REGS_PC(%r1)
	stw	%r6,REGS_MSR(%r1)
	lis	%r3,.level1_msr@ha
	lwz	%r3,.level1_msr@l(%r3)

	mfmsr	%r4
	rlwinm	%r4,%r4,0,28,26	! disable virtual addressing for data
	mtmsr	%r4		! disabled
	isync

	mtsrr1	%r3
	lis	%r3,.level1_common@ha
	la	%r3,.level1_common@l(%r3)
	mtsrr0	%r3
	li	%r4,0x700
	mfdar	%r5
	mfdsisr	%r6
	rfi

/*
 * Level 0 code for trap 0x900, decrementer interrupt
 */
.900:
	rfi			! just let it happen, and go on

/*
 * Level 0 code for trap 0xd00, trace exception
 */
.d00:
	mtsprg	3,%r2		! save r2
	mfsprg	%r2,2		! physical save area
	stw	%r3,0(%r2)	! save r3
	mfsrr0	%r3
	stw	%r3,4(%r2)	! save srr0
	mfsrr1	%r3
	stw	%r3,8(%r2)	! save srr1
	mfmsr	%r3
	ori	%r3,%r3,MSR_DR	! enable virtual addressing for data
	mtmsr	%r3		! enabled
	isync
	subi	%r1,%r1,0x100	! allocate stack frame
	mfsprg	%r3,3		! fetch original r2
	stw	%r3,REGS_R2(%r1)
	addi	%r3,%r1,0x100	! compute original r1
	stw	%r3,REGS_R1(%r1)
	stw	%r4,REGS_R4(%r1)
	stw	%r5,REGS_R5(%r1)
	stw	%r6,REGS_R6(%r1)

	mfmsr	%r3
	rlwinm	%r3,%r3,0,28,26	! disable virtual addressing for data
	mtmsr	%r3		! disabled
	isync
	lwz	%r4,0(%r2)	! fetch original r3
	lwz	%r5,4(%r2)	! fetch original srr0
	lwz	%r6,8(%r2)	! fetch original srr1
	mfmsr	%r3
	ori	%r3,%r3,MSR_DR	! enable virtual addressing for data
	mtmsr	%r3		! enabled
	isync

	stw	%r4,REGS_R3(%r1)
	stw	%r5,REGS_PC(%r1)
	stw	%r6,REGS_MSR(%r1)
	lis	%r3,.level1_msr@ha
	lwz	%r3,.level1_msr@l(%r3)

	mfmsr	%r4
	rlwinm	%r4,%r4,0,28,26	! disable virtual addressing for data
	mtmsr	%r4		! disabled
	isync

	mtsrr1	%r3
	lis	%r3,.level1_common@ha
	la	%r3,.level1_common@l(%r3)
	mtsrr0	%r3
	li	%r4,0xd00
	mfdar	%r5
	mfdsisr	%r6
	rfi

/*
 * Level 0 code for trap 0x2000, run-mode exception
 */
.2000:
	mtsprg	3,%r2		! save r2
	mfsprg	%r2,2		! physical save area
	stw	%r3,0(%r2)	! save r3
	mfsrr0	%r3
	stw	%r3,4(%r2)	! save srr0
	mfsrr1	%r3
	stw	%r3,8(%r2)	! save srr1
	mfmsr	%r3
	ori	%r3,%r3,MSR_DR	! enable virtual addressing for data
	mtmsr	%r3		! enabled
	isync
	subi	%r1,%r1,0x100	! allocate stack frame
	mfsprg	%r3,3		! fetch original r2
	stw	%r3,REGS_R2(%r1)
	addi	%r3,%r1,0x100	! compute original r1
	stw	%r3,REGS_R1(%r1)
	stw	%r4,REGS_R4(%r1)
	stw	%r5,REGS_R5(%r1)
	stw	%r6,REGS_R6(%r1)

	mfmsr	%r3
	rlwinm	%r3,%r3,0,28,26	! disable virtual addressing for data
	mtmsr	%r3		! disabled
	isync
	lwz	%r4,0(%r2)	! fetch original r3
	lwz	%r5,4(%r2)	! fetch original srr0
	lwz	%r6,8(%r2)	! fetch original srr1
	mfmsr	%r3
	ori	%r3,%r3,MSR_DR	! enable virtual addressing for data
	mtmsr	%r3		! enabled
	isync

	stw	%r4,REGS_R3(%r1)
	stw	%r5,REGS_PC(%r1)
	stw	%r6,REGS_MSR(%r1)
	lis	%r3,.level1_msr@ha
	lwz	%r3,.level1_msr@l(%r3)

	mfmsr	%r4
	rlwinm	%r4,%r4,0,28,26	! disable virtual addressing for data
	mtmsr	%r4		! disabled
	isync

	mtsrr1	%r3
	lis	%r3,.level1_common@ha
	la	%r3,.level1_common@l(%r3)
	mtsrr0	%r3
	li	%r4,0x2000
	mfdar	%r5
	mfdsisr	%r6
	rfi

!
! Loading srr0, loading srr1, and executing an "rfi" must always be done
! while running with both data and instruction translation disabled.
! This level 0 code complies with that.  This code is used during early
! KADB operation, i.e., only before the kernel takes over level 0.
!
! We enter here with translation enabled, r3/pc/msr in physical 0/4/8 off
! of sprg2 (physical save area).  Now that we're running 1/1 physical/virtual,
! we can disable translations prior to issuing the "rfi".
!
.2100_rtn:
	mfmsr	%r3
	rlwinm	%r3,%r3,0,28,25	! clear IR and DR
	mtmsr	%r3
	isync
	mfcr	%r3
	mtsprg3	%r3
	mfpvr	%r3
	sri	%r3,%r3,16
	cmpi	%r3,1
	li	%r3,0
	bne	1f
	mtspr	BAT3U, %r3		! BATU3 or IBATU3
	mtspr	BAT3L, %r3		! BATL3 or IBATL3
	b	2f

1:
	mtspr	BAT3U, %r3		! BATU3 or IBATU3
	mtspr	BAT3L, %r3		! BATL3 or IBATL3
	mtspr	DBAT3U, %r3		! BATU3 or IBATU3
	mtspr	DBAT3L, %r3		! BATL3 or IBATL3
2:
	mfsprg3	%r3
	mtcr	%r3
	mtsprg3	%r1
	mfsprg2	%r1
	lwz	%r3,8(%r1)	! restore srr1
	mtsrr1	%r3
	lwz	%r3,4(%r1)	! restore srr0
	mtsrr0	%r3
	lwz	%r3,0(%r1)	! restore r3
	mfsprg3	%r1
	rfi

.2100_end:

	.data
level0_handlers:
/*
 * kadb is effectively disabled until the kernel takes over
 * memory management.  The prom's data and instruction
 * access exception handlers may not be overwritten until we are
 * ready to take over memory management (which we are not in kadb).
 * This is because some realOF's use these handlers to fault
 * mappings into their page tables after they have been mapped.
 * Note that VOF does not have this dependency - we check for VOF
 * and if present, go ahead and install the 300/400 handlers for
 * early kadb debugging.
 */
	.word	.300,0x300
	.word	.400,0x400
	.word	.600,0x600
	.word	.700,0x700
	.word	.900,0x900
	.word	.d00,0xd00
	.word	.2000,0x2000
	.word	.2100_rtn,0x2100
	.word	.2100_end,0

/*
 * Register Usage:
 *	r3 - source
 *	r4 - destination
 *	r5 - end of source ( == next source)
 *	r6 - pointer into level0_handlers table
 *	r0 - tmp during word copy
 */

	.text

/*
 * set up sprg's for kadb's level 0 handlers
 *
 *	sprg2 - needs a physical address for saving registers
 *
 */
#if defined(lint)

/*ARGSUSED*/
void
install_level0_handlers(int VOF_present)
{}

#else   /* lint */
	ENTRY_NP(install_level0_handlers)

	mr	%r5, %r3
	mfpvr	%r3
	sri	%r3,%r3,16
	cmpi	%r3,1
	bne	1f
	li	%r3, L0_BAT3U_VAL
	li	%r4, L0_BAT3L_VAL
	mtspr	BAT3U, %r3		! BATU3 or IBATU3
	mtspr	BAT3L, %r4		! BATL3 or IBATL3
	b	2f

1:	li	%r3, L0_IBAT3U_VAL
	li	%r4, L0_IBAT3L_VAL
	mtspr	DBAT3U, %r3		! BATU3 or IBATU3
	mtspr	DBAT3L, %r4		! BATL3 or IBATL3
2:
	lis	%r6,level0_handlers@ha
	la	%r6,level0_handlers@l(%r6)
	addi	%r6,%r6,16 ! default is "don't install 300/400"

! Are we booted from a VOF machine...if so, its safe to install
! the 300/400 handlers for early kadb debugging
	cmpi	%r5,0
	beq	.no_vof
	subi	%r6,%r6,16	! it's OK to "install 300/400"
.no_vof:
	li	%r3,0x2200	! assume 0x2200 is a safe place to store
				! ... srr0, srr1, etc. for now.
				! also assume 0x2200 is mapped 1 to 1,
				! ... virtual to physical, until the
				! ... kernel has changed level 0 code.
	mtsprg	2,%r3
	mfmsr	%r4		! assumes current msr is OK
	lis	%r3,.level1_msr@ha
	stw	%r4,.level1_msr@l(%r3)

! install level 0 handlers
	lwz	%r5,0(%r6)
.l0_loop1:
	mr	%r3,%r5		! source
	lwzu	%r4,4(%r6)	! destination
	lwzu	%r5,4(%r6)	! end of source
	cmpi	%r4,0
	beq-	.l0_done
	subi	%r3,%r3,4
	subi	%r4,%r4,4
.l0_loop2:
	lwzu	%r0,4(%r3)
	stwu	%r0,4(%r4)
	dcbst	%r0,%r4 	! force to memory
	sync			! guarantee dcbst is done before icbi
	icbi	%r0,%r4 	! force out of instruction cache
	cmp	%r3,%r5
	bne+	.l0_loop2
	b	.l0_loop1
.l0_done:
	mfpvr	%r3
	sri	%r3,%r3,16
	cmpi	%r3,1
	li	%r3,0
	bne	1f
	mtspr	BAT3U, %r3		! BATU3 or IBATU3
	mtspr	BAT3L, %r3		! BATL3 or IBATL3
	b	2f
1:
	mtspr	DBAT3U, %r3		! BATU3 or IBATU3
	mtspr	DBAT3L, %r3		! BATL3 or IBATL3
2:
	sync
	isync
	blr
	SET_SIZE(install_level0_handlers)
#endif	/* lint */	

/*
 * Careful here.  Register usage is assumed.
 */

	.comm	save_300,0x100,2
	.comm	save_600,0x100,2

steal_fault_handlers:
	mflr	%r0

	li	%r8,0x300		! source
	lis	%r9,save_300@ha
	la	%r9,save_300@l(%r9)	! destination
	bl	copy_handler

	lis	%r8,.300@ha
	la	%r8,.300@l(%r8)		! source
	li	%r9,0x300		! destination
	bl	copy_handler

	li	%r8,0x600		! source
	lis	%r9,save_600@ha
	la	%r9,save_600@l(%r9)	! destination
	bl	copy_handler

	lis	%r8,.600@ha
	la	%r8,.600@l(%r8)		! source
	li	%r9,0x600		! destination
	bl	copy_handler

	mtlr	%r0
	blr

restore_fault_handlers:
	mflr	%r0

	lis	%r8,save_300@ha
	la	%r8,save_300@l(%r8)	! destination
	li	%r9,0x300		! source
	bl	copy_handler

	lis	%r8,save_600@ha
	la	%r8,save_600@l(%r8)	! destination
	li	%r9,0x600		! source
	bl	copy_handler

	mtlr	%r0
	blr

copy_handler:
	li	%r10,64			! copy 64 words
	mtctr	%r10
	subi	%r8,%r8,4
	subi	%r9,%r9,4
copy_handler_loop:
	lwzu	%r11,4(%r8)
	stwu	%r11,4(%r9)
	dcbst	%r0,%r9 	! force to memory
	sync			! guarantee dcbst is done before icbi
	icbi	%r0,%r9 	! force out of instruction cache
	bdnz+	copy_handler_loop
	sync
	isync
	blr

	.comm	bat0_save,8,2

	ENTRY(enable_bat0)
	mfpvr	%r10
	sri	%r10,%r10,16
	cmpi	%r10,1
	bne	1f
! PowerPC 601
	mfspr	%r11,528
	lis	%r10,bat0_save@ha
	stwu	%r11,bat0_save@l(%r10)
	mfspr	%r11,529
	stw	%r11,4(%r10)
	li	%r11,0x14
	mtspr	528,%r11
	li	%r11,0x7f
	mtspr	529,%r11
	blr
1:
	mfspr	%r11,537
	lis	%r10,bat0_save@ha
	stwu	%r11,bat0_save@l(%r10)
	mfspr	%r11,536
	stw	%r11,4(%r10)
	li	%r11,0x12
	mtspr	537,%r11
	li	%r11,0xfe
	mtspr	536,%r11
	blr
	SET_SIZE(enable_bat0)

	ENTRY(restore_bat0)
	mfpvr	%r10
	sri	%r10,%r10,16
	cmpi	%r10,1
	bne	1f
! PowerPC 601
	lis	%r10,bat0_save@ha
	lwzu	%r11,bat0_save@l(%r10)
	mtspr	528,%r11
	lwz	%r11,4(%r10)
	mtspr	529,%r11
	blr
1:
	lis	%r10,bat0_save@ha
	lwzu	%r11,bat0_save@l(%r10)
	mtspr	537,%r11
	lwz	%r11,4(%r10)
	mtspr	536,%r11
	blr
	SET_SIZE(restore_bat0)

	ENTRY(caller)
	lwz	%r3,0(%r1)
	lwz	%r3,4(%r3)
	blr

	ENTRY(caller2)
	lwz	%r3,0(%r1)
	lwz	%r3,0(%r3)
	lwz	%r3,4(%r3)
	blr

	ENTRY(abs)
	cmpi	%r3,0
	bgelr+
	subfic	%r3,%r3,0
	blr
	SET_SIZE(abs)

	ENTRY(cal1)
	lwz	%r3,0(%r1)
	lwz	%r3,0(%r3)
	lwz	%r3,0(%r3)
	lwz	%r3,0(%r3)
	cmpi	%r3,0
	beq	.ret0
	lwz	%r3,4(%r3)
	blr

	ENTRY(cal2)
	lwz	%r3,0(%r1)
	lwz	%r3,0(%r3)
	lwz	%r3,0(%r3)
	lwz	%r3,0(%r3)
	lwz	%r3,0(%r3)
	cmpi	%r3,0
	beq	.ret0
	lwz	%r3,4(%r3)
	blr


	ENTRY(cal3)
	lwz	%r3,0(%r1)
	lwz	%r3,0(%r3)
	lwz	%r3,0(%r3)
	lwz	%r3,0(%r3)
	lwz	%r3,0(%r3)
	lwz	%r3,0(%r3)
	cmpi	%r3,0
	beq	.ret0
	lwz	%r3,4(%r3)
	blr


	ENTRY(cal4)
	lwz	%r3,0(%r1)
	lwz	%r3,0(%r3)
	lwz	%r3,0(%r3)
	lwz	%r3,0(%r3)
	lwz	%r3,0(%r3)
	lwz	%r3,0(%r3)
	lwz	%r3,0(%r3)
	cmpi	%r3,0
	beq	.ret0
	lwz	%r3,4(%r3)
	blr


	ENTRY(cal5)
	lwz	%r3,0(%r1)
	lwz	%r3,0(%r3)
	lwz	%r3,0(%r3)
	lwz	%r3,0(%r3)
	lwz	%r3,0(%r3)
	lwz	%r3,0(%r3)
	lwz	%r3,0(%r3)
	lwz	%r3,0(%r3)
	cmpi	%r3,0
	beq	.ret0
	lwz	%r3,4(%r3)
	blr

.ret0:	li	%r3,0
	blr

	ENTRY(sp3)
	lwz	%r3,0(%r3)
	lwz	%r3,0(%r3)
	lwz	%r3,0(%r3)
	lwz	%r3,0(%r3)
	blr

	ENTRY(sprg0)
	.word	0x7c7042a6
	blr

	ENTRY(sprg1)
	.word	0x7c7142a6
	blr

	ENTRY(sprg2)
	.word	0x7c7242a6
	blr
#endif	/* !defined(lint) */

#if defined(lint)

/*ARGSUSED*/
void
outb(int port_address, unsigned char val)
{}

#else	/* lint */

	ENTRY(outb)
	lis	%r5, PCIISA_VBASE >> 16
	!andi.	%r3, %r3, 0xffff	! mask the port number
	eieio				! XXXPPC should this be after the store?
	stbx	%r4, %r3, %r5
	blr
	SET_SIZE(outb)

#endif  /* lint */
#if defined(lint)

/*ARGSUSED*/
char
inb(int port_address)
{ return ((char)(0)); }

#else	/* lint */
	ENTRY(inb)
	lis	%r5, PCIISA_VBASE >> 16
	!andi.	%r3, %r3, 0xffff	! mask the port number
	eieio				! XXXPPC should this be after the load?
	lbzx	%r3, %r3, %r5
	blr
	SET_SIZE(inb)

#endif  /* lint */

#ifdef lint
/*
 *      make the memory at {addr, size} valid for instruction execution.
 *
 *	NOTE: it is assumed that cache blocks are no smaller than 32 bytes.
 */
/*ARGSUSED*/
void sync_instruction_memory(caddr_t addr, int len)
{}
#else
	ENTRY(sync_instruction_memory)
	li	%r5,32  	! MINIMUM cache block size in bytes
	li	%r8,5		! corresponding shift

	subi	%r7,%r5,1	! cache block size in bytes - 1
	and	%r6,%r3,%r7	! align "addr" to beginning of cache block
	subf	%r3,%r6,%r3	! ... so that termination check is trivial
	add	%r4,%r4,%r6	! increase "len" to reach the end because
				! ... we're startign %r6 bytes before addr
	add	%r4,%r4,%r7	! round "len" up to cache block boundary
!!!	andc	%r4,%r4,%r7	! mask off low bit (not necessary because
				! the following shift throws them away)
	srw	%r4,%r4,%r8	! turn "len" into a loop count
	mtctr	%r4
1:
	dcbst	%r0,%r3 	! force to memory
	sync			! guarantee dcbst is done before icbi
	icbi	%r0,%r3 	! force out of instruction cache
	add	%r3,%r3,%r5
	bdnz	1b

	sync			! one sync for the last icbi
	isync
	blr
	SET_SIZE(sync_instruction_memory)
#endif
