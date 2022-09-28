/*      Copyright (c) 1990, 1991 UNIX System Laboratories, Inc. */
/*      Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T   */
/*        All Rights Reserved   */

/*      THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF          */
/*      UNIX System Laboratories, Inc.                          */
/*      The copyright notice above does not evidence any        */
/*      actual or intended publication of such source code.     */

/*      Copyright (c) 1987, 1988 Microsoft Corporation  */
/*        All Rights Reserved   */

/*      This Module contains Proprietary Information of Microsoft  */
/*      Corporation and should be treated as Confidential.         */

/*	#ident  "@(#)kern-os:fp.c       1.3.1.2" from SVR4/MP	*/

/*
 *	Copyright (c) 1995 by Sun Microsystems, Inc.
 */

#ident "@(#)float.s	1.15	96/10/22 SMI"

#include <sys/asm_linkage.h>
#include <sys/regset.h>

#if defined(lint) || defined(__lint)
#include <sys/types.h>
#include <sys/fp.h>
#else	/* lint */
#include "assym.s"
#endif	/* lint */

#if defined(lint) || defined(__lint)

int fpu_exists = 1;
int use_pentium_fpu_fdivbug = 1;
int fp_kind = FP_387;
int fpu_ignored = 0;
int fpu_pentium_fdivbug = 0;

#else	/* lint */

        .data
/
/ If fpu_exists is non-zero, fpu_probe will attempt to use any
/ hardware FPU (subject to other constraints, see below).  If
/ fpu_exists is zero, fpu_probe will report that there is no
/ FPU even if there is one.
/
        .globl fpu_exists
fpu_exists:
        .long   1
	.size	fpu_exists,4

/
/ If use_pentium_fpu_fdivbug is non-zero, fpu_probe will allow
/ use of a Pentium with the FDIV bug.  Otherwise it will behave
/ as if fpu_exists was zero if it finds a bad Pentium.
/
	.globl use_pentium_fpu_fdivbug
use_pentium_fpu_fdivbug:
	.long	1
	.size	use_pentium_fpu_fdivbug,4

        .globl fp_kind			/ FP_NO, FP_287, FP_387, etc.
fp_kind:
        .long   FP_387
	.size	fp_kind,4

/
/ The variable fpu_ignored is provided to allow other code to determine
/ whether emulation is being done because there is no FPU or because of
/ an override requested via /etc/system.
/
	.globl fpu_ignored
fpu_ignored:
	.long	0
	.size	fpu_ignored,4

/
/ The variable fpu_pentium_fdivbug is provided to allow other code to
/ determine whether the system contains a Pentium with the FDIV problem.
/
	.globl fpu_pentium_fdivbug
fpu_pentium_fdivbug:
	.long	0
	.size	fpu_pentium_fdivbug,4

/
/ The following constants are used for detecting the Pentium divide bug.
/
        .align  4
num1:	.4byte	0xbce4217d	/ 4.999999
	.4byte	0x4013ffff
num2:	.4byte	0x0		/ 15.0
	.4byte	0x402e0000
num3:	.4byte	0xde7210bf	/ 14.999999
	.4byte	0x402dffff

        .text
        .align  4

#endif	/* lint */

/*
 * FPU probe - check if we have any FP chip present by trying to do a reset.
 * If that succeeds, differentiate via cr0. Called from autoconf.
 * XXX - Any changes for 486/487? Weitek support?
 */

#if defined(lint) || defined(__lint)
 
/* ARGSUSED */
void
fpu_probe(void)
{}
 
#else	/* lint */
	ENTRY_NP(fpu_probe)
	clts				/ clear task switched bit in CR0
	fninit				/ initialize chip
	fnstsw	%ax			/ get status
	orb	%al,%al			/ status zero? 0 = chip present
	jnz     no_fpu_hw		/ no, use emulator

/
/ If there is an FP, look for the Pentium FDIV problem even if we
/ do not plan to use it.  Set fpu_pentium_fdivbug is a bad FPU is
/ detected.  Subsequent code can report the result if desired.
/
/ If (num1/num2 > num1/num3) the FPU has the FDIV bug.
/
	fldl	num1
	fldl	num2
	fdivr	%st(1),%st
	fxch	%st(1)
	fdivl	num3
	fcompp
	fstsw	%ax
	sahf
	jae	no_bug
	movl	$1, fpu_pentium_fdivbug
no_bug:
/
/ Repeat the earlier initialization sequence so that the FPU is left in
/ the expected state.
/
	fninit
	fnstsw	%ax
/
/ Ignore the FPU if fpu_exists == 0
/
	cmpl	$0, fpu_exists
	je	ignore_fpu
/
/ Ignore the FPU if it has the Pentium bug and the user did not
/ set use_pentium_fpu_fdivbug non-zero to allow use of a faulty
/ Pentium
/
	cmpl	$0, fpu_pentium_fdivbug
	je	use_it
	cmpl	$0, use_pentium_fpu_fdivbug
	je	ignore_fpu
use_it:
/
/ at this point we know we have a chip of some sort; 
/ use cr0 to differentiate.
/
	movl    %cr0,%edx               / check for 387 present flag
	testl	$CR0_ET,%edx            / ...
	jz      is287                   / z -> 387 not present
/ XXX - Also check if it is 486/487?
	movl    $FP_387,fp_kind         / we have a 387 chip
	jmp     mathchip
/
/ No 387; we must have an 80287.
/
is287:
	fsetpm				/ set the 80287 into protected mode
	movl    $FP_287,fp_kind         / we have a 287 chip
/
/ We have either a 287, 387, 486 or P5.
/ Setup cr0 to reflect the FPU hw type.
/
mathchip:
	andl    $-1![CR0_TS|CR0_EM],%edx	/ clear emulate math chip bit
	movw	cputype, %ax
	andw	$CPU_ARCH, %ax
	cmpw	$I86_386_ARCH, %ax
	je	cpu_386

	orl     $[CR0_MP|CR0_NE],%edx
	jmp	cont

	/
	/ For AT386 support we need an interrupt handler to process FP
	/ exceptions. The interrupt comes at IRQ 13. (See configure())
	/
cpu_386:
	orl     $CR0_MP,%edx
	jmp	cont

/ Do not use the FPU
ignore_fpu:
	movl	$1, fpu_ignored
/ No FP hw present.
no_fpu_hw:
	movl    %cr0,%edx
	andl    $-1!CR0_MP,%edx         / clear math chip present
	orl     $CR0_EM,%edx            / set emulate math bit  - XXX?
	movl    $FP_NO,fp_kind          / signify that there is no FPU
	movl	$0, fpu_exists		/ no FPU present

cont:
	movl    %edx,%cr0               / set machine status word

#if defined(WEITEK_later)
/
/ test for presence of weitek chip
/ we're going to commandeer a page of kernel virtual space to map in 
/ the correct physical addresses.  then we're going to play with what
/ we hope to be weitek addresses.  finally, we'll put things back the
/ way they belong.
/
/ extern unsigned long weitek_paddr;	/* chip physical address */
/
	cmpl	$0, weitek_paddr	/ if (weitek_paddr == 0)
	jz	weitek_skip		/	goto weitek_skip;
	pushl	%ebx
#if MP
	pushl	%ecx
	movl	kspt0,%ecx		/ we cannot do a push kspt0 with cpu
					/ other than cpu0 since esp uses the
					/ same addresses which are mapped with
					/ kspt0
#else	/* MP */
	pushl	kspt0
#endif	/* MP */
	movl	$0xc0000003, kspt0	/ pfn c0000, sup, writeable, present
	movl	%cr3, %eax		/ flush tlb
	movl	%eax, %cr3
	movl	$KVSBASE, %ebx		/ base address for weitek area
	movb	$WEITEK_HW, weitek_kind	/ first assume that there is a chip
	movl	$0x3b3b3b3b, 0x404(%ebx) / store a value into weitek register.
	movl	0xc04(%ebx), %eax	/ and read it back out.
	cmpl	$0x3b3b3b3b, %eax
	jnz	noweitek		/ no chip
	/ clear weitek exceptions so that floating point exceptions
	/ are reported correctly from here out
	/ initialize the 1167 timers
	movl    $0xc000c003, kspt0      / pfn c000c, sup, writeable, present
	movl	%cr3, %eax		/ flush tlb
	movl	%eax, %cr3
	movl	$0xB8000000, 0x000(%ebx)
	movl	0x400(%ebx), %eax	/ Check for 20 MHz 1163
	andl	$WEITEK_20MHz, %eax
	jnz	w_init_20MHz
	movl 	$0x16000000, 0x000(%ebx)	/ 16 MHz 1164/1165 flowthrough
						/ timer
	jmp	w_init_wt1

w_init_20MHz:
	movl	$0x56000000, 0x000(%ebx)	/ 20 MHz 1164/1165 flowthrough
	movl	$0x98000000, 0x000(%ebx)	/ timer
	
w_init_wt1:
	movl 	$0x64000000, 0x000(%ebx)	/ 1164 accumulate timer
	movl 	$0xA0000000, 0x000(%ebx)	/ 1165 accumulate timer
	movl 	$0x30000000, 0x000(%ebx)	/ Reserved mode bits (set to 0).
	movl 	weitek_cfg, %eax	/ Rounding modes and Exception
	movl 	%eax, 0x000(%ebx)	/ enables.
	movw	$0xF0, %dx		/ clear the fp error flip-flop
	movb	$0, %al
	outb	(%dx)
	/
	jmp	weitek_done
noweitek:
	movb	$WEITEK_NO, weitek_kind		/ no. no weitek

weitek_done:
#if MP
	movl	%ecx,kspt0
	movl	%cr3,%eax
	movl	%eax,%cr3
	popl	%ecx
#else	/* MP */
	popl	kspt0			/ get the old kpt0[0] back
	movl	%cr3, %eax		/ flush tlb
	movl	%eax, %cr3
#endif	/* MP */
	popl	%ebx
weitek_skip:

#endif	/* WEITEK_later */
	ret
	SET_SIZE(fpu_probe)

#endif	/* lint */

/*
 * fpsave(fp)
 *      struct fpu *fp;
 * Store the floating point state and disable the floating point unit.
 * XXX - WEITEK support needed.
 * XXX - any changes for 486/487?
 *
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
fpsave(struct fpu *fp)
{}

#else	/* lint */

        ENTRY_NP(fpsave)
        clts				/ clear TS bit in CR0
        movl    4(%esp), %eax		/ load save address
        fnsave  (%eax)			/ save state
        fwait				/ wait for completeion
	movl	%cr0, %eax
	orl	$CR0_TS, %eax
	movl	%eax, %cr0		/ set TS bit in CR0 (disable FPU)
	ret
        SET_SIZE(fpsave)

#endif	/* lint */

/*
 * fpksave(fp)
 *      struct fpu *fp;
 *
 * This is like the above routine but leaves the floating point
 * unit enabled, used during fork of processes that use floating point.
 * XXX - WEITEK support needed.
 * XXX - any changes for 486/487?
 *
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
fpksave(struct fpu *fp)
{}

#else	/* lint */

        ENTRY_NP(fpksave)
        clts				/ clear TS bit in CR0
        movl    4(%esp), %eax		/ load save address
        fnsave  (%eax)			/ save state
        fwait				/ wait for completeion
	ret
        SET_SIZE(fpksave)
 
#endif	/* lint */

/*
 * fprestore(fp)
 *      struct fpu *fp;
 * XXX - WEITEK support needed.
 * XXX - any changes for 486/487?
 *
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
fprestore(struct fpu *fp)
{}

#else	/* lint */

        ENTRY_NP(fprestore)
	clts				/ clear TS bit in CR0
	movl	4(%esp), %eax		/ load restore address
	frstor	(%eax)			/ restore state
	ret
        SET_SIZE(fprestore)
 
#endif	/* lint */

/*
 * fpenable()
 *
 * Enable the floating point unit.
 */
 
#if defined(lint) || defined(__lint)
 
/* ARGSUSED */
void
fpenable(void)
{}
 
#else	/* lint */
 
        ENTRY_NP(fpenable)
	clts				/ clear TS bit in CR0
	ret
        SET_SIZE(fpenable)
 
#endif	/* lint */

/*
 * fpdisable()
 *
 * Disable the floating point unit.
 * XXX - WEITEK support needed.
 * XXX - any changes for 486/487?
 */
 
#if defined(lint) || defined(__lint)
 
/* ARGSUSED */
void
fpdisable(void)
{}
 
#else	/* lint */
 
        ENTRY_NP(fpdisable)
	movl	%cr0, %eax
	orl	$CR0_TS, %eax
	movl	%eax, %cr0		/ set TS bit in CR0 (disable FPU)
	ret
        SET_SIZE(fpdisable)
 
#endif	/* lint */

/*
 * fpinit(void)
 *
 * Initialize the fpu hardware.
 * XXX - WEITEK support needed.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
fpinit(void)
{}

#else	/* lint */

        ENTRY_NP(fpinit)
	clts				/ clear TS bit in CR0
	fninit				/ initialize the chip
	pushl	$FPU_CW_INIT		/ inital value of FPU control word
	fldcw	(%esp)			/ load the control word
	popl	%eax
	fwait
	ret
        SET_SIZE(fpinit)
 
#endif	/* lint */

/*
 * fperr_reset()
 *	clear FPU exeception state.
 * Returns the FP status word.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
fperr_reset(void)
{
	return(0);
}

#else	/* lint */

        ENTRY_NP(fperr_reset)
	clts				/ clear TS bit in CR0
	fnstsw	%ax			/ get status
	fnclex				/ clear processor exceptions
	ret
        SET_SIZE(fperr_reset)
 
#endif	/* lint */

/*
 * fpintr_reset()
 *
 *	Reset NDP busy state.
 *
 * (The NDP's (287's) busy line is only held active while actually
 * executing, but hardware that sits between the 2 chips latches
 * the NDP error line and feeds it to the 286/386 busy line.  Since this
 * prevents normal coprocessor communication, we must clear the NDP
 * BUSY latch before attempting to examine the NDP status word.
 * Else the 286 thinks the NDP is busy and hangs on any NDP access.)
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
fpintr_reset(void)
{}

#else	/* lint */

        ENTRY_NP(fpintr_reset)
	movw    $0x0f0,%dx              / outb(0xf0, 0)
	subb    %al,%al
	outb    (%dx)
	ret
        SET_SIZE(fpintr_reset)
 
#endif	/* lint */

/*
 * fp_null()
 *
 * Stub function that simply returns.  Used by fp_fork and fpnoextflt
 * when calling installctx in place of save and restore functions.
 * Written in assembly to optimize out unnecessary instructions added
 * by the compiler.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
fp_null(int arg)
{}

#else	/* lint */

	ENTRY_NP(fp_null)
	ret
	SET_SIZE(fp_null)

#endif	/* lint */
