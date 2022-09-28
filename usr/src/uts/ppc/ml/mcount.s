/*
 * Copyright (c) 1986-1993, 1994 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)mcount.s 1.16	96/05/29 SMI"

#include <sys/asm_linkage.h>
#include <sys/gprof.h>
#include <sys/reg.h>

#if defined(lint)
void
_mcount()
{}
#else
#include "assym.s"
#if defined(GPROF)
#include <sys/machthread.h>

	ENTRY_NP(_mcount)

	/*
	 * Gonna monkey with some registers. Gotta save them because
	 * the compiler doesn't for -xpg compiled objects. 
	 *
	 * registers used include
	 * r3, r4 at first 
	 * r5 to figure out if profiling is on
	 * r6, r7, r8, r10, r11 once we've figured out that profiling is
	 * on.
	 */
	stwu	%r1, -SA(40)(%r1)	! grow stack by at least 8 words
	stw	%r3, 8(%r1)		! just save these two for
	stw	%r4, 12(%r1)		! starters

	/*
	 * get kern_profiling struct ptr
	 */

	lwz 	%r3, T_CPU(THREAD_REG)	! get cpu struct ptr
	cmpwi	%r3, 0			! cpu struct is null out of mlsetup only
	beq-	.mc_out
	lwz 	%r4, CPU_PROFILING(%r3)	! cpu_profiling to r4
	cmpwi 	%r4, 0			! is cpu_profiling == 0?
	beq+ 	.mc_out

	/*
	 * Find out if profiling is turned on
	 */

	stw	%r5, 16(%r1)		! save r5 now
	lwz 	%r5, PROFILING(%r4)
	cmpwi 	%r5, PROFILE_ON		! is p->profiling == 3?
	bne+ 	.mc_out2

	/*
	 * Save the rest of the registers.
	 */

	stw	%r6, 20(%r1)		! now r6 (we're getting deeper)
	stw	%r7, 24(%r1)		! now the rest
	stw	%r8, 28(%r1)		! we're committed
	stw	%r10, 32(%r1)
	stw	%r11, 36(%r1)

	/*
	 * Get the lock
	 */

	la	%r6, PROF_LOCK(%r4)
.again:
	lwarx 	%r5, 0, %r6
	cmpwi 	%r5, 0
	bne-	.mc_out3 		! somebody else has the lock
	stwcx.  THREAD_REG,0,%r6 	! become the owner
	bne- 	.again			! lost reservation, try again

	isync

	/*
	 * Hash PARENTPC to some entry in froms. 
	 * Note, low order two bits of PARENTPC will
	 * be 0, and fromssize is some power of 2 times sizeof (kp_call_t *).
	 * link_addr = &froms[(frompc >> 2) & ((fromssize/sizeof(kp_call_t *)) - 1)]
	 */

	mflr	%r10			! who called _mcount
	lwz	%r11, 0(%r1)		! follow back link of sp
	lwz	%r11, 4(%r11)		! who called the function that called _mcount

        lwz     %r5, PROF_FROMSSIZE(%r4)	! scratch1
        lwz     %r6, PROF_FROMS(%r4)		! scratch0
        addi	%r5, %r5, 0xffffffff   		! subtract 1
        and     %r5, %r11, %r5			! mask grandparentpc
        add     %r7, %r6, %r5 			! offset into froms LINK_ADDR
						! link_addr is r7
.testlink:
        lwz     %r3, 0(%r7)            		! top = *link_addr reuse r3
        cmpwi  	%r3, 0				! test TOP
        bne+   	.oldarc

	! This is the first time we've seen a call from here; get new kp_call struct

	lwz	%r3, PROF_TOSNEXT(%r4)		! tosnext
	lwz	%r8, PROF_TOS(%r4)		! tos
	lwz	%r6, PROF_TOSSIZE(%r4)		! tossize
	addi	%r5, %r3, KPCSIZE		! nextptr += sizeof(kp_struct_t)
	stw	%r5, PROF_TOSNEXT(%r4)		! tosnext = nextptr
	add	%r6, %r6, %r8			! 
	cmpl	%r3, %r6			! are we too far?
	bge-	.done				! should be overflow

	! Initialize arc

	stw	%r3, 0(%r7)			! old tosnext ptr
	stw	%r11, KPC_FROM(%r3)		! store 2 functions back
	stw	%r10, KPC_TO(%r3)		! save lr 
	li	%r6, 1				! move 1 to scratch0
	xor	%r10, %r10, %r10		! zero a register
	stw	%r6, KPC_COUNT(%r3)		! save count
	stw	%r10, KPC_LINK(%r3)		! zero the link
	b	.done

	! Old arc this branch usually taken

.oldarc:
	lwz	%r5, KPC_TO(%r3)		
	cmp	%r5, %r10
	bne+	.chainloop
	lwz	%r6, KPC_FROM(%r3)	
	cmp	%r6, %r11
	beq+	.storeNdone
	
.chainloop:
	addi	%r7, %r3, KPC_LINK
	b	.testlink

.storeNdone:
	lwz	%r5, KPC_COUNT(%r3)
	addi	%r5, %r5, 1
	stw	%r5, KPC_COUNT(%r3)

.done:
	lwz	%r11, 36(%r1)
	lwz	%r10, 32(%r1)
	lwz	%r8, 28(%r1)
	lwz	%r7, 24(%r1)

        li      %r5, 0		        ! value to store
	la	%r3, PROF_LOCK(%r4)	! load lock
	stw	%r5,	0(%r3)		! clear lock
	
.mc_out3:
	lwz	%r6, 20(%r1)
.mc_out2:
	lwz	%r5, 16(%r1)
.mc_out:
	lwz	%r4, 12(%r1)
	lwz	%r3, 8(%r1)
	addi	%r1, %r1, SA(40)	! reduce stack
	blr
	SET_SIZE(_mcount)
#else /* GPROF */
	ENTRY_NP(_mcount)
	blr
	SET_SIZE(_mcount)
#endif /* GPROF */

#if defined(GPROF)
        .data					! This is actually useless
        .global kernel_profiling		! its not referenced anywhere
kernel_profiling:				! I suppose you can check via
        .word   1				! adb or something
 
#endif /* defined(GPROF) */
#endif /* lint */

#if defined(lint)

/*
 * return pc value from prior to interrupt.
 * Called out of profile driver.
 */
greg_t
_get_pc(void)
{ return ((greg_t)0); }
#else
	ENTRY(_get_pc)

	mfsrr0	%r3
	blr
	SET_SIZE(_get_pc)
#endif
