/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)locore.s	1.85	96/07/28 SMI"

#include <sys/asm_linkage.h>
#include <sys/stack.h>
#include <sys/machthread.h>
#include <sys/psw.h>
#include <sys/reg.h>
#include <sys/trap.h>
#include <sys/traptrace.h>
#include <sys/mmu.h>
#include <sys/clock.h>
#include <sys/isa_defs.h>

#if defined(lint) || defined(__lint)
#include <sys/types.h>
#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/time.h>
#else /* lint */
#include "assym.s"
#endif /* lint */

/*
 * Special Purpose Register number definitions.
 */
#define	DSISR	18
#define	DAR	19
#define	SRR0	26
#define	SRR1	27
#define	DMISS	976
#define	DCMP	977
#define	HASH1	978
#define	HASH2	979
#define	IMISS	980
#define	ICMP	981
#define	RPA	982
#define	SPRG0	272
#define	SPRG1	273
#define	SPRG2	274
#define	SPRG3	275
#define	BAT0U	528	/* 601-BAT0U or IBAT0U */
#define	BAT0L	529	/* 601-BAT0L or IBAT0L */
#define	BAT1U	530	/* 601-BAT1U or IBAT1U */
#define	BAT1L	531	/* 601-BAT1L or IBAT1L */
#define	BAT2U	532	/* 601-BAT2U or IBAT2U */
#define	BAT2L	533	/* 601-BAT2L or IBAT2L */
#define	BAT3U	534	/* 601-BAT3U or IBAT3U */
#define	BAT3L	535	/* 601-BAT3L or IBAT3L */
#define	IBAT0U	528
#define	IBAT0L	529
#define	IBAT1U	530
#define	IBAT1L	531
#define	IBAT2U	532
#define	IBAT2L	533
#define	IBAT3U	534
#define	IBAT3L	535
#define	DBAT0U	536
#define	DBAT0L	537
#define	DBAT1U	538
#define	DBAT1L	539
#define	DBAT2U	540
#define	DBAT2L	541
#define	DBAT3U	542
#define	DBAT3L	543
#define	HID0	1008

/*
 * the base of the vector table could be 0x0 or 0xfff00000
 */
#define VECBASE	0x0

/*
 * physical base addresses of additional level0 handlers or sections of
 * them which are copied into low memory.
 */
#define	FASTTRAP_PADDR	0x2100
#define	TRAP_RTN_PADDR	0x2300
#define	RESET_PADDR	0x2500
#define	SYSCALL_RTN_PADDR 0x2700
#define HDLR_END	0x2900-4	/* last word used by trap handlers */

/*
 * BAT values for Level 0 handlers to map low memory.
 */
#define	BATU_LOMEM_MAPPING	0x14	/* for 601 BAT Upper */
#define	BATL_LOMEM_MAPPING	0x7f	/* for 601 BAT Lower */
#define	IBATU_LOMEM_MAPPING	0xfe	/* for PowerPC IBAT Upper */
#define	IBATL_LOMEM_MAPPING	0x12	/* for PowerPC IBAT Lower */

/*
 * Byte offsets to access byte or word within PTE structure.
 * PTE structure: (BIG ENDIAN format used as reference)
 *		0-------1-------2-------3-------
 *		v.........vsid...........h...api
 *		4-------5-------6-------7-------
 *		ppn.............000.R.C.WIMG.0.PP
 */
#ifdef _BIG_ENDIAN
#define	PTEBYTE6	6
#define	PTEWORD0	0
#define	PTEWORD1	4
#else /* _LITTLE_ENDIAN */
#define	PTEBYTE6	1
#define	PTEWORD0	4
#define	PTEWORD1	0
#endif

/*
 *
 *	At this time, the PowerPC 601, 603, and 604 are supported.
 *	The primary issues that need to be addressed are:
 *
 *		For non-601, the time base frequency needs to be initialized
 *		based on platform specific information, presumably made
 *		available by Open Firmware.
 *
 *		Single threaded interrupt handling should be done when a
 *		PANIC is in progress.  This may be doable outside of this
 *		code by making all interrupts run at the same level, but
 *		this may require checking here.
 *
 *		The code the deals with "unsafe" drivers should be removed
 *		when unsafe drivers are no longer supported.
 *
 *		It appears that the code below the label "bump_dec" is not
 *		correct, shown by the instructions that have been commented
 *		out.  This needs more careful study when MP support is added.
 *		There is more code like this below "clock_done".
 *
 *		The code dealing with "release interrupts" should be removed.
 *
 *		The level 1 "sys_call" code should be rewritten to perform
 *		better.  The current algorithm is from x86, which is very
 *		inappropriate.
 *
 *	Other review comments:
 *
 *		In general, there should more use of Symbolic constants
 *		instead of numeric ones.  One example of this is the
 *		kernel VSID (Virtual Segment ID) group number, which is 1.
 *		Another is the "pvr" register values for identifying which
 *		of the PowerPC processors we're running on, 1 for 601, etc.
 *		sprg register usage should be symbolic.
 *
 *		The clock code could benefit from optimistic masking
 *		techniques ... and may need them.  If a clock interrupt is
 *		taken while a dispatcher lock is held, it could deadlock
 *		trying to get the dispatcher lock.  So, dispatcher locks must
 *		disable the clock.  Doing this by setting a mask in a register
 *		(an sprg?) or just cpu->cpu_pri would be cheap.  If the clock
 *		interrupt occurred while the mask is set, then a pending
 *		indication is set somewhere (in the sprg or cpu struct) and
 *		the handler returns with the interrupt disabled.  This could
 *		be done in the level 0 for the clock.   When the interrupt is
 *		enabled again (by splx or disp_lock_exit, etc), the mask is
 *		cleared and the pending indication is checked.  If the
 *		indication is set, then the interrupt handler is invoked.
 *
 *		Since clocks can't be blocked, maybe the level 1 handler
 *		should just not fire off the clock thread if clock level
 *		interrupts should be blocked by cpu->cpu_pri, but still do
 *		everything else (increment time, etc)., and when it is
 *		unmasked, the code can invoke the clock thread.
 *
 *		Sherif has done optimistic masking on x86 (and I've been
 *		meaning to review his work), and this approach could work
 *		for PPC on all interrupts, and could greatly speed spl code.
 *
 *		Of course, this can all be done later, except I'm wondering
 *		if disp_lock code blocks the clock interrupt somehow.  Isn't
 *		this a problem now?
 */

/*
 *
 *	This file contains kernel ml startup code, interrupt code,
 *	system call and trap ml code, and hrt primitives for PowerPC.
 *
 *	This section defines some standard Solaris symbols and the
 *	stack and thread structure for thread 0.
 */

#if !defined(lint) && !defined(__lint)
	.data
	.align	2

	.globl	t0stack
	.type	t0stack,@object
	.align	4		! stacks are 16-byte (2**4 bytes) aligned
t0stack:.skip	T0STKSZ		! thread 0's stack should be the 1st thing
				! in the data section

	.align	5		! thread structures are 32-byte aligned
	.globl	t0
t0:	.skip	THREAD_SIZE

	.globl	Kernelbase;
	.type	Kernelbase,@object;
	.size	Kernelbase,0;
	.set	Kernelbase, KERNELBASE

	.globl	Sysbase;
	.type	Sysbase,@object;
	.size	Sysbase,0;
	.set	Sysbase, SYSBASE

	.globl	Syslimit;
	.type	Syslimit,@object;
	.size	Syslimit,0;
	.set	Syslimit, SYSLIMIT

	.global	msgbuf
	.type	msgbuf, @object;
	.size	msgbuf, MSGBUFSIZE;
	.set	msgbuf, V_MSGBUF_ADDR	! address of printf message buffer

#ifdef TRAPTRACE
	/*
	 * Trap trace area.
	 */
	.align	4			! 16-byte alignment
trap_trace_ctl:
        .word   trap_tr0                ! next  CPU 0
        .word   trap_tr0                ! first
        .word   trap_tr0 + TRAP_TSIZE   ! limit
        .word   0                       ! junk for register save
trap_tr0:
        .skip   TRAP_TSIZE

	.global	trap_trace_ctlp		! needed by debug code in startup
trap_trace_ctlp:			! so another area can be allocated
	.word	trap_trace_ctl
#endif /* TRAPTRACE */

/*
 *	Terminology used to describe levels of exception handling:
 *
 *	    Level 0 - ml code running with instruction relocation off
 *	    Level 1 - ml code running with instruction relocation on
 *	    Level 2 - C code
 *
 *	The number 1 priority use for the 4 sprg registers is to make
 *	the transition out of level 0 as efficient as possible.  The
 *	current use of these registers is as follows:
 *
 *	    sprg0 - curthread (virtual)
 *	    sprg1 - curthread->t_stack (virtual)
 *	    sprg2 - CPU (physical)
 *	    sprg3 - scratch
 *
 *	The transition from level 0 to level 1 code is done with a
 *	a blr by putting the addr of the appropriate level-1 handler
 *	in the LR.
 *
 *	Level 0 handlers are copied into physical addresses 0x100, 0x200,
 *	... during kernel initialization.
 *
 *	There is the following static data that is needed to make an
 *	efficient transition from level 0 code to level 1 code.  It is
 *	copied from the statically defined data into the CPU structure
 *	during initialization.  This is described just below the next
 *	section which defines the first stuff in the data section.
 *
 *
 *	structure member   Entry points and MSRs
 *	-------------------------------------------------------------
 *	CPU_CMNTRAP      - &cmntrap (always handled by the kernel)
 *	CPU_KADB         - &kadb_trap (traps that are handled by
 *			      kadb, this is modified in the kernel's
 *			      _start when kadb is running)
 *	CPU_SYSCALL      - &sys_call
 *	CPU_CMNINT       - &cmnint
 *	CPU_CLOCKINTR    - &clock_intr
 *
 *	CPU_MSR_ENABLED  - MSR with interrupts enabled
 *	CPU_MSR_DISABLED - MSR with interrupts disabled
 *	CPU_MSR_KADB     - MSR for kadb events (if kadb is running,
 *			   this is the same as CPU_MSR_DISABLED,
 *			   otherwise it's CPU_MSR_ENABLED)
 */
level0_2_level1:
	.word	cmntrap
	.word	trap_kadb
	.word	sys_call
	.word	cmnint
	.word	clock_intr

	.word	MSR_EE|MSR_ME|MSR_IR|MSR_DR|MSR_FP	! level 1 bits needed
	.word	       MSR_ME|MSR_IR|MSR_DR|MSR_FP	! level 1 bits needed

#endif /* lint */

/*
 *	_start() - Kernel initialization
 *
 *	Our assumptions:
 *		- We are running with virtual addressing.
 *		- Interrupts are disabled.
 *		- The kernel's text, initialized data and bss are mapped.
 *		- We can easily tell whether or not KADB is around.
 *
 *	Our actions:
 *		- Save arguments, and check for kadb
 *			r3 - &cif_handler of secondary boot or kadb
 *			r4 - if kadb, entry point for level 1 handler
 *			     if not kadb, this is 0
 *			r5 - &bopp
 *			r6 - &bootaux
 *		- Initialize our stack pointer to the thread 0 stack (t0stack)
 *		  and leave room for a phony "struct regs".
 *		- When KADB is around, CPU_KADB needs to be initialized
 *		  from the argument in r6.
 *		  (NOTE: we choose the vectors we want KADB to handle)
 *		- Check for cpu type, i.e. 601, 603, 604, or 620.
 *		- mlsetup(sp, cif_handler) gets called.
 *		- We change our appearance to look like the real thread 0.
 *		  Initialize sprg0, sprg1, and sprg2
 *		- Setup Level 0 interrupt handlers
 *		- main() gets called.  (NOTE: main() never returns).
 *
 */

/*
 *	NOW, the real code!
 */

#if defined(lint)

void
_start(void)
{}

#else   /* lint */

!	_start() - Kernel Initialization

	.text
	ENTRY_NP(_start)
	bl	_start1		! get the following addresses
	.word	edata		! define edata (referenced for /dev/ksyms)
	.word	0		! no longer used
	.word	t0stack		! r16
	.word	t0		! r2 (t0 is curthread!)
	.word	level0_2_level1	! r18
	.word	cpus		! r20 - CPU
_start1:
	mflr	%r14		! points back to array of pointers
	lwz	%r15,4(%r14)
	lwz	%r16,8(%r14)
	lwz	THREAD_REG,12(%r14)
	lwz	%r18,16(%r14)
	lwz	%r20,20(%r14)

!		- Save arguments, and check for kadb
!			r3 - &cif_handler
!			r4 - if kadb, entry point for level 1 handler,
!			     otherwise this is 0
!			r5 - &bopp
	.comm	cif_handler,4,2
	.comm	cifp,4,2
	.comm	bootopsp,4,2
	.comm	_kadb_entry,4,2

	lis	%r11,cifp@ha	! save pointer to caller's cif_handler
	stw	%r3,cifp@l(%r11)
	lwz	%r31,0(%r3)	! save value of cif_handler in r31

	lis	%r11,bootopsp@ha
	stw	%r5,bootopsp@l(%r11)
	lwz	%r0,0(%r5)
	lis	%r11,bootops@ha
	stw	%r0,bootops@l(%r11)

!		- Initialize our stack pointer to the thread 0 stack (t0stack)
!		  and leave room for a phony "struct regs".
	addi	%r1,%r16,SA(T0STKSZ-REGSIZE-MINFRAME)

!		- When KADB is around, _kadb_entry needs to be initialized
!		  from the argument in r4.

	cmpi	%r4,0		! kadb running?
	lwz	%r0,0(%r18)		! "cmntrap"
	lwz	%r3,20(%r18)		! level 1 msr (interrupts enabled)
	beq+	.no_kadb
	lwz	%r0,4(%r18)		! "trap_kadb"
	lis	%r7,_kadb_entry@ha
	lwz	%r3,24(%r18)		! level 1 msr (interrupts disabled)
	stw	%r4,_kadb_entry@l(%r7)	! install kadb's handler
	lis	%r7,dvec@ha
	stw	%r4,dvec@l(%r7)		! set dvec to a valid address
.no_kadb:
	mfmsr	%r4
	stw	%r0,CPU_KADB(%r20)
	or	%r3,%r3,%r4	! current msr "or" our memory value
	stw	%r3,CPU_MSR_KADB(%r20)

	lwz	%r3,20(%r18)
	or	%r3,%r3,%r4	! current msr "or" interrupts enabled
	stw	%r3,CPU_MSR_ENABLED(%r20)
	lwz	%r3,24(%r18)
	or	%r3,%r3,%r4	! current msr "or" interrupts disabled
	stw	%r3,CPU_MSR_DISABLED(%r20)
	lwz	%r3,0(%r18)	! "cmntrap"
	stw	%r3,CPU_CMNTRAP(%r20)
	lwz	%r3,8(%r18)	! "sys_call"
	stw	%r3,CPU_SYSCALL(%r20)
	lwz	%r3,12(%r18)	! "cmnint"
	stw	%r3,CPU_CMNINT(%r20)
	lwz	%r3,16(%r18)	! "clock_intr"
	stw	%r3,CPU_CLOCKINTR(%r20)

!		- Check for cpu type, i.e. 601, 603, 604, or 620.
	mfpvr	%r0
	sri	%r3,%r0,16	! Version (1,3,4,6,20)
	cmpi	%r3,1
	bne-	.not_601
	!
	! Found 601. Apply patches to BAT values
	!
	lis	%r3, _bat601_patch_list@ha
	la	%r3, _bat601_patch_list@l(%r3)
	bl	_patch_code
	li	%r3,CPU_601
	b	.cpu_identified
.not_601:
	cmpi	%r3,3
	beq-	.is603
	cmpi	%r3,6
	beq-	.is603
	cmpi	%r3,7		! 603ev ?
	bne-	.not_603
.is603:
	li	%r3,CPU_603
	b	.cpu_identified
.not_603:
	cmpi	%r3,4
	beq-	.is604
	cmpi	%r3,9		! 604e ?
	bne-	.not_604
.is604:
	li	%r3,CPU_604
	b	.cpu_identified
.not_604:
	cmpi	%r3,20
	bne-	.not_620
	li	%r3,CPU_620
	b	.cpu_identified
.not_620:
	!
	! PANIC: Unknown CPU type!
	!
	lis	%r3, pgmname@ha
	la	%r3, pgmname@l(%r3)
	mr	%r4, %r31
	bl	prom_init	! need to do this before using promif library
	lis	%r3, badcpu@ha
	la	%r3, badcpu@l(%r3)
	mfpvr	%r0
	sri	%r4,%r0,16
	bl	prom_printf	! print PANIC message
1:
	b	1b		! loop for ever...
	.data
badcpu:
	.string "\n\nPANIC: Unsupported CPU type with PVR value %d.\n\nHalted."
pgmname:
	.string "kernel"
	.align	4
	.text
.cpu_identified:
	lis	%r11,cputype@ha
	sth	%r3,cputype@l(%r11)

!		- mlsetup(sp, cif_handler) gets called.
	mr	%r3,%r1		! pass in the current stack pointer.
				! NOTE that it points to Thread 0's stack.
	mr	%r4,%r31	! cif_handler
	bl	mlsetup

!		- We change our appearance to look like the real thread 0.
!		  We assume that mlsetup has set t0->t_stk and t0->t_cpu.
!		  For PowerPC, this is a good time to Initialize sprg0,
!		  sprg1 and sprg2.
!
!		    sprg0 - curthread (virtual)
!		    sprg1 - curthread->t_stack (virtual) (Really, sprg1
!			    is the kernel stack address when a transition
!			    from user mode to kernel mode occurs)
!		    sprg2 - CPU + CPU_R3 (physical)
!		    sprg3 - scratch

	lwz	%r1,T_STACK(THREAD_REG)
	li	%r0,0

	mtsprg	0,THREAD_REG	! curthread is thread0
	mtsprg	1,%r1
	stw	%r0,0(%r1)	! terminate t0 stack trace
	stw	%r0,4(%r1)
	bl	check_OF_revision
	bl	ppcmmu_param_init
	lwz	%r3,T_CPU(THREAD_REG)
			! adjust it so kadb can do store at offset 0.
	addi	%r3,%r3,CPU_R3
	bl	va_to_pa
	mtsprg	2,%r3


!	- main() gets called.  (NOTE: main() never returns).
	bl	main
	.word	0		! a guaranteed illegal instruction!?
	/* NOTREACHED */

	SET_SIZE(_start)

! Set the segment registers and SDR1 value for the kernel.  The PROM
! doesn't necessarily have a page table that is compatible with our
! usage of VSIDs.  This routine writes segment registers to work with
! the kernel's page table and sets the SDR1 value.  The routine uses
! a BAT to map in this code so that SDR1 and the segment registers
! can be adjusted while still being mapped via a BAT register.  The
! BAT is restored and the new page table is in effect.
!
! The routine copies a chunk of code to physical address 0, switches
! to real mode and jumps to this address.  While in real mode,
! new value is placed in the SDR1 register, and the system returns to
! virtual mode.
!
! Args:
!	%r3 - new SDR1 value

	ENTRY_NP(kern_segregs_sdr1)
	MINSTACK_SETUP
	mfpvr	%r4
	sri	%r4,%r4,16
	cmpi	%r4,1
	bne	1f
	! 601
	mfspr	%r8, BAT2U		! Save the BAT
	mfspr	%r9, BAT2L		! Save the BAT
	li	%r5, BATU_LOMEM_MAPPING
	li	%r6, BATL_LOMEM_MAPPING
	isync
	mtspr	BAT2U, %r5		! set BAT2 to map low memory
	mtspr	BAT2L, %r6
	b	2f
1:
	mfspr	%r8,IBAT2U		! Save the IBAT
	mfspr	%r9,IBAT2L		! Save the IBAT
	mfspr	%r10,DBAT2U		! Save the DBAT
	mfspr	%r11,DBAT2L		! Save the DBAT
	li	%r5, IBATU_LOMEM_MAPPING
	li	%r6, IBATL_LOMEM_MAPPING
	isync
	mtspr	IBAT2U, %r5		! set IBAT2 to map low memory
	mtspr	IBAT2L, %r6
	mtspr	DBAT2U, %r5		! set DBAT2 to map low memory
	mtspr	DBAT2L, %r6
2:
	isync

	!
	! In the future we may need to save what is at 0 to be restored later
	! However, at present, there is no need to do so since 0-0x100
	! is unused.
	!

	!
	! copy the routine down to physical address 0
	!
	lis	%r5, change_sdr_seg@ha			! copy source = r5
	la	%r5, change_sdr_seg@l(%r5)
	li	%r6, 0					! copy dest = r6
	lis	%r4, end_change_sdr_seg@ha		! end loop point = r4
	la	%r4, end_change_sdr_seg@l(%r4)
1:
	lwz	%r7,0(%r5)		! get a word
	stw	%r7,0(%r6)		! store a word

	dcbst	0,%r6			! initiate data cache to memory
	sync				! wait till there
	icbi	0,%r6			! invalidate copy in instruction cache
	sync				! wait for icbi to be globally performed
	isync				! remove copy in instruction buffer

	addi	%r5,%r5,4		! source += 4
	addi	%r6,%r6,4		! dest += 4
	cmplw	%r5,%r4			! if source != end
	bne	1b			!	loop again
#define DEBUG_KSS
#ifdef DEBUG_KSS
	mfsr	%r4, 0xd
	mfsr	%r5, 0xe
	mfsr	%r6, 0xf
#endif /* DEBUG_KSS */

	!
	! Jump to the routine we just copied to 0 to do the work
	!
	bla	0

	!
	! restore what was down there
	!

	!
	! restore the BAT mapping
	!
	mfpvr	%r4
	sri	%r4,%r4,16
	cmpi	%r4,1
	bne	1f
	isync
	mtspr	BAT2U, %r8	! restore BAT2
	mtspr	BAT2L, %r9
	b	2f
1:
	isync
	mtspr	IBAT2U, %r8	! restore IBAT2
	mtspr	IBAT2L, %r9
	mtspr	DBAT2U, %r10	! restore DBAT2
	mtspr	DBAT2L, %r11
2:
	isync

	MINSTACK_RESTORE
	blr			! return
	SET_SIZE(kern_segregs_sdr1)

	.data
	.align 2

	! uses %r3-%r7
	! NOTE: r3 contains the new SDR1 value
	!
change_sdr_seg:
	!
	! build initial segment register value
	!
	li	%r4,1			! KERNEL_VSID_RANGE
	slwi	%r4,%r4,4		! move range number to proper place
	ori	%r4,%r4,0xf		! we count down so start with 0xf
	oris	%r4,%r4,SR_KU >> 16	! r4 holds first segment register value

	! go to Real Mode
	mfmsr	%r7
	li	%r6, MSR_IR|MSR_DR
	andc	%r6,%r7,%r6		! turn off IR and DR
	sync
	isync
	mtmsr	%r6		! go into real mode
	isync

	!
	! loop through segment registers
	!
	li	%r5,0x10		! start the counter
1:
	subic.	%r5,%r5,1	! r5--
	slwi	%r6,%r5,28	! move regnum to MSB for mtsrin
	isync
	mtsrin	%r4,%r6		! write the seg register
	isync
	subic	%r4,%r4,1 
	bne+	1b

	sync
	mtsdr1	%r3		! write the SDR1 register
	isync

	sync
	isync
	mtmsr	%r7		! put IR and DR back on
	isync
	blr			! return
	!
	! This is the equivalent of the MAX_100 macro - we need to
	! verify that the size of this code segment copied to location
	! 0x0 does not exceed 0x100 in size.  If the size does exceed
	! 0x100, then this instruction will generate an assembler warning
	! This instruction will never actually be executed.
	!
	li	%r4, (end_change_sdr_seg - change_sdr_seg - 4) * 0x80


end_change_sdr_seg:
	.text

!
!	Define macros to allow SPRG2 to point to physical save area
!	with an offset of 0 instead of an offset of CPU_R3 for
!	compatibility with kadb's offsets 0, 4, and 8.
!
#define	L0_R3	CPU_R3   - CPU_R3
#define	L0_SRR0	CPU_SRR0 - CPU_R3
#define	L0_SRR1	CPU_SRR1 - CPU_R3
#define	L0_DSISR CPU_DSISR - CPU_R3
#define	L0_DAR	CPU_DAR - CPU_R3

/*
 * Macros describing level 0 handlers and other code to be copied to low
 * memory.  These allow different code to be copied down depending on the
 * platform and whether kadb is present.
 *
 * The list of handlers to copy contains a pointer the the code description,
 * and the low-memory destination address.
 *
 * The code description consists of a number of sections, terminated by a
 * zero-length section.  Each section has a simple header containing the
 * byte count and a flag for the versions that should use the section.
 * The headers aren't copied.
 *
 * Think of these sections as startup-time #ifdefs.
 */

/*
 * HANDLER(vector, desc).
 *	vector = the low memory physical address for the handler.
 *	desc = pointer to the SECTION_LIST comprising the handler.
 */
#define	HANDLER(vector, desc)	\
	.word	vector;			/* copy to this address */	\
	.word	desc;			/* copy from this code section list */

/*
 * SECTION_LIST(label).
 *	label = address of the SECTIONs of code for the handler.
 *	The section list is terminated by a 0 section.
 */
#define	SECTION_LIST(label)	\
	.word	label			/* address of section */

/*
 * SECTION(label, flags).
 *	label = label for this section.
 *	flags = flags indicating how to decide whether to include the section
 *		in the handlers (startup time #ifdef).  For example, the
 *		section may be included only for a certain CPU version, or
 *		only if kadb is present.
 * As a side-effect, label_len is set to the number of bytes of code
 * in this section.  Another symbol, label_start is used internally.
 */
#define SECTION(label, arch_flags) \
label:									\
	.word	(label/**/_len);	/* length not including header */ \
	.word	arch_flags;		/* architecture flags */	\
label/**/_start:			/* label for length calc */

/*
 * SUBSECTION_END(label).
 *	End of a subsection of a handler.  Another subsection follows.
 */
#define	SUBSECTION_END(label)		\
	.set	label/**/_len,.-label/**/_start

/*
 * SECTION_END(label).
 * 	End of a subsection of a handler, with no more sections following.
 */
#define	SECTION_END(label) \
	SUBSECTION_END(label); \
	.word	0, 0

/*
 * Section arch_flags.
 */
#define ARCH_ALL	0	/* no special handling */
#define	ARCH_KADB	1	/* use only if KADB present */
#define ARCH_NO_KADB	2	/* use only if KADB absent */
#define	ARCH_MATCH	4	/* use only if %pvr version matches 0:15 */
#define	ARCH_MISMATCH	8	/* use only if %pvr version mismatches 0:15 */

#define	ARCH(x)		((x)<<16|ARCH_MATCH)
#define	ARCH_NOT(x)	((x)<<16|ARCH_MISMATCH)

#define	ARCH_601	ARCH(1)		/* use only if 601 processor */
#define	ARCH_NON601	ARCH_NOT(1)	/* use only if not 601 processor */

/*
 * Handler length checking macros.
 * These cause an assembler error if the argument is greater than 0x100,
 * or 0x200, the size limit for the level-0 handler.  As an unfortunate
 * side-effect, they generate one instruction, which shouldn't be executed.
 * The technique uses the property that an add immediate (load immediate)
 * operand must be less than 16 bits (signed).
*/
#define	MAX_100(size)	li	%r3, (size-4)*0x80
#define	MAX_200(size)	li	%r3, (size-4)*0x40

/*
 * Table of level 0 handlers.
 *	This table must be ordered by ascending addresses for overlap
 *	detection to work.
 */
	.text
level0_handlers:
	HANDLER(0x100, .L0_common)	/* soft reset */
	HANDLER(0x200, .L0_common)	/* machine check */
	HANDLER(0x300, .L0_common)	/* data storage exception */
	HANDLER(0x400, .L0_common)	/* instruction storage exception */
	HANDLER(0x500, .L500)		/* external interrupt */
	HANDLER(0x600, .L0_common)	/* alignment */
	HANDLER(0x700, .L0_common_kadb)	/* program interrupt */
	HANDLER(0x800, .L0_common)	/* floating-point unavailable */
	HANDLER(0x900, .L900)		/* decrementer interrupt */
	HANDLER(0xa00, .L0_common)	/* reserved */
	HANDLER(0xb00, .L0_common)	/* reserved */
	HANDLER(0xc00, .Lc00)		/* system call */
	HANDLER(0xd00, .L0_common_kadb)	/* trace */
	/*
	 * TLB miss handlers (for the 603) are installed by the secondary
	 * boot while running without address translation enabled.
	 */
					/* 1000 instruction TLB miss (603) */
					/* 1100 data fetch TLB miss (603) */
					/* 1200 data store TLB miss (603) */
	HANDLER(0x1300, .L0_common)	/* breakpoint exception (603) */
	HANDLER(0x1400, .L0_common) 	/* System Management Interrupt (603) */
	HANDLER(0x2000, .L0_common_kadb) /* run mode exception (601-only) */
	HANDLER(FASTTRAP_PADDR, .Lfasttrap)	/* fast trap */
	HANDLER(TRAP_RTN_PADDR, .L_trap_rtn)	/* trap return */
	HANDLER(RESET_PADDR, .hard_reset)	/* reboot/reset */
	HANDLER(SYSCALL_RTN_PADDR, .L_syscall_rtn)	/* syscall return */
	HANDLER(HDLR_END, .dummy_hdlr)	/* dummy handler for size check */
	HANDLER(0, 0)			/* list terminator */

.dummy_hdlr:
	SECTION_LIST(0)			/* empty list for size check */


!-------------------------------------------------------------------------
! Copy level 0 handlers into low physical memory
!-------------------------------------------------------------------------

! Register Usage
!	r3 - source
!	r4 - destination
!	r5 - tmp during word copy
!	r6 - pointer into level0_handlers table
!	r7 - pointer to list of sections
!	r8 - pvr
!	r9 - size in bytes
!	r10 - kadb_entry (non-zero if kadb present)
!	r11,r12 saved BAT entry. Caller doesn't mind if I corrupt r11,r12
!	ctr - word count for copy
!
	ENTRY_NP(copy_handlers)
	mfpvr	%r8
	sri	%r11,%r8,16
	cmpi	%r11,1
	bne	1f
	mfspr	%r11, BAT2U		! Save the BAT
	mfspr	%r12, BAT2L		! Save the BAT
	li	%r0, BATU_LOMEM_MAPPING
	mtspr	BAT2U, %r0		! set BAT2 to map low memory
	li	%r0, BATL_LOMEM_MAPPING
	mtspr	BAT2L, %r0
	b	2f

1:	mfspr	%r11, DBAT2U		! Save the BAT
	mfspr	%r12, DBAT2L		! Save the BAT
	li	%r0, IBATU_LOMEM_MAPPING
	mtspr	DBAT2U, %r0		! set DBAT2 to map low memory
	li	%r0, IBATL_LOMEM_MAPPING
	mtspr	DBAT2L, %r0
2:
	isync

	andis.	%r8, %r8, 0xffff	! clear revision
	lis	%r6, level0_handlers@ha
	la	%r6, level0_handlers@l(%r6)
	subi	%r6, %r6, 4	! pre decrement the ptr
	lis	%r10, _kadb_entry@ha
	lwz	%r10, _kadb_entry@l(%r10)

	!
	! Copy next handler
	!
	li	%r4, 0		! clear previous destination
.h0_loop1:
	lwzu	%r5, 4(%r6)	! destination address in low memory
	lwzu	%r7, 4(%r6)	! source entry table
	cmpi	%r7, 0
	beq-	.h0_done
	subi	%r7, %r7, 4	! pre-decrement section list
	cmpl	%r5, %r4	! compare new destination with old
	blt	.h0_error	! error, overlap
	subi	%r4, %r5, 4	! pre-decrement destination

	!
	! Copy next section
	!
.h0_loop3:
	lwzu	%r3, 4(%r7)	! load pointer to section
	cmpi	%r3, 0
	beq-	.h0_loop1	! done get next handler entry
	subi	%r3, %r3, 4	! pre-decrement section pointer

	!
	! Copy one section
	!
.h0_loop4:
	lwzu	%r9, 4(%r3)	! load size in bytes (not including header
	cmpi	%r9, 0
	beq-	.h0_loop3	! done with this section
	srwi	%r5, %r9, 2	! size in words
	mtctr	%r5		! setup counter for copy
	!
	! Test to see whether the section should be copied
	!
	lwzu	%r5, 4(%r3)	! load flags of section
	andi.	%r0, %r5, ARCH_MATCH
	beq-	.h0_test2	! not the match case
	andis.	%r0, %r5, 0xffff	! mask out flags
	cmp	%r0, %r8	! compare CPU version
	bne-	.h0_skip		! mismatch - disqualified
.h0_test2:
	andi.	%r0, %r5, ARCH_MISMATCH
	beq-	.h0_test3		! not the mismatch case
	andis.	%r0, %r5, 0xffff ! mask out flags
	cmp	%r0, %r8	! compare CPU version
	beq-	.h0_skip	! match - disqualified
.h0_test3:
	andi.	%r0, %r5, ARCH_KADB
	beq-	.h0_test4	! not the kadb-only case
	cmpi	%r10, 0		! KADB present?
	beq-	.h0_skip	! no, disqualified
.h0_test4:
	andi.	%r0, %r5, ARCH_NO_KADB
	beq-	.h0_loop2		! not the kadb-only case
	cmpi	%r10, 0		! KADB present?
	beq-	.h0_loop2	! yes, qualified

.h0_skip:
	add	%r3, %r3, %r9	! skip section
	b	.h0_loop4	! copy next section

	!
	! copy part of section, %cnt words from %r3 to %r4.
	!
.h0_loop2:
	lwzu	%r5,4(%r3)
	stwu	%r5,4(%r4)
	dcbst	%r0,%r4 	! force to memory
	sync			! guarantee dcbst is done before icbi
	icbi	%r0,%r4 	! force out of instruction cache
	bdnz	.h0_loop2
	b	.h0_loop4	! copy next section

.h0_error:
	lis	%r3, .copy_msg@ha
	la	%r3, .copy_msg@l(%r3)
	bl	prom_printf	! Print error message
	b	.		! hang


.h0_done:
	sync			! one sync for the last icbi

	mfpvr	%r3
	sri	%r3,%r3,16
	cmpi	%r3,1
	bne	1f
	mtspr	BAT2U, %r11	! restore BAT2
	mtspr	BAT2L, %r12
	b	2f
1:
	mtspr	DBAT2U, %r11	! restore DBAT2
	mtspr	DBAT2L, %r12
2:
	isync
	blr			! return
	SET_SIZE(copy_handlers)

	.data
.copy_msg:
	.asciz	"copy_handlers:  level0 handler overlap at %x\n"
	.text

/*
 * Code patching routine.
 *	Used to patch BAT values for 601 machines.
 * Entry:
 *	%r3 = list address (lists repeat until terminated by double 0)
 *		list structure:
 *			word 0: old value expected
 *			word 1: new value to set
 *			word 2: mask (one bits where value should be)
 *			words 3 to n-1: addresses to change.
 *			word n: 0
 * Uses:
 *	%r3 = list pointer
 *	%r4 = old instruction expected
 *	%r5 = xor difference between old and new instructions
 *	%r6 = patch location pointer
 *	%r7 = old value
 *	%r8 = mask
 *	%r9 = scratch for comparison
 */
	ENTRY_NP(_patch_code)
	addi	%r3, %r3, -4	! back up pointer
.patch1:
	lwzu	%r4, 4(%r3)	! load old instruction
	cmpi	%r4, 0
	beq-	.patch_done	! ending with empty list
	lwzu	%r5, 4(%r3)	! load new instruction
	lwzu	%r8, 4(%r3)	! load mask
	and	%r4, %r4, %r8	! mask old value
	and	%r5, %r5, %r8	! mask new value
	xor	%r5, %r4, %r5	! %r5 = difference between old and new values
.patch2:
	lwzu	%r6, 4(%r3)	! load address to be updated
	cmpi	%r6, 0		! end of sublist?
	beq-	.patch1		! yes, get next sublist
	lwz	%r7, 0(%r6)	! load instruction
	and	%r9, %r7, %r8	! mask
	cmp	%r9, %r4	! check for match
	bne-	.patch_err	! no match ... give error
	xor	%r7, %r7, %r5	! apply difference
	stw	%r7, 0(%r6)	! store new instruction
	dcbst	%r0,%r6 	! force to memory
	sync			! guarantee dcbst is done before icbi
	icbi	%r0,%r6 	! force out of instruction cache
	b	.patch2

.patch_done:
	sync			! one sync for the last icbi
	isync
	blr			! return

.patch_err:
	lis	%r3, _patch_msg@ha
	la	%r3, _patch_msg@l(%r3)
	mr	%r4, %r6	! second arg is address
	bl	prom_printf	! Print error message
	b	.		! hang
        SET_SIZE(_patch_code)

	.data
_patch_msg:
	.asciz	"patch_code:  mismatch on old instr at %x\n"

/*
 * Patch list for BAT values.  The code should have the PowerPC standard value
 * in it, but that doesn't work for 601, so on 601 processors, we patch to
 * the 601-specific value.
 */
	.align	2
_bat601_patch_list:
	.word	IBATL_LOMEM_MAPPING	! old value - standard PowerPC value
	.word	BATL_LOMEM_MAPPING	! new value - non-standard, 601 value
	.word	0xffff		! mask for immediate field
	.word	.patch_batl1	! address list
	.word	.patch_batl2
	.word	.patch_batl3
	.word	.patch_batl4
	.word	.patch_batl5
	.word	0		! sub-list terminator

	.word	IBATU_LOMEM_MAPPING	! old value - standard PowerPC value
	.word	BATU_LOMEM_MAPPING	! new value - non-standard, 601 value
	.word	0xffff		! mask for immediate field
	.word	.patch_batu1	! address list
	.word	.patch_batu2
	.word	.patch_batu3
	.word	.patch_batu4
	.word	.patch_batu5
	.word	0		! sub-list terminator
	.word	0		! list terminator

#endif	/* lint */

/*
 *	Stack frames for PowerPC exceptions (including interrupts)
 *
 *	Whenever an exception occurs, the level 0 code creates a stack
 *	frame to save the current state of activity.  The saved information
 *	is known as the "regs" structure (see sys/reg.h).  Additionally, an
 *	additional stack frame is created that is often used by C functions
 *	that are called from this machine language code.  The following
 *	diagrams shows the stack frame created:
 *
 *	  r1 -> 0  - back chain (0)
 *		4  - reserved for callee's LR
 *	    Next is the regs structure
 *		8-164
 *
 *	Thus we need SA(MINFRAME + REGSIZE) = 176 bytes for the stack frames.
 *	When in kernel mode, we just decrement the existing kernel mode
 *	stack pointer (r1) by SA(MINFRAME + REGSIZE) to create the new stack
 *	frame, while if we were running in user mode, we just grab the
 *	value in sprg1 that we have previously set up for this purpose.
 *
 *	Here we define the byte offsets used for saving the various
 *	registers during exception handling.
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
#define	REGS_MQ  (R_MQ  * 4 + MINFRAME)

/*
 *	For all level 0 exception handlers, the following is the state when
 *	transferring control to level 1.
 *
 *		r1 -	points to a MINFRAME sized stack frame, followed
 *			by a register save area ("struct regs" described
 *			in reg.h) on the current kernel stack.
 *
 *		the register save area is filled in with saved values for
 *		R1, R2, R4, R5, R6, R20, CR, PC (saved srr0), and MSR (saved
 *		srr1).  The other registers are saved in the level 1 code.
 *
 *		r2 -	curthread
 *
 *		r4 -	trap type (same as interrupt vector, see trap.h)
 *
 *		r5 -	value of the DAR (when trap type is a data fault)
 *
 *		r6 -	value of the DSISR (when trap type is a data fault
 *			or an alignment fault)
 *
 *		r20 -	CPU
 *
 *	The other interrupt handlers are invoked with a single argument
 *	that was provided when the interrupt routine was registered via
 *	ddi_add_intr().  When we go from level 0 to level 1 for non-clock
 *	interrupts, there is nothing special needed to be done.  The
 *	level 1 code needs to read the vector number provided in a
 *	platform-specific manner, e.g., read a on-board status register.
 *
 *----------------------------------------------------------------------------
 *
 *	Level 0 exception handlers (they need to be copied to low memory).
 *
 *	Tactics used include:
 *
 *		minimize extra stores to memory (use proper kernel stack)
 *		loading special registers into proper greg for call to trap()
 *		enable interrupts fairly quickly in level-1 (cmntrap)
 *		be MP-safe (only use sprgN and/or per-cpu locations as temps)
 *
 *	The solution is close to optimum unless there is a penalty for
 *	instruction execution with IR disabled.  If we determine that
 *	this is the case, we should revisit this code.
 *
 *	Some instruction reordering can be done for improvement.
 *	This has not yet been done to keep things clean for now.
 *
 *	It is assumed that trap() is called as follows:
 *
 *		trap(rp, type, dar, dsisr)
 *
 *	where:
 *
 *		rp	- pointer to regs structure
 *		type	- trap type (vector number, e.g., 0x100, 0x200, ...)
 *		dar	- DAR register (traps 0x300 and 0x600)
 *		dsisr	- DSISR register (traps 0x300 and 0x600)
 *
 *	XXXPPC - use BAT 0 for better performance.
 */
/*
 * .L0_common()
 *
 * Common Level 0 Exception handler. This basically saves the state and
 * calls the level 1 common trap handler. Currently the following
 * exceptions are handled by this.
 *	 100	Soft Reset exception trap (NMI trap?)
 *	 200	Machine check
 *	 300	Data Storage Exception
 *	 400	Instruction Storage Exception
 *	 800	Floating Point Unavailable Exception
 *	 a00	Reserved
 *	 b00	Reserved
 *	1300	Instruction Address Breakpoint Exception (603 specific)
 *	1400	System Management Interrupt (603 specific)
 *
 * NOTE: When special handling is required for any of the above exceptions
 *	then they need seperate handlers.
 */
#if !defined(lint) && !defined(__lint)
	.text
.L0_common:
	SECTION_LIST(.L0_entry)
	SECTION_LIST(.L0_cmntrap)
	SECTION_LIST(0)

	MAX_100(.L0_entry_len1 + .L0_cmntrap_len)
	MAX_100(.L0_entry_len2 + .L0_cmntrap_len)

.L0_entry:
	SECTION(.L0_entry1, ARCH_ALL)

				! States are 's'means saved, 't' in tmp,
				!     'T' means in 2nd tmp, 'Z' means in
				!     last temp, and '.' means needs saving.
				! 1245630pmc
				! ..........
	mtsprg	3,%r1		! t.........
	mfsprg	%r1,2		! t.........
	stw	%r3,L0_R3(%r1)	! tt........
	mfsrr0	%r3		! tt.....t..
	stw	%r3,L0_SRR0(%r1)	! tt.....T..
	mfsrr1	%r3		! tt.....Tt.
	stw	%r3,L0_SRR1(%r1)	! tt.....TT.
	mfdsisr	%r3
	stw	%r3, L0_DSISR(%r1)
	mfdar	%r3
	stw	%r3, L0_DAR(%r1)
	mfmsr	%r1		! tt.....TT.
	ori	%r1,%r1,MSR_DR	! enable virtual addressing for data
				! We need to save CR before we use it below
	mfcr	%r3		! tt.....TTt
	mtmsr	%r1		! tt.....TTt
	mfsrr1	%r1		! tt.....TTt
	andi.	%r1,%r1,MSR_PR	! was in kernel mode?
	beq-	1f		! If not, so use the bottom of the kernel stack
	lis	%r1, 0x2000
	ori	%r1, %r1, 0x1e
	isync
	mtsr	14, %r1
	ori	%r1, %r1, 0x1f	! set SR14,15 with kernel SR values
	mtsr	15, %r1
	isync
	mfsprg	%r1,1		! curthread->t_stack
	b	2f
1:
				! YES, so make room for a new kernel stack frame
	mfsprg	%r1,3		! get old kernel stack pointer, then adjust it
	subi	%r1,%r1, SA(REGSIZE + MINFRAME) ! make room for registers
2:
!	r1 now points to a valid kernel stack frame for saving evethang.
				! tt.....TTt
	stw	THREAD_REG,REGS_R2(%r1)	! tt...s.TTt make room for curthread
	stw	%r20,REGS_R20(%r1)	! tt...ssTTt make room for CPU
	stw	%r3,REGS_CR(%r1)	! tt...ssTTs save CR
	mfsprg	THREAD_REG,0	! establish curthread in THREAD_REG
	stw	%r4,REGS_R4(%r1)	! tts..ssTTs
	lwz	%r20,T_CPU(THREAD_REG)	! establish CPU in r20
	stw	%r6,REGS_R6(%r1)	! tts.sssTTs
	lwz	%r3,CPU_SRR0(%r20)	! tts.sssZTs
	stw	%r5,REGS_R5(%r1)	! ttsssssZTs
	lwz	%r4,CPU_SRR1(%r20)	! ttsssssZZs
	mfsprg	%r6,3			! TtsssssZZs
	stw	%r3,REGS_PC(%r1)	! TtssssssZs save PC
	stw	%r4,REGS_MSR(%r1)	! Ttssssssss save MSR
	stw	%r6,REGS_R1(%r1)	! stssssssss
	SUBSECTION_END(.L0_entry1)
	SECTION(.L0_mq_save, ARCH_601)
	mfmq	%r3			! save the MQ reg
	stw	%r3, REGS_MQ(%r1)	! could be saved only on trap from user
	SUBSECTION_END(.L0_mq_save)
	SECTION(.L0_batset, ARCH_ALL)
.patch_batu1:
	li	%r3, IBATU_LOMEM_MAPPING
.patch_batl1:
	li	%r4, IBATL_LOMEM_MAPPING
	mtspr	BAT3U, %r3		! BATU3 or IBATU3
	mtspr	BAT3L, %r4		! BATL3 or IBATL3

	mflr	%r4
	stw	%r4, REGS_LR(%r1)	! save LR
	SECTION_END(.L0_batset)

/*
 * L0_entry has two possible lengths.  It would be OK now to use just the
 * longer one for the length check, but both are maintained in case the
 * two choices are re-introduced and it isn't clear which is longer.
 */
.L0_entry_len1	=	.L0_entry1_len + .L0_batset_len
.L0_entry_len2	=	.L0_entry1_len + .L0_mq_save_len + .L0_batset_len

/*
 * Section for L0 handler to enter cmntrap.
 */
	SECTION(.L0_cmntrap, ARCH_ALL)
	lwz	%r6,CPU_MSR_DISABLED(%r20)	! MSR for traps
	lwz	%r5,CPU_CMNTRAP(%r20)	! address of cmntrap()
	lwz	%r3,CPU_R3(%r20)	! ssssssssss
	mtlr	%r5
	mtmsr	%r6			! MSR_IR on
	isync				! MSR_IR takes effect
	blrl				! jump to cmntrap and put type in lr
	SECTION_END(.L0_cmntrap)

/*
 * Hard Reset - Jump to ROM
 */
.hard_reset:
	SECTION_LIST(.hard_resets)
	SECTION_LIST(0)

	MAX_200(.hard_resets_len + .hard_reset_cmn_len)
	MAX_200(.hard_reset_std_len + .hard_reset_cmn_len)

	SECTION(.hard_resets, ARCH_601)

	! Assume interrupts disabled.

	!!-------------------------------------------------------------
	!! HID0 contents on PowerON reset
	!!-------------------------------------------------------------
	lis	%r2, EXT16(0x8001)	! same as lis of 0x8001 without warning
	ori	%r2, %r2, 0x0080

	!!-------------------------------------------------------------
	!! THE FOLLOWING: location 0x95 (in BE mode) is where we want to go
	!! but the address is "munged" 95 XOR 7 = 92 in LE mode
	!! (this is what changes the mem subsystem interface!)
	!!-------------------------------------------------------------
	li      %r5, 0				!! this will go out...
	lis	%r29, PCIISA_VBASE >> 16	!! to this port
	li      %r28, 0x95
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	sync
	sync
	sync
	mtspr   HID0, %r2             ! Disable LE on 601
	sync
	sync
	sync
	sync
	stbx	%r5, %r28, %r29           !! and the mem
	eieio
	SUBSECTION_END(.hard_resets)	/* continues with .hard_reset_cmn */

/*
 * Hard Reset for 603/604 - Jump to ROM
 */
	SECTION(.hard_reset_std, ARCH_NON601)

	! Assume interrupts disabled.

	mfspr	%r2,HID0
	ori	%r2,%r2,0x0800		! ICFI on
	rlwinm	%r2,%r2,0x0,0x11,0xf	! ICE off
	isync
	mtspr	HID0,%r2		! I-cache is disabled

	rlwinm	%r2,%r2,0x0,0x12,0x10	! DCE off
	ori	%r2,%r2,0x0400		! DCFI on
	isync
	mtspr	HID0,%r2		! D-cache is disabled
	sync
	sync
	nop
	nop
	nop

	mfmsr	%r2
	rlwinm	%r2, %r2,0,0,30		! LE off
	rlwinm	%r2, %r2,0,16,14	! ILE off
	li	%r5,0x0000
	lis	%r29, PCIISA_VBASE >> 16
	li	%r28, 0x95
	sync
	mtmsr	%r2
	isync
	sync
	sync
	sync
	sync
	stbx	%r5, %r28, %r29
	sync
	SUBSECTION_END(.hard_reset_std)

	SECTION(.hard_reset_cmn, ARCH_ALL)

	!!-------------------------------------------------------------
	!! String of palindromic instructions that work the same
	!! regardless of whether they are fetched and interpreted in
	!! Big-Endian mode or Little-Endian mode.
	!! Somewhere in the midst of this list the machine actually gets
	!! itself into Big-Endian mode; further assembly is done for
	!! Big-Endian.
	!!-------------------------------------------------------------
	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138

 	.endian big       !! switch the assembler to Big-Endian mode

	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138
	addi    %r0, %r1, 0x138

	mfmsr	%r0
	ori	%r0,%r0,0x3000
	andi.	%r0,%r0,0x7fcf
	mtsrr1	%r0
	mtmsr	%r0
	isync
	mtsrr1	%r0
	lis	%r0,EXT16(0xfff0)
	ori	%r0,%r0,0x0100
	mtsrr0	%r0
	rfi
 	.endian little       !! switch the assembler to Little-Endian mode
	SECTION_END(.hard_reset_cmn)

/*
 * External Interrupts
 */
.L500:
	SECTION_LIST(.L0_entry)
	SECTION_LIST(.L0_cmnint)
	SECTION_LIST(0)

	MAX_100(.L0_entry_len1 + .L0_cmnint_len)
	MAX_100(.L0_entry_len2 + .L0_cmnint_len)

	/*
 	 * Section .L0_entry has setup the stack, BATs, and started the save.
	 * Enable IR and go to cmnint.
 	 */
	SECTION(.L0_cmnint, ARCH_ALL)

	lwz	%r4,CPU_MSR_DISABLED(%r20)	! MSR for interrupts
	lwz	%r5,CPU_CMNINT(%r20)	! cmnint code address
	lwz	%r3,CPU_R3(%r20)	! ssssssssss
	mtlr	%r5
	mtmsr	%r4			! MSR_IR on
	isync				! MSR_IR takes effect

	blrl				! branch high to cmnint
	SECTION_END(.L0_cmnint)

/*
 * Level-0 kadb trap.
 *	Level 0 handler which allows kadb (if present) to intercept the trap.
 *	Otherwise, same as L0_common.
 * 	Used for:
 *	700	Program interrupt
 *	d00	Trace interrupt
 *	2000 	Run mode exception (601 only).
 */
.L0_common_kadb:
	SECTION_LIST(.L0_entry)		/* standard trap entry code */
	SECTION_LIST(.L0_kadb_trap)	/* call kadb if present */
	SECTION_LIST(0)

	MAX_100(.L0_entry_len1 + .L0_kadb_trap_len)
	MAX_100(.L0_entry_len2 + .L0_kadb_trap_len)
	MAX_100(.L0_entry_len1 + .L0_no_kadb_len)
	MAX_100(.L0_entry_len2 + .L0_no_kadb_len)

/*
 * Section to enter KADB if present, otherwise the cmntrap section is used.
 */
	SECTION(.L0_kadb_trap, ARCH_KADB)
	lwz	%r4,CPU_MSR_KADB(%r20)	! MSR for traps
	lwz	%r5,CPU_KADB(%r20)	! trap_kadb code address
	lwz	%r3,CPU_R3(%r20)	! ssssssssss
	mtmsr	%r4			! MSR_IR on
	isync				! MSR_IR takes effect
	mtlr	%r5
	blrl				! branch high to trap_kadb()
	SUBSECTION_END(.L0_kadb_trap)

	/*
	 * Version of above for when kadb is not present - call cmntrap.
	 */
	SECTION(.L0_no_kadb, ARCH_NO_KADB)
	lwz	%r6,CPU_MSR_DISABLED(%r20)	! MSR for traps
	lwz	%r5,CPU_CMNTRAP(%r20)	! address of cmntrap()
	lwz	%r3,CPU_R3(%r20)	! ssssssssss
	mtlr	%r5
	mtmsr	%r6			! MSR_IR on
	isync				! MSR_IR takes effect
	blrl				! jump to cmntrap and put type in lr
	SECTION_END(.L0_no_kadb)

/*
 * Decrementer Interrupt
 */
.L900:
	SECTION_LIST(.L0_entry)
	SECTION_LIST(.L0_decr)
	SECTION_LIST(0)

	MAX_100(.L0_entry_len1 + .L0_decr_len)
	MAX_100(.L0_entry_len2 + .L0_decr_len)

	SECTION(.L0_decr, ARCH_ALL)
	lwz	%r4,CPU_MSR_DISABLED(%r20)	! MSR for clock interrupt
	lwz	%r5,CPU_CLOCKINTR(%r20)	! clock_intr code address
	lwz	%r3,CPU_R3(%r20)	! ssssssssss
	mtmsr	%r4			! MSR_IR on
	isync				! MSR_IR takes effect
	mtlr	%r5
	blrl				! branch high to clock_intr()
	SECTION_END(.L0_decr)

/*
 * System Call Interrupt
 */
.Lc00:
	SECTION_LIST(.L0_fastcheck)
	SECTION_LIST(.L0_entry)
	SECTION_LIST(.L0_syscall)
	SECTION_LIST(0)

	MAX_100(.L0_fastcheck_len + .L0_entry_len1 + .L0_syscall_len)
	MAX_100(.L0_fastcheck_len + .L0_entry_len2 + .L0_syscall_len)

	SECTION(.L0_fastcheck, ARCH_ALL)

	cmpi	%r0, -1
	bne+	.regsyscall	! if %r0 > 0 then regular syscall
				! else a fast syscall

	cmpi	%r3, SC_GETHRESTIME
	bgt	.fast_unknown
	ba	FASTTRAP_PADDR	! branch to .Lfasttrap (physical addr)

.fast_unknown:
!!!	li	%r0, 0		! put 0 in %r0 (invalid syscall)
!  	b	.regsyscall	! do regular syscall

.regsyscall:
	SECTION_END(.L0_fastcheck)

	SECTION(.L0_syscall, ARCH_ALL)
	lwz	%r4,CPU_MSR_DISABLED(%r20)	! MSR for syscalls
	lwz	%r5,CPU_SYSCALL(%r20)	! sys_call code address
	lwz	%r3,CPU_R3(%r20)	! ssssssssss
	mtmsr	%r4			! MSR_IR on
	isync				! MSR_IR takes effect
	mtlr	%r5
	blrl				! branch high to sys_call()
	SECTION_END(.L0_syscall)

! Fast syscalls (like gethrtime) are handled here. %r3 contains code for
! fast syscall. Volatile registers need not be saved. %r3, %r4 contain
! longlong value on return from geth*time calls & %r3 contains fpuflags on
! return from getlwpfpu.

.Lfasttrap:
	SECTION_LIST(.Lfasttraps)
	SECTION_LIST(0)

	MAX_200(.Lfasttraps_len)

	SECTION(.Lfasttraps, ARCH_ALL)
	mtsprg	3,%r1		! save sp in sprg3
	mfsprg	%r1,2		! get cpu ptr (phys addr) in %r1
	mfsrr0	%r4
	stw	%r4,L0_SRR0(%r1)! save srr0 in cpu struct
	mfsrr1	%r5
	stw	%r5,L0_SRR1(%r1)! save srr1 in cpu struct

	lis	%r1, 0x2000
	ori	%r1, %r1, 0x1e
	isync
	mtsr	14, %r1
	ori	%r4, %r4, 0x1f	! set SR14,15 with kernel SR values
	mtsr	15, %r4
	isync
.patch_batu2:
	li	%r4, IBATU_LOMEM_MAPPING	! 603, 604, 620 IBAT 1-1 mapping for 8M
.patch_batl2:
	li	%r5, IBATL_LOMEM_MAPPING
	mtspr	BAT3U, %r4		! BATU3 or IBATU3
	mtspr	BAT3L, %r5		! BATL3 or IBATL3

	mfmsr	%r1
	ori	%r1, %r1, MSR_DR|MSR_IR
	mtmsr	%r1		! turn on translations
	isync

	mfsprg	%r1, 1		! get kernel stack from sprg1

	stw	THREAD_REG, REGS_R2(%r1)	! save %r2
	mflr	%r4
	mfsprg	THREAD_REG, 0			! set %r2 to curthread
	stw	%r4, REGS_LR(%r1)		! save lr

	cmpi	%r3, SC_GETLWPFPU
	beq	.Lgetlwpfpu
	cmpi	%r3, SC_GETHRTIME
	beq	.Lgethrtime
	cmpi	%r3, SC_GETHRVTIME
	beq	.Lgethrvtime
	cmpi	%r3, SC_GETHRESTIME
	beq	.Lgethrestime

.Lgetlwpfpu:
	lwz	%r3, T_LWP(THREAD_REG)
	lwz	%r3, LWP_PCB_FLAGS(%r3)
	b	.Lfastexit

.Lgethrtime:
	lis	%r4, gethrtime@ha
	ori	%r4, %r4, gethrtime@l
	b	.Lgethtimecall

.Lgethrvtime:
	lis	%r4, gethrvtime@ha
	ori	%r4, %r4, gethrvtime@l
	b	.Lgethtimecall

.Lgethrestime:
	lis	%r4, get_hrestime@ha
	ori	%r4, %r4, get_hrestime@l

.Lgethtimecall:
	mtlr	%r4
	blrl			! branch high into kernel segment

.Lfastexit:
	lwz	%r5, REGS_LR(%r1)
	lwz	THREAD_REG, REGS_R2(%r1)	! restore %r2

	mfmsr	%r8
	rlwinm	%r8, %r8, 0, 28, 25		! clear IR and DR
	mtmsr	%r8		! turn off translations
	isync


	li	%r8, 0
	mtspr	BAT3U, %r8		! BATU3 or IBATU3 invalidate
	mtspr	BAT3L, %r8		! BATL3 or IBATL3 invalidate

.Lretfromfast:
	mtsr	14, %r8
	mtsr	15, %r8		! invalidate SR14,15 for user

	mfsprg	%r1, 2
	lwz	%r6, L0_SRR0(%r1)
	lwz	%r7, L0_SRR1(%r1)
	mtlr	%r5				! restore lr
	mtsrr0	%r6				! restore srr0
	mtsrr1	%r7				! restore srr1
	mfsprg	%r1, 3				! restore sp
	rfi					! RET to userland
						! %r3, %r4 contain return vals
	SECTION_END(.Lfasttraps)

!
! Return from trap code that executes in low-memory, since we have to run
! in physical address mode when restoring srr0, srr1.

.L_trap_rtn:
	SECTION_LIST(.L_trap_rtns)
	SECTION_LIST(0)

	MAX_200(.L_trap_rtns_len + .L_trap_rtn_mq_len + .L_trap_rtn_usr_len)

	SECTION(.L_trap_rtns, ARCH_ALL)

	lwz	%r11, T_CPU(THREAD_REG)	! get cpu ptr (virt addr) in %r11

	lwz	%r4,REGS_PC(%r1)
	lwz	%r5,REGS_MSR(%r1)

	stw	%r4, CPU_SRR0(%r11)	! store srr0, srr1 in cpu struct.
	stw	%r5, CPU_SRR1(%r11)

	lwz	%r6,REGS_LR(%r1)
	lwz	%r8,REGS_CTR(%r1)
	mtlr	%r6			! restore LR
	lwz	%r9,REGS_XER(%r1)
	mtctr	%r8			! restore CTR
	mtxer	%r9			! restore XER

	andi.	%r5, %r5, MSR_PR
	bne+	.user_return
!
!	restore general registers
!

.kernel_return:
	lwz	%r0,REGS_R0(%r1)
	lwz	%r2,REGS_R2(%r1)
	lwz	%r3,REGS_R3(%r1)
	lwz	%r4,REGS_R4(%r1)

	stw	%r3, CPU_R3(%r11)	! save r3 in cpu struct
	lwz	%r3,REGS_CR(%r1)

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

	mtsprg	3, %r1

	mfmsr	%r1
	rlwinm	%r1, %r1, 0, 28, 25		! clear IR and DR
	mtmsr	%r1				! xlations off
	isync

	li	%r1, 0
	mtspr	BAT3U, %r1		! BATU3 or IBATU3 invalidate
	mtspr	BAT3L, %r1		! BATL3 or IBATL3 invalidate

	mtcrf	0xff, %r3		! restore CR
	mfsprg	%r3, 2			! get cpu ptr (phys addr) in %r3

	lwz	%r1, L0_SRR0(%r3)
	mtsrr0	%r1				! restore srr0
	lwz	%r1, L0_SRR1(%r3)
	mtsrr1	%r1				! restore srr1
	lwz	%r3, L0_R3(%r3)			! restore r3
	mfsprg	%r1, 3				! restore r1

	rfi				! Back, to the Future

.user_return:
	SUBSECTION_END(.L_trap_rtns)

	/*
	 * Subsection to restore MQ register (601-only).
	 */
	SECTION(.L_trap_rtn_mq, ARCH_601)
	lwz	%r0,REGS_MQ(%r1)
	mtmq	%r0
	SUBSECTION_END(.L_trap_rtn_mq)

	/*
	 * Sub-section to restore user registers.
	 */
	SECTION(.L_trap_rtn_usr, ARCH_ALL)

	lwz	%r0,REGS_R0(%r1)
	lwz	%r2,REGS_R2(%r1)
	lwz	%r3,REGS_R3(%r1)
	lwz	%r4,REGS_R4(%r1)

	stw	%r3, CPU_R3(%r11)	! save r3 in cpu struct
	lwz	%r3,REGS_CR(%r1)

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

	mtsprg	3, %r1

	mfmsr	%r1
	rlwinm	%r1, %r1, 0, 28, 25		! clear IR and DR
	mtmsr	%r1				! xlations off
	isync

	li	%r1, 0
	mtspr	BAT3U, %r1		! BATU3 or IBATU3 invalidate
	mtspr	BAT3L, %r1		! BATL3 or IBATL3 invalidate

	mtsr	14, %r1			! invalidate SR14,15 since
	mtsr	15, %r1			! returning to user

	mtcrf	0xff, %r3		! restore CR
	mfsprg	%r3, 2			! get cpu ptr (phys addr) in %r3

	lwz	%r1, L0_SRR0(%r3)
	mtsrr0	%r1				! restore srr0
	lwz	%r1, L0_SRR1(%r3)
!
! Paranoia - the next line guarantees that we do not return to user mode
! from interrupt/trap with interrupt disabled. This seems to have fixed
! some system hangs(?).
!
	ori	%r1,%r1,MSR_EE		! msr - interrupts enabled
	mtsrr1	%r1				! restore srr1
	lwz	%r3, L0_R3(%r3)			! restore r3
	mfsprg	%r1, 3				! restore r1

	rfi				! Back, to the Future

	SECTION_END(.L_trap_rtn_usr)


!
! Return from syscall that executes in low-memory, since we have to run
! in physical address mode when restoring srr0, srr1.

.L_syscall_rtn:
	SECTION_LIST(.L_syscall_rtns)
	SECTION_LIST(0)

	MAX_200(.L_syscall_rtns_len)

	SECTION(.L_syscall_rtns, ARCH_ALL)

	lwz	%r11, T_CPU(THREAD_REG)	! get cpu ptr (virt addr) in %r11
					! XXX - should already be in %r20

	lwz	%r15,REGS_PC(%r1)
	lwz	%r16,REGS_MSR(%r1)

	stw	%r15, CPU_SRR0(%r11)	! store srr0, srr1 in cpu struct.
	stw	%r16, CPU_SRR1(%r11)

	lwz	%r15,REGS_LR(%r1)
	mtlr	%r15			! restore LR
	SUBSECTION_END(.L_syscall_rtns)

	/*
	 * Subsection to restore MQ register (601-only).
	 */
	SECTION(.L_sc_rtn_mq, ARCH_601)
	lwz	%r0,REGS_MQ(%r1)	/* MQ could be considered volatile */
	mtmq	%r0			/* across system calls */
	SUBSECTION_END(.L_sc_rtn_mq)


	/*
	 * Sub-section to restore user registers.
	 *
	 * Since this is a normal return, it should not be necessary to restore
	 * the volatile registers %r5-%r13, but libc may be relying on some
	 * of them.
	 */
	SECTION(.L_sc_rtn_usr, ARCH_ALL)

	lwz	%r0,REGS_R0(%r1)
	lwz	%r2,REGS_R2(%r1)
					! %r3-%r4 already restored (rvals)
	stw	%r3, CPU_R3(%r11)	! save r3 in cpu struct
	lwz	%r3,REGS_CR(%r1)	! restore CR - but clear SO

	lwz	%r5,REGS_R5(%r1)	! %r5-%r13 shouldn't need restoring
	lwz	%r6,REGS_R6(%r1)	! but are used by libc
	lwz	%r7,REGS_R7(%r1)
	lwz	%r8,REGS_R8(%r1)
	lwz	%r9,REGS_R9(%r1)
	lwz	%r10,REGS_R10(%r1)
	lwz	%r11,REGS_R11(%r1)
	lwz	%r12,REGS_R12(%r1)
	lwz	%r13,REGS_R13(%r1)
	lwz	%r14,REGS_R14(%r1)	! %r14-%r16 were used by sys_call
	lwz	%r15,REGS_R15(%r1)
	lwz	%r16,REGS_R16(%r1)
					! %r17-%r19 preserved by sys_call and C
	lwz	%r20,REGS_R20(%r1)	! %r20-%r21 were used by sys_call
	lwz	%r21,REGS_R21(%r1)
	lwz	%r22,REGS_R22(%r1)
					! %r23-%r31 preserved by sys_call and C
	lwz	%r1,REGS_R1(%r1)

	mtsprg	3, %r1

	mfmsr	%r1
	rlwinm	%r1, %r1, 0, 28, 25	! clear IR and DR
	mtmsr	%r1			! xlations off
	isync

	li	%r1, 0
	mtspr	BAT3U, %r1		! BATU3 or IBATU3 invalidate
	mtspr	BAT3L, %r1		! BATL3 or IBATL3 invalidate

	mtsr	14, %r1			! invalidate SR14,15 since
	mtsr	15, %r1			! returning to user

	mtctr	%r1			! clear CTR
	mtxer	%r1			! clear XER

	lis	%r1, CR0_SO >> 16
	andc	%r3, %r3, %r1		! turn off SO (error) bit
	mtcrf	0xff, %r3		! indicate success in CR reg
	mfsprg	%r3, 2			! get cpu ptr (phys addr) in %r3

	lwz	%r1, L0_SRR0(%r3)
	mtsrr0	%r1			! restore srr0
	lwz	%r1, L0_SRR1(%r3)
	mtsrr1	%r1			! restore srr1
	lwz	%r3, L0_R3(%r3)		! restore r3
	mfsprg	%r1, 3			! restore r1

	rfi				! Back, to the Future

	SECTION_END(.L_sc_rtn_usr)

#endif /* lint */



/*
 *	Level 1 interrupt (and exception) handlers
 *
 *	NOTE that this code is mostly a port of the x86 ml code.
 *
 *	General register usage in interrupt loops is as follows:
 *
 *		r2  - curthread (always set up by level 1 code)
 *		r14 - interrupt vector
 *		r15 - pointer to autovect structure for handler
 *		r16 - number of handlers in chain
 *		r17 - is DDI_INTR_CLAIMED status of chain
 *		r18 - autovect pointer
 *		r19 - cpu_on_intr
 *		r20 - CPU
 *		r21 - msr (with interrupts disabled)
 *		r22 - msr (with interrupts enabled)
 *		r23 - old ipl
 *		r24 - stack pointer saved before changing to interrupt stack
 */
#if defined(lint) || defined(__lint)

void
_interrupt(void)
{}

void
cmnint(void)
{}

#else   /* lint */

	ENTRY_NP2(cmnint,_interrupt)
#ifdef TRAPTRACE
	TRACE_PTR(%r3, %r4);		! get pointer into %r3 using %r4

	mflr	%r4			! get trap type
	subi	%r4, %r4, 4		! in case blrl was at end of l0 handler
	stw	%r4, TRAP_ENT_TT(%r3)	! put trap type in trace
	lwz	%r4, CPU_SRR1(%r20)
	stw	%r4, TRAP_ENT_MSR(%r3)	! put old MSR in trace
	lwz	%r4, CPU_SRR0(%r20)
	stw	%r4, TRAP_ENT_PC(%r3)	! put old PC in trace
	lwz	%r4, REGS_R1(%r1)
	stw	%r4, TRAP_ENT_SP(%r3)	! put old SP in trace
	stw	THREAD_REG, TRAP_ENT_R2(%r3) ! put THREAD_REG in trace
	lwz	%r4, REGS_LR(%r1)
	stw	%r4, TRAP_ENT_X1(%r3)	! put old link register in trace

	TRACE_NEXT(%r3, %r4, %r5)	! update trace pointer
	lwz	%r3, CPU_R3(%r20)	! restore %r3
	lwz	%r4, REGS_R4(%r1)	! restore %r4
	lwz	%r5, REGS_R5(%r1)	! restore %r5
#endif /* TRAPTRACE */
					! 03789012345678912345678901lcx
					! .............................
	stw	%r0,REGS_R0(%r1)	! s............................
	li	%r0, 0
	mtspr	BAT3U, %r0		! BATU3 or IBATU3 invalidate
	mtspr	BAT3L, %r0		! BATL3 or IBATL3 invalidate
	stw	%r3,REGS_R3(%r1)	! .s...........................
	stw	%r7,REGS_R7(%r1)	! ..s..........................
	stw	%r8,REGS_R8(%r1)	! ...s.........................
	stw	%r9,REGS_R9(%r1)	! ....s........................
	stw	%r10,REGS_R10(%r1)	! .....s.......................
	stw	%r11,REGS_R11(%r1)	! ......s......................
	stw	%r12,REGS_R12(%r1)	! .......s.....................
	stw	%r13,REGS_R13(%r1)	! ........s....................
	stw	%r14,REGS_R14(%r1)	! .........s...................
	stw	%r15,REGS_R15(%r1)	! ..........s..................
	mfctr	%r9
	stw	%r16,REGS_R16(%r1)	! ...........s.................
	stw	%r17,REGS_R17(%r1)	! ............s................
	stw	%r18,REGS_R18(%r1)	! .............s...............
	mfxer	%r10
	stw	%r19,REGS_R19(%r1)	! ..............s..............
	stw	%r21,REGS_R21(%r1)	! ...............s.............
	stw	%r22,REGS_R22(%r1)	! ................s............
	stw	%r23,REGS_R23(%r1)	! .................s...........
	stw	%r24,REGS_R24(%r1)	! ..................s..........
	stw	%r25,REGS_R25(%r1)	! ...................s.........
	stw	%r26,REGS_R26(%r1)	! ....................s........
	stw	%r27,REGS_R27(%r1)	! .....................s.......
	stw	%r28,REGS_R28(%r1)	! ......................s......
	stw	%r29,REGS_R29(%r1)	! .......................s.....
	stw	%r30,REGS_R30(%r1)	! ........................s....
	stw	%r31,REGS_R31(%r1)	! .........................s...
	mfmsr	%r21			! msr - interrupts disabled
	stw	%r9,REGS_CTR(%r1)	! ...........................s.
	lwz	%r23,CPU_PRI(%r20)	! get current ipl
	stw	%r10,REGS_XER(%r1)	! ............................s
	ori	%r22,%r21,MSR_EE	! msr - interrupts enabled
!
! NOTE: we are free to use registers freely.
!

	lis	%r5,setlvl@ha
	lwz	%r5,setlvl@l(%r5)
	la	%r4,0(%r1)		! &vector (set by setlvl())
	mr	%r3,%r23		! current spl level
	mtlr	%r5
	blrl				! call  *setlvl
	lwz	%r14,0(%r1)		! setlvl can modify the vector

	! check for spurious interrupt
	cmpi	%r3,-1
	beq-	_sys_rtt_spurious

 	! At this point we can take one of two paths.  If the new
 	! priority level is below LOCK LEVEL, then we jump to code that
	! will run this interrupt as a separate thread.  Otherwise the
	! interrupt is run on the "interrupt stack" defined in the cpu
	! structure (similar to the way interrupts always used to work).

	cmpi	%r3,LOCK_LEVEL		! compare to highest thread level
	stw	%r3,CPU_PRI(%r20)	! update ipl
	blt+	intr_thread		! process as a separate thread

	!
	! Handle high_priority nested interrupt on separate interrupt stack
	!

	lwz	%r19,CPU_ON_INTR(%r20)
	sli	%r18,%r14,3		! vector*8 (for index into autovect below)
	cmpi	%r19,0
	addi	%r18,%r18,AVH_LINK	! adjust autovect pointer by link offset
	li	%r24, 0
	bne-	onstack			! already on interrupt stack
	li	%r4,1
	mr	%r24,%r1		! save the thread stack pointer
	stw	%r4,CPU_ON_INTR(%r20)	! mark that we're on the interrupt stack
	lwz	%r1,CPU_INTR_STACK(%r20) ! get on interrupt stack
onstack:
	stw	%r24,0(%r1)		! for use by kadb stack tracing code
	addis	%r18,%r18,autovect@ha
	addi	%r18,%r18,autovect@l	! r18 is now the address of the list of handlers
	mtmsr	%r22			! sti
	!
	! Get handler address
	!
pre_loop1:
	lwz	%r15,0(%r18)		! autovect for 1st handler
	li	%r16,0			! r16 is no. of handlers in chain
	li	%r17,0			! r17 is DDI_INTR_CLAIMED status of chain
loop1:
 	cmpi  	%r15,0			! if pointer is null
 	beq-   	.intr_ret		! then skip
 	lwz	%r0,AV_VECTOR(%r15)	! get the interrupt routine
 	lwz	%r3,AV_INTARG(%r15)	! get argument to interrupt routine
	cmpi	%r0,0			! if func is null
	mtlr	%r0
	beq-	.intr_ret		! then skip
	addi	%r16,%r16,1
	blrl				! call interrupt routine with arg
 	lwz	%r15,AV_LINK(%r15)	! get next routine on list
	or	%r17,%r17,%r3		! see if anyone claims intpt.
	b	loop1			! keep looping until end of list

.intr_ret:
	cmpi	%r16,1			! if only 1 intpt in chain, it is OK
	beq+	.intr_ret1
	cmpi	%r17,0			! If no one claims intpt, then it is OK
	beq-	.intr_ret1
	b	pre_loop1		! else try again.

.intr_ret1:
	mtmsr	%r21			! cli
	lwz	%r3,CPU_SYSINFO_INTR(%r20)
	stw	%r23,CPU_PRI(%r20)	! restore old ipl
	addi	%r3,%r3,1		! cpu_sysinfo.intr++
	stw	%r3,CPU_SYSINFO_INTR(%r20)
	lis	%r5,setlvlx@ha
	mr	%r4,%r14		! interrupt vector
	lwz	%r5,setlvlx@l(%r5)
	mr	%r3,%r23		! old ipl
	mtlr	%r5
	blrl				! call  *setlvlx
					! r3 contains the current ipl
	cmpi	%r24,0
	beq-	.intr_ret2
	li	%r0, 0
	stw	%r0,CPU_ON_INTR(%r20)
	mr	%r1,%r24		! restore the previous stack pointer
.intr_ret2:
!!!XXXPPC - check for need to call clock()!!! or just wait for next tick???
 	b	dosoftint		! check for softints before we return.
	SET_SIZE(cmnint)

#endif	/* lint */

/*
 * Handle an interrupt in a new thread.
 *	Entry:  traps disabled.
 *		r1	- pointer to regs
 *		r2	- curthread
 *		r3	- interrupt level for this interrupt
 *		r4	- (new) interrupt thread
 *		r14	- vector
 *		r20	- CPU
 *		r21	- msr for interrupts disabled
 *		r22	- msr for interrupts enabled
 *		r23	- old spl
 *		r25	- save for stashing unsafe entry point
 *		r26	- saved interrupt level for this interrupt
 */
/*
 *	General register usage in interrupt loops is as follows:
 *
 *		r2  - curthread (always set up by level 1 code)
 *		r14 - interrupt vector
 *		r15 - pointer to autovect structure for handler
 *		r16 - number of handlers in chain
 *		r17 - is DDI_INTR_CLAIMED status of chain
 *		r18 - autovect pointer
 *		r19 - cpu_on_intr
 *		r20 - CPU
 *		r21 - msr (with interrupts disabled)
 *		r22 - msr (with interrupts enabled)
 *		r23 - old ipl
 *		r24 - stack pointer saved before changing to interrupt stack
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
intr_thread(void)
{}

#else	/* lint */
	.globl intr_thread
intr_thread:
	! Get set to run interrupt thread.
	! There should always be an interrupt thread since we allocate one
	! for each level on the CPU, and if we release an interrupt, a new
	! thread gets created.
	!
	!    t = CPU->cpu_intr_thread
	!    CPU->cpu_intr_thread = t->t_link
	!    t->t_lwp = curthread->t_lwp
	!    t->t_state = ONPROC_THREAD
	!    t->t_sp = sp
	!    t->t_intr = curthread
	!    curthread = t
	!    sp = t->t_stack
	!    t->t_pri = intr_pri + R3 (== ipl)

	mr	%r24,%r1		! save the thread stack pointer
	lwz 	%r4,CPU_INTR_THREAD(%r20)
	lwz 	%r6,T_LWP(THREAD_REG)
	lwz	%r5,T_LINK(%r4)
	stw 	%r6,T_LWP(%r4)
	li	%r7,ONPROC_THREAD
	stw 	%r5,CPU_INTR_THREAD(%r20) ! unlink thread from CPU's list

	!
	! Threads on the interrupt thread free list could have state already
	! set to TS_ONPROC, but it helps in debugging if they're TS_FREE
	! Could eliminate the next two instructions with a little work.
	!
	stw	%r7,T_STATE(%r4)
	!
	! chain the interrupted thread onto list from the interrupt thread.
	! Set the new interrupt thread as the current one.
	!
	sli	%r18,%r14,3		! vector*8 (for index into autovect below)
	lis	%r5,intr_pri@ha
	stw	%r1,T_SP(THREAD_REG)	! mark stack for resume
	stw	THREAD_REG,T_INTR(%r4)	! push old thread
	addi	%r18,%r18,AVH_LINK	! adjust autovect pointer by link offset
	lhz	%r6,intr_pri@l(%r5)	! XXX Can cause probs if new class is
					! loaded on some other cpu.
	mtsprg	0,%r4
	mr	%r26,%r3		! save IPL for possible use by intr_thread_exit
	stw	%r4,CPU_THREAD(%r20)	! set new thread
	add	%r6,%r6,%r3	 	! convert level to dispatch priority
	mr	THREAD_REG,%r4
	lwz	%r1,T_STACK(%r4)	! interrupt stack pointer
	stw	%r24,0(%r1)		! for use by kadb stack tracing code
	addis	%r18,%r18,autovect@ha
	stw	%r26,8(%r1)		! save IPL for possible use by intr_passivate
	!
	! Initialize thread priority level from intr_pri
	!
	sth	%r6,T_PRI(%r4)
	addi	%r18,%r18,autovect@l	! r18 is now the address of the list of handlers

	mtmsr	%r22			! sti (enable interrupts)
pre_loop2:
	lwz	%r15,0(%r18)		! autovect for 1st handler
	li	%r16,0			! r16 is no. of handlers in chain
	li	%r17,0			! r17 is DDI_INTR_CLAIMED status of chain
loop2:

 	cmpi  	%r15,0			! if pointer is null
 	beq-   	loop_done2		! then skip

 	lwz	%r0,AV_VECTOR(%r15)	! get the interrupt routine
 	lwz	%r3,AV_INTARG(%r15)	! get argument to interrupt routine
	lwz	%r4,AV_MUTEX(%r15)
	cmpi	%r0,0			! if func is null
	mtlr	%r0
	beq-	loop_done2		! then skip
	cmpi	%r4,0
	addi	%r16,%r16,1
	bne-	.unsafedriver2
	blrl				! call interrupt routine with arg
 	lwz	%r15,AV_LINK(%r15)	! get next routine on list
	or	%r17,%r17,%r3		! see if anyone claims intpt.
	b	loop2			! keep looping until end of list

.unsafedriver2:
	mr	%r25,%r0
	mr	%r3,%r4			! mutex
	bl	mutex_enter

	mtlr	%r25			! get the interrupt routine
 	lwz	%r3,AV_INTARG(%r15)	! get argument to interrupt routine
	blrl				! call interrupt routine with arg
	or	%r17,%r17,%r3		! see if anyone claims intpt.

	lwz	%r3,AV_MUTEX(%r15)
	bl	mutex_exit

 	lwz	%r15,AV_LINK(%r15)	! get next routine on list
	b	loop2			! keep looping until end of list
loop_done2:
	cmpi	%r16,1			! if only 1 intpt in chain, it is OK
	beq+	.loop_done2_1
	cmpi	%r17,0			! If no one claims intpt, then it is OK
	beq-	.loop_done2_1
	b	pre_loop2		! else try again.

.loop_done2_1:
	mtmsr	%r21			! cli
	lwz	%r4,CPU_SYSINFO_INTR(%r20)
	lwz	%r5,CPU_SYSINFO_INTRTHREAD(%r20)
	addi	%r4,%r4,1		! cpu_sysinfo.intr++
	addi	%r5,%r5,1		! cpu_sysinfo.intrthread++
	stw	%r4,CPU_SYSINFO_INTR(%r20)
	stw	%r5,CPU_SYSINFO_INTRTHREAD(%r20)

	! if there is still an interrupted thread underneath this one
	! then the interrupt was never blocked or released and the
	! return is fairly simple.  Otherwise jump to intr_thread_exit
	lwz 	%r0,T_INTR(THREAD_REG)
	cmpi 	%r0,0
	beq-	intr_thread_exit

	!
	! link the thread back onto the interrupt thread pool
	lwz 	%r4,CPU_INTR_THREAD(%r20)
	li	%r5,FREE_THREAD
	stw 	THREAD_REG,CPU_INTR_THREAD(%r20) ! link thread into CPU's list
	lwz	%r6,CPU_BASE_SPL(%r20)
	stw	%r4,T_LINK(THREAD_REG)
	stw	%r5,T_STATE(THREAD_REG)
	cmp	%r23,%r6			! if (oldipl >= basespl)

	! set the thread state to free so kadb doesn't see it
	stw	%r5,T_STATE(THREAD_REG)

	blt-	intr_restore_ipl
	mr	%r6,%r23
intr_restore_ipl:
	stw	%r6,CPU_PRI(%r20)	! restore old ipl
	lwz	THREAD_REG,T_INTR(THREAD_REG)	! new curthread
	lis	%r5,setlvlx@ha
	mr	%r4,%r14		! interrupt vector
	lwz	%r5,setlvlx@l(%r5)
	mr	%r3,%r6			! old ipl
	mtlr	%r5
	blrl				! call  *setlvlx
	mtsprg	0,THREAD_REG
	stw	THREAD_REG,CPU_THREAD(%r20)
	mr	%r1,%r24		! restore the previous stack pointer
 	b	dosoftint		! check for softints before we return.

	!
	! An interrupt returned on what was once (and still might be)
	! an interrupt thread stack, but the interrupted process is no longer
	! there.  This means the interrupt must've blocked or called
	! release_interrupt().
	!
	! There is no longer a thread under this one, so put this thread back
	! on the CPU's free list and resume the idle thread which will dispatch
	! the next thread to run.
	!
	! All interrupts are disabled here
	!

intr_thread_exit:
	!
	! Put thread back on either the interrupt thread list if it is
	! still an interrupt thread, or the CPU's free thread list, if it
	! did a release_interrupt
	! As a reminder, the regs at this point are
	!	%esi	interrupt thread
	lwz	%r4,CPU_SYSINFO_INTRBLK(%r20)
	lhz	%r5,T_FLAGS(THREAD_REG)
	addi	%r4,%r4,1
	andi.	%r5,%r5,T_INTR_THREAD
	stw	%r4,CPU_SYSINFO_INTRBLK(%r20)	! cpu_sysinfo.intrblk++
	beq-	rel_intr

	!
	! This was an interrupt thread, so clear the pending interrupt flag
	! for this level
	!
	li	%r7,1
	lwz	%r6,CPU_INTR_ACTV(%r20)	! fetch interrupt flag
	sl	%r7,%r7,%r26		! bit mask
	andc	%r6,%r6,%r7
	stw	%r6,CPU_INTR_ACTV(%r20)	! clear interrupt flag
	bl	set_base_spl		! set CPU's base SPL level

	lwz	%r3,CPU_BASE_SPL(%r20)	! arg1 = ipl
	lis	%r5,setlvlx@ha
	stw	%r3,CPU_PRI(%r20)
	lwz	%r5,setlvlx@l(%r5)
	mr	%r4,%r14		! arg2 = vector
	mtlr	%r5
	blrl				! call  *setlvlx
	bl	splhigh			! block all intrs below lock level
	!
	! Set the thread state to free so kadb doesn't see it
	!
	! Put thread on either the interrupt pool or the free pool and
	! call swtch() to resume another thread.
	!
	lwz 	%r4,CPU_INTR_THREAD(%r20)
	li	%r5,FREE_THREAD
	stw 	THREAD_REG,CPU_INTR_THREAD(%r20) ! link thread into CPU's list
	stw	%r4,T_LINK(THREAD_REG)
	stw	%r5,T_STATE(THREAD_REG)
	mtmsr	%r22			! sti (enable interrupts)
	bl 	swtch

	! swtch() shouldn't return

rel_intr:
	lwz	%r3,CPU_BASE_SPL(%r20)	! arg1 = ipl
	lis	%r5,setlvlx@ha
	stw	%r3,CPU_PRI(%r20)
	lwz	%r5,setlvlx@l(%r5)
	mr	%r4,%r14		! arg2 = vector
	mtlr	%r5
	blrl				! call  *setlvlx

	li	%r5,TS_ZOMB
	stw	%r5,T_STATE(THREAD_REG) ! set zombie so swtch will free
	bl 	swtch_from_zombie
#endif /* lint */

/*
 * Set Cpu's base SPL level, base on which interrupt levels are active
 *	Called at spl7 or above.
 */
#if defined(lint) || defined(__lint)

void
set_base_spl(void)
{}

#else   /* lint */

	ENTRY_NP(set_base_spl)
	lwz	%r3,T_CPU(THREAD_REG)
	lwz	%r4,CPU_INTR_ACTV(%r3)	! load active interrupts mask
	ori	%r4,%r4,1		! force range of "cntlzw" to exclude 32
	!
	! If clock is blocked, we want cpu_base_spl to be one less.
	!
	andi.	%r5,%r4,(1<<CLOCK_LEVEL)
	srwi	%r6,%r5,1		! mask for clock-level-1
	or	%r4,%r4,%r6		! OR in clock level into clock-level-1
	andc	%r4,%r4,%r5		! clear clock level

	cntlzw	%r5,%r4			! should return 16-31
					! which equate to spl 15-0
	subfic	%r5,%r5,31		! 31-r5
	stw	%r5,CPU_BASE_SPL(%r3)	! store base priority
	blr
	SET_SIZE(set_base_spl)

	.comm clk_intr,4,2

	.data
	.align	2

!
! XXXPPC *** HACKED CONSTANT FOR NOW ******
! We need to architect a correct method to initialize this value.
!
	.globl	dec_incr_per_tick
dec_incr_per_tick:			! number to add to decrementer
	.long	10000000		! for each interrupt (601 value)
!!!!	changed in startup.c if !601
!!!!	.long	100000			! for each interrupt (603 value)

	.globl	hrestime, hres_lock, timedelta
	.globl	hrestime_adj
	.align	3
hrestime:
	.long 0,0
	.size hrestime,8
hrestime_adj:
	.long 0,0
	.size hrestime_adj,8
timedelta:
	.long 0,0
	.size timedelta,8
hres_lock:
	.long 0
	.size hres_lock,4
_nsec_per_tick:
	.long 10000000
	.size _nsec_per_tick,4
nanosec:
	.long 1000000000
	.size nanosec,4

	.align	3
	.globl	tb_at_tick
tb_at_tick:
	.long	0, 0			! timebase value at last HZ clock tick
	.size tb_at_tick,8

#endif /* lint */

/*
 * byte offsets from hrestime for other variables, used for faster
 * address calculations.  Assumes LL_LSW/LL_MSW provide offsets
 * of least/most significant word offsets within a longlong.
 */
#define	HRESTIME_SEC	(hrestime     -hrestime)
#define	HRESTIME_NSEC	(hrestime+4   -hrestime)
#define	HRESTIME_ADJ	(hrestime_adj -hrestime)
#define	HRESTIME_ADJ_L	(HRESTIME_ADJ +LL_LSW)
#define	HRESTIME_ADJ_M	(HRESTIME_ADJ +LL_MSW)
#define	TIMEDELTA	(timedelta    -hrestime)
#define	TIMEDELTA_L	(TIMEDELTA    +LL_LSW)
#define	TIMEDELTA_M	(TIMEDELTA    +LL_MSW)
#define	HRES_LOCK	(hres_lock    -hrestime)+HRES_LOCK_OFFSET
#define	NSEC_PER_TICK	(_nsec_per_tick-hrestime)
#define	NANO_SEC	(nanosec      -hrestime)

#ifdef _LONG_LONG_LTOH
#define TB_AT_TICK_L	(tb_at_tick - hrestime)
#define TB_AT_TICK_H	(tb_at_tick + 4 - hrestime)
#else 
#define TB_AT_TICK_H	(tb_at_tick - hrestime)
#define TB_AT_TICK_L	(tb_at_tick + 4 - hrestime)
#endif

#if defined(lint) || defined(__lint)

void
init_dec_register(void)
{}

#else   /* lint */
	.text

	ENTRY(init_dec_register)
	lis	%r3,dec_incr_per_tick@ha
	lwz	%r3,dec_incr_per_tick@l(%r3)
	mtdec	%r3
	mfdec	%r3
	blr
	SET_SIZE(init_dec_register)

#endif /* lint */

/*
 * This code assumes that the real time clock interrupts 100 times per
 * second.
 *
 * clock() is called in a special thread called the clock_thread.
 * It sort-of looks like and acts like an interrupt thread, but doesn't
 * hold the spl while it's active.  If a clock interrupt occurs and
 * the clock_thread is already active or if we're running at LOCK_LEVEL
 * or higher, the clock_pend counter is incremented, causing clock()
 * to repeat for another tick.
 *
 * Interrupts are disabled upon entry.
 */

/*
 *	General register usage in interrupt loops is as follows:
 *
 *		r2  - curthread (always set up by level 1 code)
 *		r14 - clock_thread
 *		r15 - &clock_pend
 *		r16 - &clock_lock
 *		r17 - old curthread
 *		r18 -
 *		r19 -
 *		r20 - CPU
 *		r21 - msr (with interrupts disabled)
 *		r22 - msr (with interrupts enabled)
 *		r23 - old ipl
 *		r24 - stack pointer saved before changing to interrupt stack
 *		r25 - &hrestime
 *		r26 - &clk_intr
 *		r27 - clk_intr
 *		r28 - dec_incr_per_tick
 */

#if defined(lint) || defined(__lint)

void
clock_intr(void)
{}

#else   /* lint */

	ENTRY_NP(clock_intr)
#ifdef TRAPTRACE
	TRACE_PTR(%r5, %r4);		! get pointer into %r5 using %r4

	li	%r4, 0			! get trap type
	ori	%r4, %r4, 0xc10c	! try to spell clock
	stw	%r4, TRAP_ENT_TT(%r5)	! put trap type in trace
	lwz	%r4, CPU_SRR1(%r20)
	stw	%r4, TRAP_ENT_MSR(%r5)	! put old MSR in trace
	lwz	%r4, CPU_SRR0(%r20)
	stw	%r4, TRAP_ENT_PC(%r5)	! put old PC in trace
	lwz	%r4, REGS_R1(%r1)
	stw	%r4, TRAP_ENT_SP(%r5)	! put old SP in trace
	stw	THREAD_REG, TRAP_ENT_R2(%r5) ! put THREAD_REG in trace
	lwz	%r4, REGS_LR(%r1)
	stw	%r4, TRAP_ENT_X1(%r5)	! put old link register in trace

	TRACE_NEXT(%r5, %r4, %r6)	! update trace pointer
#endif /* TRAPTRACE */
					! 03789012345678912345678901lcx
					! .............................
	stw	%r0,REGS_R0(%r1)	! s............................
	li	%r0, 0
	mtspr	BAT3U, %r0		! BATU3 or IBATU3 invalidate
	mtspr	BAT3L, %r0		! BATL3 or IBATL3 invalidate
	stw	%r3,REGS_R3(%r1)	! .s...........................
	stw	%r7,REGS_R7(%r1)	! ..s..........................
	stw	%r8,REGS_R8(%r1)	! ...s.........................
	stw	%r9,REGS_R9(%r1)	! ....s........................
	stw	%r10,REGS_R10(%r1)	! .....s.......................
	stw	%r11,REGS_R11(%r1)	! ......s......................
	stw	%r12,REGS_R12(%r1)	! .......s.....................
	stw	%r13,REGS_R13(%r1)	! ........s....................
	stw	%r14,REGS_R14(%r1)	! .........s...................
	stw	%r15,REGS_R15(%r1)	! ..........s..................
	mfctr	%r9
	stw	%r16,REGS_R16(%r1)	! ...........s.................
	stw	%r17,REGS_R17(%r1)	! ............s................
	lis	%r14,clock_thread@ha
	stw	%r18,REGS_R18(%r1)	! .............s...............
	mfxer	%r10
	stw	%r19,REGS_R19(%r1)	! ..............s..............
	lwz	%r14,clock_thread@l(%r14)
	stw	%r21,REGS_R21(%r1)	! ...............s.............
	stw	%r22,REGS_R22(%r1)	! ................s............
	stw	%r23,REGS_R23(%r1)	! .................s...........
	stw	%r24,REGS_R24(%r1)	! ..................s..........
	stw	%r25,REGS_R25(%r1)	! ...................s.........
	stw	%r26,REGS_R26(%r1)	! ....................s........
	stw	%r27,REGS_R27(%r1)	! .....................s.......
	lis	%r26,clk_intr@ha
	stw	%r28,REGS_R28(%r1)	! ......................s......
	lwzu	%r27,clk_intr@l(%r26)
	stw	%r29,REGS_R29(%r1)	! .......................s.....
	lis	%r28,dec_incr_per_tick@ha
	stw	%r30,REGS_R30(%r1)	! ........................s....
	lwz	%r28,dec_incr_per_tick@l(%r28)
	stw	%r31,REGS_R31(%r1)	! .........................s...
	addi	%r27,%r27,1		! clk_intr++
	mfmsr	%r21			! msr - interrupts disabled
	stw	%r9,REGS_CTR(%r1)	! ...........................s.
	stw	%r10,REGS_XER(%r1)	! ............................s
	ori	%r22,%r21,MSR_EE	! msr - interrupts enabled
!
! adjust decrementer count
!
bump_dec:
	mfdec	%r3
	add.	%r3,%r3,%r28
	mtdec	%r3
	bge+	dec_ok
!
! When the new decrementer value is less than 0, we need to account
! for multiple clock ticks.  We do this by incrementing clock_pend
! while holding the clock_lock, and then go back to update the
! decrementer register accordingly.
!
	lis	%r16,clock_lock@ha
	lis	%r15,clock_pend@ha
	la	%r16,clock_lock@l(%r16)
	mr	%r3,%r16
	bl	lock_set
	lwzu	%r5,clock_pend@l(%r15)
	mr	%r3,%r16
	addi	%r5,%r5,1
	stw	%r5,0(%r15)
	bl	lock_clear
	b	bump_dec

dec_ok:
	lis	%r25,hrestime@ha
	la	%r25,hrestime@l(%r25)
	la	%r3,HRES_LOCK(%r25)
	bl	lock_set

!
! For non-601 machines, get time at tick for get_hrestime.
!
	mfpvr	%r5
	sri	%r5,%r5,16		! Version (1,3,4,20)
	cmpi	%r5,1
	beq-	2f
1:
	mftbu	%r8
	mftb	%r7
	mftbu	%r5
	cmpl	%r8, %r5
	bne-	1b
	stw	%r5,TB_AT_TICK_H(%r25)
	stw	%r7,TB_AT_TICK_L(%r25)
2:

!
! Apply adjustment, if any
!
!
! #define HRES_ADJ	(NSEC_PER_CLOCK_TICK >> ADJ_SHIFT)
!
! void
! adj_hrestime()
! {
! 	long long adj;
!
! 	if (hrestime_adj == 0)
! 		adj = 0;
! 	else {
! 		if (hrestime_adj > 0) {
! 			if (hrestime_adj < HRES_ADJ)
! 				adj = hrestime_adj;
! 			else
! 				adj = HRES_ADJ;
! 		} else {
! 			if (hrestime_adj < -(HRES_ADJ))
! 				adj = -(HRES_ADJ);
! 			else
! 				adj = hrestime_adj;
! 		}
! 		timedelta -= adj;
! 		hrestime.tv_nsec += adj;
! 	}
!
! 	hrestime_adj = timedelta;
! 	hrestime.tv_nsec += NSEC_PER_CLOCK_TICK;
!
! 	if (hrestime.tv_nsec >= NANOSEC) {
! 		one_sec = 1;
! 		hrestime.tv_sec++;
! 		hrestime.tv_nsec -= NANOSEC;
! 	}
! }

	lwz	%r7,HRESTIME_ADJ_L(%r25)
	lwz	%r8,HRESTIME_ADJ_M(%r25)
	lwz	%r3,TIMEDELTA_L(%r25)
	lwz	%r4,TIMEDELTA_M(%r25)
	lwz	%r5,NSEC_PER_TICK(%r25)
	or.	%r9,%r7,%r8
	lwz	%r11,HRESTIME_NSEC(%r25)
	lwz	%r12,HRESTIME_SEC(%r25)
	beq+	.adj_is_zero
	cmpi	%r8,0			! check sign of MSW of longlong
	sri	%r10,%r5,ADJ_SHIFT	! r10 is HRES_ADJ
	blt	.adj_less_than_0
	bne	.adj_big1
	cmp	%r7,%r10		! if (hrestime_adj < HRES_ADJ)
	bge	.adj_big1
	mr	%r9,%r7			! adj = hrestime_adj
	b	.adj_1
.adj_big1:
	mr	%r9,%r10		! adj = HRES_ADJ
.adj_1:
	subfc	%r3,%r9,%r3		! timedelta += -adj
	addme	%r4,%r4			! upper bits of -adj are 1s
	b	.adj_done

.adj_less_than_0:
	cmpi	%r8,-1			! check for large negative
	subfic	%r10,%r10,0		! -HRES_ADJ
	bne	.adj_big2
	cmp	%r7,%r10		! if (hrestime_adj < -HRES_ADJ)
	blt	.adj_big2
	mr	%r9,%r7
	b	.adj_2
.adj_big2:
	mr	%r9,%r10
.adj_2:
	subfc	%r3,%r9,%r3		! timedelta += -adj
	addze	%r4,%r4			! upper bits of -adj are 0s
.adj_done:
	! adj is in r9
	stw	%r3,TIMEDELTA_L(%r25)
	add	%r11,%r11,%r9			! nsec += adj
						! overflow handled below
	stw	%r4,TIMEDELTA_M(%r25)
.adj_is_zero:
	stw	%r3,HRESTIME_ADJ_L(%r25)	! hrestime_adj = timedelta
	lwz	%r6,NANO_SEC(%r25)
	stw	%r4,HRESTIME_ADJ_M(%r25)
	add	%r11,%r5,%r11			! nsec += NSEC_PER_CLOCK_TICK
	cmp	%r11,%r6
	blt	.not_one_sec			! if (nsec >= NANOSEC)
	li	%r0,1
	lis	%r3,one_sec@ha
	addi	%r12,%r12,1			! sec++
	subf	%r11,%r6,%r11			! nsec -= NANOSEC
	stw	%r0,one_sec@l(%r3)		! one_sec = 1
	stw	%r12,HRESTIME_SEC(%r25)		! store sec
.not_one_sec:
	stw	%r11,HRESTIME_NSEC(%r25)	! store nsec
	bl	unlock_hres_lock

	lwz	%r18,CPU_SYSINFO_INTR(%r20)
	stw	%r27,0(%r26)		! store clk_intr
	addi	%r18,%r18,1		! CPU->cpu_sysinfo_intr++

	stw	%r18,CPU_SYSINFO_INTR(%r20)
!
! If the clock interrupt interrupted LOCK_LEVEL (or higher priority) activity,
! just increment the clock_pend counter, and return from interrupt.
!
	lwz	%r5,CPU_PRI(%r20)
	cmpi	%r5,LOCK_LEVEL
	bge-	clock_done

	la	%r3,T_LOCK(%r14)	! &clock_thread->t_lock
	bl	lock_try
	cmpi	%r3,0
	beq-	clock_done		! exit if lock already set

	lwz	%r0,T_STATE(%r14)	! check to see if thread is blocked
	lwz	%r10,T_LWP(THREAD_REG)
	cmpi	%r0,FREE_THREAD
	bne-	rel_clk_lck

	!
	! consider the clock thread part of the same LWP as current thread
	!
	li	%r0,ONPROC_THREAD
	stw	%r10,T_LWP(%r14)
	stw	%r0,T_STATE(%r14)

	!
	! Push the interrupted thread onto list from the clock thread.
	! Set the clock thread as the current one.
	!
	stw	%r1,T_SP(THREAD_REG)	! set interrupted thread's stack pointer
	mr	%r24,%r1
	stw	THREAD_REG,T_INTR(%r14) ! link curthread onto clock_thread's list
	stw	%r20,T_CPU(%r14)	! set new thread's CPU ptr
	stw	%r20,T_BOUND_CPU(%r14)	! bind clock thread to CPU for base_spl
	stw	%r14,CPU_THREAD(%r20)	! set CPU's thread
	mtsprg	0,%r14			! set new curthread
	mr	%r17,THREAD_REG		! save old curthread
	la	%r5,CPU_THREAD_LOCK(%r20)
	li	%r0,CLOCK_LEVEL
	mr	THREAD_REG,%r14		! set curthread
	stw	%r5,T_LOCKP(%r14)	! set thread's disp lock to onproc thread lock
	lwz	%r1,T_STACK(%r14)	! change stacks
	lwz	%r23,CPU_PRI(%r20)	! get current ipl
	stw	%r24,0(%r1)		! for use by kadb stack tracing code
	stw	%r0,8(%r1)		! save ipl for intr_passivate
	stw	%r23,12(%r1)		! save oldipl for resume_from_intr

!
!	Initialize clock thread priority based on intr_pri
!
	lis	%r5,intr_pri@ha
	lhz	%r5,intr_pri@l(%r5)	! load intr_pri
! sparc and x86 use LOCK_LEVEL, but they're wrong.  (Check out the
! code in sun4c's machdep.c initializing the priority, and the
! initialization of intr_pri, if you don't believe me.)  CLOCK_LEVEL
! and LOCK_LEVEL are numerically the same, so it's only a theoretical
! difference.
	addi	%r5,%r5,CLOCK_LEVEL
	sth	%r5,T_PRI(%r14)		! Set priority of clock thread.

!
! We inherit the ipl level of the interrupted thread, and do not program
! the interrupt control registers.  We do reenable clock interrupts by
! resetting the machine state register.
!
! Disable the following instruction.  Interrupts cannot be enabled
! until after the check for "pinned interrupt" in clock().
!
!	mtmsr	%r22			! sti (enable interrupts)

	bl	clock			! void clock(void)
	mtmsr	%r21			! cli

	lwz	%r3,T_INTR(THREAD_REG)
	li	%r0,FREE_THREAD
	cmpi	%r3,0
	stw	%r0,T_STATE(THREAD_REG)
	beq-	clk_thread_exit		! interrupted thread has gone away
	li	%r8,0
	mr	THREAD_REG,%r17
	stw	%r17,CPU_THREAD(%r20)
	mtsprg	0,%r17
	la	%r3,T_LOCK(%r14)
	lwz	%r1,T_SP(%r17)
	bl	lock_clear

clock_intr_exit:
	lwz	%r23,CPU_PRI(%r20)	! get current ipl
	b	dosoftint		! normal clock exit

rel_clk_lck:
	la	%r3,T_LOCK(%r14)
	bl	lock_clear		! clear clock_thread->t_lock
clock_done:
!
! Since the clock thread is running, we just note that another
! clock tick has occurred.  We do this by incrementing clock_pend
! while holding the clock_lock, and then just return from interrupt.
!
	lis	%r16,clock_lock@ha
	lis	%r15,clock_pend@ha
	la	%r16,clock_lock@l(%r16)
	mr	%r3,%r16
	bl	lock_set
	lwzu	%r5,clock_pend@l(%r15)
	mr	%r3,%r16
	addi	%r5,%r5,1
	stw	%r5,0(%r15)
	bl	lock_clear
	b	clock_intr_exit

clk_thread_exit:
	lwz	%r3,CPU_SYSINFO_INTRBLK(%r20)
	addi	%r3,%r3,1
	stw	%r3,CPU_SYSINFO_INTRBLK(%r20)
	!
	! No pinned (interrupted) thread to return to,
	! so clear the pending interrupt flag for this level and call swtch
	!
	lwz	%r4,CPU_INTR_ACTV(%r20)
	andi.	%r4,%r4,(~(1<<CLOCK_LEVEL))&0xffff
	stw	%r4,CPU_INTR_ACTV(%r20)
	bl	set_base_spl
	mtmsr	%r22			! sti (enable interrupts)
	bl	swtch
	SET_SIZE(clock_intr)

#endif	/* lint */

/*
 *	Level 1 trap code
 */
#if defined(lint)

void
cmntrap(void)
{}

#else   /* lint */

	ENTRY_NP(cmntrap)
	lwz	%r5, CPU_DAR(%r20)
	lwz	%r6, CPU_DSISR(%r20)
#ifdef TRAPTRACE
	TRACE_PTR(%r3, %r4);		! get pointer into %r3 using %r4

	mflr	%r4			! get trap type
	subi	%r4, %r4, 4		! in case blrl was at end of l0 handler
	stw	%r4, TRAP_ENT_TT(%r3)	! put trap type in trace
	lwz	%r4, CPU_SRR1(%r20)
	stw	%r4, TRAP_ENT_MSR(%r3)	! put old MSR in trace
	lwz	%r4, CPU_SRR0(%r20)
	stw	%r4, TRAP_ENT_PC(%r3)	! put old PC in trace
	lwz	%r4, REGS_R1(%r1)
	stw	%r4, TRAP_ENT_SP(%r3)	! put old SP in trace
	stw	THREAD_REG, TRAP_ENT_R2(%r3)	! put THREAD_REG in trace
	lwz	%r4, REGS_LR(%r1)
	stw	%r4, TRAP_ENT_X1(%r3)	! put old link register in trace
	stw	%r5, TRAP_ENT_X2(%r3)	! put data address reg in trace
	stw	%r6, TRAP_ENT_X3(%r3)	! put fault info in trace
	TRACE_NEXT(%r3, %r4, %r5)	! update trace pointer
	lwz	%r3, CPU_R3(%r20)	! restore %r3
	lwz	%r4, REGS_R4(%r1)	! restore %r4
	lwz	%r5, CPU_DAR(%r20)	! restore %r5
#endif /* TRAPTRACE */
	mflr	%r4			! extract trap type from %lr
	subi	%r4, %r4, 4		! in case blrl was at end of l0 handler
	andi.	%r4, %r4, 0xff00
					! 03789012345678912345678901lcx
					! .............................
	stw	%r0,REGS_R0(%r1)	! s............................
	li	%r0, 0
	mtspr	BAT3U, %r0		! BATU3 or IBATU3 invalidate
	mtspr	BAT3L, %r0		! BATL3 or IBATL3 invalidate
	mfmsr	%r0
	ori	%r0, %r0, MSR_EE	! enable interrupts
	mtmsr	%r0
	stw	%r3,REGS_R3(%r1)	! .s...........................
	stw	%r7,REGS_R7(%r1)	! ..s..........................
	stw	%r8,REGS_R8(%r1)	! ...s.........................
	stw	%r9,REGS_R9(%r1)	! ....s........................
	stw	%r10,REGS_R10(%r1)	! .....s.......................
	stw	%r11,REGS_R11(%r1)	! ......s......................
	stw	%r12,REGS_R12(%r1)	! .......s.....................
	stw	%r13,REGS_R13(%r1)	! ........s....................
	stw	%r14,REGS_R14(%r1)	! .........s...................
	stw	%r15,REGS_R15(%r1)	! ..........s..................
	mfctr	%r9
	stw	%r16,REGS_R16(%r1)	! ...........s.................
	stw	%r17,REGS_R17(%r1)	! ............s................
	stw	%r18,REGS_R18(%r1)	! .............s...............
	mfxer	%r10
	stw	%r19,REGS_R19(%r1)	! ..............s..............
	stw	%r21,REGS_R21(%r1)	! ...............s.............
	stw	%r22,REGS_R22(%r1)	! ................s............
	stw	%r23,REGS_R23(%r1)	! .................s...........
	stw	%r24,REGS_R24(%r1)	! ..................s..........
	stw	%r25,REGS_R25(%r1)	! ...................s.........

	stw	%r26,REGS_R26(%r1)	! ....................s........
	stw	%r27,REGS_R27(%r1)	! .....................s.......
	stw	%r28,REGS_R28(%r1)	! ......................s......

	stw	%r29,REGS_R29(%r1)	! .......................s.....
	stw	%r30,REGS_R30(%r1)	! ........................s....
	stw	%r31,REGS_R31(%r1)	! .........................s...

	stw	%r9,REGS_CTR(%r1)	! ...........................s.
	stw	%r10,REGS_XER(%r1)	! ............................s

	li	%r10,0
	addi	%r3,%r1,MINFRAME	! init arg1, arg2-4 already set up
	stw	%r10,0(%r1)
.call_trap:
	bl	trap			! call trap(rp, type, dar, dsisr)
	b	_sys_rtt
	SET_SIZE(cmntrap)

#endif	/* lint */

/*
 *	call_kadb() - Invoke the debugger (called from debug_enter()).
 *
 *	trap_kadb() - Level 1 trap code for traps that kadb services
 */
#if defined(lint) || defined(__lint)

void
call_kadb(void)
{}

void
trap_kadb(void)
{}

#else   /* lint */

	ENTRY_NP2(call_kadb, int20)
	lis	%r7,_kadb_entry@ha
	lwz	%r0,_kadb_entry@l(%r7)
	cmpi	%r0,0
	beqlr				! return if kadb is not running

! Create a stack frame
	mr	%r0,%r1			! save r1
	subi	%r1,%r1,0x100
! Disable interrupts
	mfmsr	%r11
	rlwinm	%r12,%r11,0,17,15	! clear MSR_EE
	mtmsr	%r12

	stw	%r2,REGS_R2(%r1)
	stw	%r4,REGS_R4(%r1)
	stw	%r5,REGS_R5(%r1)
	stw	%r6,REGS_R6(%r1)
	stw	%r20,REGS_R20(%r1)
	mfcr	%r3
	stw	%r3,REGS_CR(%r1)	! pseudo cr
	mflr	%r3
	stw	%r3,REGS_PC(%r1)	! pseudo pc (caller of "call_kadb()")
	stw	%r11,REGS_MSR(%r1)	! pseudo msr
	stw	%r0,REGS_R1(%r1)	! saved r1 from above
	lis	%r4,0x1234		! pseudo trap type for "call_kadb()"
	ori	%r4,%r4,0x0adb		! pseudo trap type for "call_kadb()"
	lwz	%r3,REGS_R3(%r1)	! restore r3
	b	1f

	ALTENTRY(trap_kadb)
#ifdef TRAPTRACE
	TRACE_PTR(%r3, %r4);		! get pointer into %r3 using %r4
	mflr	%r4			! get trap type
	subi	%r4, %r4, 4		! in case blrl was at end of l0 handler
	stw	%r4, TRAP_ENT_TT(%r3)	! put trap type in trace
	lwz	%r4, CPU_SRR1(%r20)
	stw	%r4, TRAP_ENT_MSR(%r3)	! put old MSR in trace
	lwz	%r4, CPU_SRR0(%r20)
	stw	%r4, TRAP_ENT_PC(%r3)	! put old PC in trace
	lwz	%r4, REGS_R1(%r1)
	stw	%r4, TRAP_ENT_SP(%r3)	! put old SP in trace
	stw	THREAD_REG, TRAP_ENT_R2(%r3)	! put THREAD_REG in trace
	lwz	%r4, REGS_LR(%r1)
	stw	%r4, TRAP_ENT_X1(%r3)	! put old link register in trace
	TRACE_NEXT(%r3, %r4, %r5)	! update trace pointer
	lwz	%r3, CPU_R3(%r20)	! restore %r3
	lwz	%r4, REGS_R4(%r1)	! restore %r4
	lwz	%r5, REGS_R5(%r1)	! restore %r5
#endif /* TRAPTRACE */
	mflr	%r4			! get trap type from link register
	subi	%r4, %r4, 4
	andi.	%r4, %r4, 0xff00	! mask trap type
1:
					! 03789012345678912345678901lcx
					! .............................
	stw	%r0,REGS_R0(%r1)	! s............................
	li	%r0, 0
	mtspr	BAT3U, %r0		! BATU3 or IBATU3 invalidate
	mtspr	BAT3L, %r0		! BATL3 or IBATL3 invalidate
	stw	%r3,REGS_R3(%r1)	! .s...........................
	stw	%r7,REGS_R7(%r1)	! ..s..........................
	stw	%r8,REGS_R8(%r1)	! ...s.........................
	stw	%r9,REGS_R9(%r1)	! ....s........................
	stw	%r10,REGS_R10(%r1)	! .....s.......................
	stw	%r11,REGS_R11(%r1)	! ......s......................
	stw	%r12,REGS_R12(%r1)	! .......s.....................
	stw	%r13,REGS_R13(%r1)	! ........s....................
	stw	%r14,REGS_R14(%r1)	! .........s...................
	stw	%r15,REGS_R15(%r1)	! ..........s..................
	mfctr	%r9
	stw	%r16,REGS_R16(%r1)	! ...........s.................
	stw	%r17,REGS_R17(%r1)	! ............s................
	stw	%r18,REGS_R18(%r1)	! .............s...............
	mfxer	%r10
	stw	%r19,REGS_R19(%r1)	! ..............s..............
	stw	%r21,REGS_R21(%r1)	! ...............s.............
	stw	%r22,REGS_R22(%r1)	! ................s............
	stw	%r23,REGS_R23(%r1)	! .................s...........
	stw	%r24,REGS_R24(%r1)	! ..................s..........
	stw	%r25,REGS_R25(%r1)	! ...................s.........
	stw	%r26,REGS_R26(%r1)	! ....................s........
	stw	%r27,REGS_R27(%r1)	! .....................s.......
	stw	%r28,REGS_R28(%r1)	! ......................s......
	stw	%r29,REGS_R29(%r1)	! .......................s.....
	stw	%r30,REGS_R30(%r1)	! ........................s....
	stw	%r31,REGS_R31(%r1)	! .........................s...
	stw	%r9,REGS_CTR(%r1)	! ...........................s.
	stw	%r10,REGS_XER(%r1)	! ............................s

	lis	%r7,_kadb_entry@ha
	lwz	%r0,_kadb_entry@l(%r7)
	addi	%r3,%r1,MINFRAME	! pointer to struct regs
	mtlr	%r0
kadb_called_from_here_only:
	blrl				! call kadb
kadb_returns_here_always:
	cmpi	%r3,0			! 0 => just return to kernel
	beq	_kadb_rtn
					! 1 => kernel should service trap
	mfmsr	%r21
	ori	%r22,%r21,MSR_EE
	mtmsr	%r22			! enable interrupts
	addi	%r3,%r1,MINFRAME	! init arg1, arg2-4 already set up
	b	.call_trap		! enter trap() from only 1 place
					! for better kadb stack traces

	ALTENTRY(_kadb_rtn)
/*
 * Entrypoint "_kadb_rtn" is used by KADB when it is done processing, and
 * is continuing the kernel (e.g., ":c").  The only assumptions are that R1
 * has a valid stack pointer value and interrupts are disabled.
 */

/*
 * After stopping in kadb, the DECrementer register has an arbitrary,
 * meaningless value.  There is often a noticeable delay of normal
 * processing that occurs (prior to this change).  One sane thing
 * that we can do about this is to set the decrementer to a value
 * that will make it appear as if only 1 tick has occurred.  This
 * provides similar behavior to x86, but I don't know about SPARC.
 */
	li	%r3, -128	! one decrementer tick after
				! and interrupt (on 601, anyway)
	mtdec	%r3
	b	set_user_regs

	SET_SIZE(call_kadb)

#endif	/* lint */

/*
 *	Level 1 syscall entry
 *
 *	Entered with interrupts disabled.
 *
 *	Non-volatile register usage -
 *
 *		r2  - curthread
 *		r14 - ttolwp(curthread)
 *		r21 - MSR for interrupts disabled
 *		r22 - MSR for interrupts enabled
 */
#if defined(lint) || defined(__lint)

void
sys_call(void)
{}

#else   /* lint */

	ENTRY_NP(sys_call)
#ifdef TRAPTRACE
	TRACE_PTR(%r3, %r4);		! get pointer into %r3 using %r4
	mflr	%r4			! get trap type
	subi	%r4, %r4, 4		! in case blrl was at end of l0 handler
	stw	%r4, TRAP_ENT_TT(%r3)	! put trap type in trace
	lwz	%r4, CPU_SRR1(%r20)
	stw	%r4, TRAP_ENT_MSR(%r3)	! put old MSR in trace
	lwz	%r4, CPU_SRR0(%r20)
	stw	%r4, TRAP_ENT_PC(%r3)	! put old PC in trace
	lwz	%r4, REGS_R1(%r1)
	stw	%r4, TRAP_ENT_SP(%r3)	! put old SP in trace
	stw	THREAD_REG, TRAP_ENT_R2(%r3)	! put THREAD_REG in trace
	lwz	%r4, REGS_LR(%r1)
	stw	%r4, TRAP_ENT_X1(%r3)	! put old link register in trace
	stw	%r0, TRAP_ENT_X2(%r3)	! put syscall number in trace
	lwz	%r4, CPU_R3(%r20)
	stw	%r4, TRAP_ENT_X3(%r3)	! put first arg into trace
	TRACE_NEXT(%r3, %r4, %r5)	! update trace pointer
	lwz	%r3, CPU_R3(%r20)	! restore %r3
	lwz	%r4, REGS_R4(%r1)	! restore %r4
	lwz	%r5, REGS_R5(%r1)	! restore %r5
#endif /* TRAPTRACE */

	li	%r4, 0
	mtspr	BAT3U, %r4		! BATU3 or IBATU3 invalidate
	mtspr	BAT3L, %r4		! BATL3 or IBATL3 invalidate
					! 03789012345678912345678901lcx
					! .............................
	stw	%r0,REGS_R0(%r1)	! s............................
	stw	%r3,REGS_R3(%r1)	! .s...........................
	stw	%r4,0(%r1)		! guarantee stack trace termination
	stw	%r7,REGS_R7(%r1)	! ..s..........................
	stw	%r8,REGS_R8(%r1)	! ...s.........................
	stw	%r9,REGS_R9(%r1)	! ....s........................
	stw	%r10,REGS_R10(%r1)	! .....s.......................
	stw	%r11,REGS_R11(%r1)	! ......s......................
	stw	%r12,REGS_R12(%r1)	! .......s.....................
	stw	%r13,REGS_R13(%r1)	! ........s....................
	stw	%r14,REGS_R14(%r1)	! .........s...................
	stw	%r15,REGS_R15(%r1)	! ..........s..................
	mfctr	%r4
	stw	%r16,REGS_R16(%r1)	! ...........s.................
	stw	%r17,REGS_R17(%r1)	! ............s................
	stw	%r18,REGS_R18(%r1)	! .............s...............
	mfxer	%r5
	stw	%r19,REGS_R19(%r1)	! ..............s..............
	stw	%r21,REGS_R21(%r1)	! ...............s.............
	stw	%r22,REGS_R22(%r1)	! ................s............
	stw	%r23,REGS_R23(%r1)	! .................s...........
	stw	%r24,REGS_R24(%r1)	! ..................s..........
	stw	%r25,REGS_R25(%r1)	! ...................s.........
	mfmsr	%r21			! msr - interrupts disabled
	stw	%r26,REGS_R26(%r1)	! ....................s........
	stw	%r27,REGS_R27(%r1)	! .....................s.......
	stw	%r28,REGS_R28(%r1)	! ......................s......
	ori	%r22,%r21,MSR_EE	! msr - interrupts enabled
	stw	%r29,REGS_R29(%r1)	! .......................s.....
	stw	%r30,REGS_R30(%r1)	! ........................s....
	stw	%r31,REGS_R31(%r1)	! .........................s...

	lwz	%r14,T_LWP(THREAD_REG)
	li	%r15,LWP_SYS
	stw	%r4,REGS_CTR(%r1)	! ...........................s.
	stw	%r5,REGS_XER(%r1)	! ............................s

        stb	%r15,LWP_STATE(%r14)		! set lwp state to SYSTEM

	mtmsr	%r22				! enable interrupts

	!
	! Restore some registers.  It'd be better if these hadn't been
	! disturbed by .L0_entry.  XXX - Fix that.
	!
	lwz	%r4, REGS_R4(%r1)
	lwz	%r5, REGS_R5(%r1)
	lwz	%r6, REGS_R6(%r1)

	!
	! At this point, the system call args must still be in %r3-%r10
	! and the non-volatile user registers are unchanged.  This way,
	! if no pre-syscall or post-syscall handling is needed, we can
	! call the handler directly and return without reloading most of the
	! non-volatile registers.
	!
	! The system call number should be still in %r0, also.
	!
	! We use some non-volatiles as follows:
	!	%r14 = lwp pointer
	!	%r15 = scratch
	!	%r16 = scratch
	!	%r20 = cpu structure
	!	%r21 = disabling %msr
	!	%r22 = enabling %msr

	!
	! Test for pre-system-call handling
	!
	lwz	%r15, CPU_SYSINFO_SYSCALL(%r20)	! update CPU syscall statistics
	lwz	%r16, LWP_RU_SYSC(%r14)		! update LWP syscall statistics
	addi	%r15, %r15, 1
	addi	%r16, %r16, 1
	stw	%r15, CPU_SYSINFO_SYSCALL(%r20)
	lbz	%r15, T_PRE_SYS(THREAD_REG)	! pre-syscall?
	sth	%r0, T_SYSNUM(THREAD_REG)	! save system call number
	stw	%r16, LWP_RU_SYSC(%r14)
#ifdef SYSCALLTRACE
	lis	%r16, syscalltrace@ha
	lwz	%r16, syscalltrace@l(%r16)
	or.	%r15, %r15, %r16
#else
	cmpi	%r15, 0
#endif
	bne-	_syscall_pre			! do pre-syscall checks

	!
	! Call the handler.  The args are still in %r3-%r10.
	!
.syscall_pre_done:
	cmpli	%r0, NSYSCALL			! check for illegal syscall
	bge-	_syscall_ill			! illegal system call
	lis	%r15, sysent@ha
	la	%r15, sysent@l(%r15)
	la	%r15, SY_CALLC(%r15)
#if SYSENT_SIZE == 16
	sli	%r16, %r0, 4			! index = 16 * syscall number
#else
	.error	"sysent size change requires change in shift amount"
#endif
	lwzx	%r15, %r15, %r16		! load handler address
	mtlr	%r15
	blrl					! call handler

	!
	! normal return from system call.
	!
	mtmsr	%r21				! disable interrupts
	lwz	%r15, T_POST_SYS_AST(THREAD_REG) ! includes T_ASTFLAG
	cmpi	%r15, 0				! post-sys handling needed?
	bne-	_syscall_post			! yes, post_sys, signal, or AST
#ifdef TRAPTRACE
	stw	%r3, REGS_R3(%r1)	! save %r3, %r4
	stw	%r4, REGS_R4(%r1)
	TRACE_PTR(%r3, %r4);		! get pointer into %r3 using %r4
	li	%r4, TT_SC_RET		! normal return type
	stw	%r4, TRAP_ENT_TT(%r3)	! put trap type in trace
	lwz	%r4, REGS_MSR(%r1)
	stw	%r4, TRAP_ENT_MSR(%r3)	! put MSR in trace
	lwz	%r4, REGS_PC(%r1)
	stw	%r4, TRAP_ENT_PC(%r3)	! put old PC in trace
	lwz	%r4, REGS_R1(%r1)
	stw	%r4, TRAP_ENT_SP(%r3)	! put old SP in trace
	stw	THREAD_REG, TRAP_ENT_R2(%r3)	! put THREAD_REG in trace
	lwz	%r4, REGS_CR(%r1)
	stw	%r4, TRAP_ENT_X1(%r3)	! put condition register in trace
	lwz	%r4, REGS_R3(%r1)
	stw	%r4, TRAP_ENT_X2(%r3)	! put first rval into trace
	lwz	%r4, REGS_R4(%r1)
	stw	%r4, TRAP_ENT_X3(%r3)	! put second rval into trace
	TRACE_NEXT(%r3, %r4, %r5)	! update trace pointer
	lwz	%r3, REGS_R3(%r1)	! restore %r3
	lwz	%r4, REGS_R4(%r1)	! restore %r4
	lwz	%r5, REGS_R5(%r1)	! restore %r5
#endif /* TRAPTRACE */

	stwcx.	THREAD_REG,0,%r1	! guarantee no "dangling reservation"
	li	%r15, LWP_USER
	lis	%r16, 0
	stb	%r15, LWP_STATE(%r14)		! store LWP state
	sth	%r16, T_SYSNUM(THREAD_REG)	! clear system call number
.patch_batu3:
	li	%r15, IBATU_LOMEM_MAPPING	! 603, 604, 620 IBAT 1-1 mapping for 8M
.patch_batl3:
	li	%r16, IBATL_LOMEM_MAPPING
	mtspr	BAT3U, %r15		! BATU3 or IBATU3
	mtspr	BAT3L, %r16		! BATL3 or IBATL3
	isync
	ba	SYSCALL_RTN_PADDR	! branch absolute to low mem

_syscall_pre:
	bl	pre_syscall		! abort_flg = pre_syscall(args);
	cmpi	%r3, 0			! did it abort?
	bne-	_syscall_post		! yes - do post_syscall();
	lwz	%r0, REGS_R0(%r1)	! reload system call number
	lwz	%r3, REGS_R3(%r1)	! reload arguments
	lwz	%r4, REGS_R4(%r1)
	lwz	%r5, REGS_R5(%r1)
	lwz	%r6, REGS_R6(%r1)
	lwz	%r7, REGS_R7(%r1)
	lwz	%r8, REGS_R8(%r1)
	lwz	%r9, REGS_R9(%r1)
	lwz	%r10, REGS_R10(%r1)
	b	.syscall_pre_done	! prepare to call the handler

	!
	! illegal system call - syscall number out of range
	!
_syscall_ill:
	bl	nosys
	!
	! Fall through
	!

	!
	! Post-system-call special processing needed.
	! Here with interrupts disabled, and enabling %msr in %r22
	!
_syscall_post:
	mtmsr	%r22			! enable interrupts
	bl	post_syscall		! call post_syscall(rvals);
#ifdef TRAPTRACE
	mtmsr	%r21			! disable interrupts
	TRACE_PTR(%r3, %r4);		! get pointer into %r3 using %r4
	li	%r4, TT_SC_POST		! syscall return (post) trap type
	stw	%r4, TRAP_ENT_TT(%r3)	! put trap type in trace
	lwz	%r4, REGS_MSR(%r1)
	stw	%r4, TRAP_ENT_MSR(%r3)	! put MSR in trace
	lwz	%r4, REGS_PC(%r1)
	stw	%r4, TRAP_ENT_PC(%r3)	! put PC in trace
	lwz	%r4, REGS_R1(%r1)
	stw	%r4, TRAP_ENT_SP(%r3)	! put SP in trace
	stw	THREAD_REG, TRAP_ENT_R2(%r3) ! put THREAD_REG in trace
	lwz	%r4, REGS_CR(%r1)
	stw	%r4, TRAP_ENT_X1(%r3)	! put condition register in trace
	lwz	%r4, REGS_R3(%r1)
	stw	%r4, TRAP_ENT_X2(%r3)	! put syscall rval1 in trace
	lwz	%r4, REGS_R4(%r1)
	stw	%r4, TRAP_ENT_X3(%r3)	! put syscall rval2 in trace
	TRACE_NEXT(%r3, %r4, %r5)	! update trace pointer
#endif /* TRAPTRACE */
	b	_sys_rtt_syscall	! normal trap return
	SET_SIZE(sys_call)

#endif	/* lint */

/*
 *	Return from trap
 */
#if defined(lint) || defined(__lint)

/*
 * This is a common entry point used for:
 *
 *	1. icode() calls here when proc 1 is ready to go to user mode.
 *	2. forklwp() makes this the start of all new user processes.
 *	3. syslwp_create() makes this the start of all new user lwps.
 *
 * It is assumed that this entry point is entered with interrupts enabled
 * and after initializing the stack pointer with curthread->t_stack, the
 * current thread is headed for user mode, so there is no need to check
 * for "return to kernel mode" vs. "return to user mode".
 */

void
lwp_rtt(void)
{}

#else   /* lint */

	ENTRY_NP(lwp_rtt)
	lwz	%r1,T_STACK(THREAD_REG)
	lwz	%r3,REGS_R3(%r1)
	lwz	%r4,REGS_R4(%r1)
	bl	post_syscall	! post_syscall(rval1, rval2)

	/*
	 * Return to user.
	 */

	ALTENTRY(_sys_rtt_syscall)
	mfmsr	%r5
	rlwinm	%r6,%r5,0,17,15	! clear MSR_EE
	mtmsr	%r6		! disable interrupts
	lbz	%r8,T_ASTFLAG(THREAD_REG)
	cmpli	%r8,0
	beq+	set_user_regs
	ori	%r5,%r6,MSR_EE
	mtmsr	%r5		! enable interrupts
	li	%r4,T_AST
	addi	%r3,%r1,MINFRAME	! init arg1
	bl	trap			! AST trap
	b	_sys_rtt_syscall

#endif	/* lint */

/*
 * This is a common entry point used:
 *
 *	1. for interrupted threads that are being started after their
 *	   corresponding interrupt thread has blocked.  Note that
 *	   the interrupted thread could be either a kernel thread or
 *	   a user thread.  This entry is set up by intr_passivate().
 *	2. after all traps (user or kernel mode) to return from interrupt.
 *	3. after check for soft interrupts, when there are none to service.
 *	4. after spurious interrupts.
 */

#if defined(lint) || defined(__lint)

void
_sys_rtt(void)
{}

#else   /* lint */

	.data
	.globl	ppc_spurious
ppc_spurious:
	.word	0

	.text
	ALTENTRY(_sys_rtt_spurious)
	lis	%r3,ppc_spurious@ha	! for the heck of it, keep a
	lwzu	%r5,ppc_spurious@l(%r3)	! running count of spurious
	addi	%r5,%r5,1		! interrupts
	stw	%r5,0(%r3)

	ALTENTRY(_sys_rtt)
	lwz	%r5,REGS_MSR(%r1)
	andi.	%r5,%r5,MSR_PR	! going back to kernel or user mode?
	bne+	_sys_rtt_syscall

	! return to supervisor mode
sr_sup:
	! Check for kernel preemption
	lwz	%r20,T_CPU(THREAD_REG)	! CPU
	lbz	%r0,CPU_KPRUNRUN(%r20)
	cmpli	%r0,0
	beq-	set_sup_regs
	li	%r3,1
	bl	kpreempt		! kpreempt(1)
set_sup_regs:
	mfmsr	%r5
	rlwinm	%r6,%r5,0,17,15		! clear MSR_EE
	mtmsr	%r6			! disable interrupts
!
!	restore special purpose registers - srr0, srr1, lr, cr, ctr, xer
!	    (stagger loads and stores for "hopefully" better performance)
!
!	assumes interrupts are disabled
!
set_user_regs:
	stwcx.	THREAD_REG,0,%r1	! guarantee no "dangling reservation"

/*
 * Actual return code is at TRAP_RTN_PADDR phys addr in low memory.
 * Therefore we need temp BAT mapping for low memory.
 */
.patch_batu4:
	li	%r4, IBATU_LOMEM_MAPPING	! 603, 604, 620 IBAT 1-1 mapping for 8M
.patch_batl4:
	li	%r5, IBATL_LOMEM_MAPPING
	mtspr	BAT3U, %r4		! BATU3 or IBATU3
	mtspr	BAT3L, %r5		! BATL3 or IBATL3
	isync

	ba	TRAP_RTN_PADDR	! branch absolute to low mem

	SET_SIZE(lwp_rtt)

#endif	/* lint */


/*
 * int
 * intr_passivate(from, to)
 *      kthread_id_t     from;           interrupt thread
 *      kthread_id_t     to;             interrupted thread
 *
 *      intr_passivate(t, itp) makes the interrupted thread "t" runnable.
 *
 *      Since t->t_sp has already been saved, t->t_pc is all that needs
 *      set in this function.  The SPARC register window architecture
 *      greatly complicates this.  We have stashed the IPL return value
 *	away in the register save area of the interrupted thread.
 *
 *      Returns interrupt level of the thread.
 */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
int
intr_passivate(kthread_id_t from, kthread_id_t to)
{ return (0); }

#else   /* lint */

	ENTRY(intr_passivate)

	lis	%r6,_sys_rtt@ha
	lwz	%r5,T_STACK(%r3)	! where the stack began
	la	%r6,_sys_rtt@l(%r6)
	lwz	%r3,8(%r5)		! where the IPL was saved
	stw	%r6,T_PC(%r4)		! set interrupted thread's resume pc
	blr
	SET_SIZE(intr_passivate)

#endif	/* lint */

/*
 * Return a thread's interrupt level.
 * This isn't saved anywhere but on the interrupt stack at interrupt
 * entry at the bottom of interrupt stack.
 *
 * Caller 'swears' that this really is an interrupt thread.
 *
 * int
 * intr_level(t)
 *      kthread_id_t    t;
 */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
int
intr_level(kthread_id_t t)
{ return (0); }

#else   /* lint */

	ENTRY(intr_level)
	lwz	%r5,T_STACK(%r3)	! where the stack began
	lwz	%r3,0(%r5)		! where the IPL was saved
	blr
	SET_SIZE(intr_level)

/*
 * dosoftint(old_pil in %eax)
 * Process software interrupts
 * Interrupts are disabled here.
 */

/*
 * Handle an interrupt in a new thread.
 *	Entry:  traps disabled.
 *		r1	- pointer to regs
 *		r2	- curthread
 *		r20	- CPU
 *		r21	- msr for interrupts disabled
 *		r22	- msr for interrupts enabled
 *		r23	- old spl
 *		r25	- save for stashing unsafe entry point
 *		r26	- saved interrupt level for this interrupt
 */
/*
 *	General register usage in interrupt loops is as follows:
 *
 *		r2  - curthread (always set up by level 1 code)
 *		r14 - interrupt vector
 *		r15 - pointer to softvect structure for handler
 *		r16 - number of handlers in chain
 *		r17 - is DDI_INTR_CLAIMED status of chain
 *		r18 - softvect pointer
 *		r19 - cpu_on_intr
 *		r20 - CPU
 *		r21 - msr (with interrupts disabled)
 *		r22 - msr (with interrupts enabled)
 *		r23 - old ipl
 *		r24 - stack pointer saved before changing to interrupt stack
 */

pre_dosoftint:
!	NOTE that we don't care if the following store fails.
	stwcx.	%r6,%r0,%r5		! cancel reservation

!!!XXXPPC - need to verify that %r23 has IPL to go to when done with
!!!	 doing softints.  Can softints be processed on multiple CPUs
!!!	 at the same time?

dosoftint:
	lis	%r5,softinfo@ha
	lwzu	%r0,softinfo@l(%r5)
	cmpi	%r0,0
	beq+	_sys_rtt
	cntlzw	%r4,%r0		! should return 16-31
	subfic	%r3,%r4,31	! 31-r4 gives 15-0
	cmp	%r23,%r3	! if curipl >= pri
	mr	%r26,%r3	! save IPL for possible use by intr_thread_exit
	bge+	_sys_rtt

	li	%r7,1
	lwarx	%r6,%r0,%r5	! must clear bit atomically
	sl	%r7,%r7,%r3
	cmp	%r0,%r6
	bne-	pre_dosoftint	! start over if bit changed
	xor	%r6,%r6,%r7	! clear bit
	stwcx.	%r6,%r0,%r5	! store it back
	isync
	bne-	dosoftint	! if failed, try again

	mr	%r3,%r26	! IPL to go to
	stw	%r26,CPU_PRI(%r20)
	lis	%r4,setspl@ha
	lwz	%r4,setspl@l(%r4)
	mtlr	%r4
	blrl				! call setspl(new ipl)

	! Get set to run interrupt thread.
	! There should always be an interrupt thread since we allocate one
	! for each level on the CPU, and if we release an interrupt, a new
	! thread gets created.
	!
	!    t = CPU->cpu_intr_thread
	!    CPU->cpu_intr_thread = t->t_link
	!    t->t_lwp = curthread->t_lwp
	!    t->t_state = ONPROC_THREAD
	!    t->t_sp = sp
	!    t->t_intr = curthread
	!    curthread = t
	!    sp = t->t_stack
	!    t->t_pri = intr_pri + R3 (== ipl)

	lwz 	%r4,CPU_INTR_THREAD(%r20)
	lwz 	%r6,T_LWP(THREAD_REG)
	lwz	%r5,T_LINK(%r4)
	stw 	%r6,T_LWP(%r4)
	li	%r7,ONPROC_THREAD
	stw 	%r5,CPU_INTR_THREAD(%r20) ! unlink thread from CPU's list

	!
	! Threads on the interrupt thread free list could have state already
	! set to TS_ONPROC, but it helps in debugging if they're TS_FREE
	! Could eliminate the next two instructions with a little work.
	!
	stw	%r7,T_STATE(%r4)
	!
	! chain the interrupted thread onto list from the interrupt thread.
	! Set the new interrupt thread as the current one.
	!
	sli	%r18,%r26,3		! level*8 (for index into softvect below)
	lis	%r5,intr_pri@ha
	stw	%r1,T_SP(THREAD_REG)	! mark stack for resume
	stw	THREAD_REG,T_INTR(%r4)	! push old thread
	addi	%r18,%r18,AVH_LINK	! adjust softvect pointer by link offset
	lhz	%r6,intr_pri@l(%r5)	! XXX Can cause probs if new class is
					! loaded on some other cpu.
	mtsprg	0,%r4
	stw	%r4,CPU_THREAD(%r20)	! set new thread
	add	%r6,%r6,%r26	 	! convert level to dispatch priority
	mr	THREAD_REG,%r4
	lwz	%r1,T_STACK(%r4)	! interrupt stack pointer
	addis	%r18,%r18,softvect@ha
	stw	%r26,8(%r1)		! save IPL for possible use by intr_passivate
	!
	! Initialize thread priority level from intr_pri
	!
	sth	%r6,T_PRI(%r4)
	addi	%r18,%r18,softvect@l	! r18 is now the address of the list of handlers

	mtmsr	%r22			! sti (enable interrupts)
pre_loop3:
	lwz	%r15,0(%r18)		! softvect for 1st handler
loop3:
 	cmpi  	%r15,0			! if pointer is null
 	beq-   	loop_done3		! then skip

 	lwz	%r0,AV_VECTOR(%r15)	! get the interrupt routine
 	lwz	%r3,AV_INTARG(%r15)	! get argument to interrupt routine
	lwz	%r4,AV_MUTEX(%r15)
	cmpi	%r0,0			! if func is null
	mtlr	%r0
	beq-	loop_done3		! then skip
	cmpi	%r4,0
	addi	%r16,%r16,1
	bne-	.unsafedriver3
	blrl				! call interrupt routine with arg
 	lwz	%r15,AV_LINK(%r15)	! get next routine on list
	b	loop3			! keep looping until end of list

.unsafedriver3:
	mr	%r25,%r0
	mr	%r3,%r4			! mutex
	bl	mutex_enter

	mtlr	%r25			! get the interrupt routine
 	lwz	%r3,AV_INTARG(%r15)	! get argument to interrupt routine
	blrl				! call interrupt routine with arg

	lwz	%r3,AV_MUTEX(%r15)
	bl	mutex_exit

 	lwz	%r15,AV_LINK(%r15)	! get next routine on list
	b	loop3			! keep looping until end of list
loop_done3:
	mtmsr	%r21			! cli
	lwz	%r3,CPU_SYSINFO_INTR(%r20)
	addi	%r3,%r3,1		! cpu_sysinfo.intr++
	stw	%r3,CPU_SYSINFO_INTR(%r20)

	! if there is still an interrupted thread underneath this one
	! then the interrupt was never blocked or released and the
	! return is fairly simple.  Otherwise jump to intr_thread_exit
	lwz 	%r0,T_INTR(THREAD_REG)
	cmpi 	%r0,0
	beq-	softintr_thread_exit

	!
	! link the thread back onto the interrupt thread pool
	lwz 	%r4,CPU_INTR_THREAD(%r20)
	li	%r5,FREE_THREAD
	stw 	THREAD_REG,CPU_INTR_THREAD(%r20) ! link thread into CPU's list
	lwz	%r6,CPU_BASE_SPL(%r20)
	stw	%r4,T_LINK(THREAD_REG)
	stw	%r5,T_STATE(THREAD_REG)
	cmp	%r23,%r6		! if (oldipl >= basespl)

	! set the thread state to free so kadb doesn't see it
	stw	%r5,T_STATE(THREAD_REG)

	blt-	softintr_restore_ipl
	mr	%r6,%r23
softintr_restore_ipl:
	stw	%r6,CPU_PRI(%r20)	! restore old ipl
	lwz	THREAD_REG,T_INTR(THREAD_REG)	! new curthread
	mr	%r3,%r6			! old ipl
	lis	%r4,setspl@ha
	lwz	%r4,setspl@l(%r4)
	mtlr	%r4
	blrl				! call setspl(new ipl)
	mtsprg	0,THREAD_REG
	stw	THREAD_REG,CPU_THREAD(%r20)
	lwz	%r1,T_SP(THREAD_REG)	! restore the previous stack pointer
 	b	dosoftint		! check for softints before we return.

softintr_thread_exit:
	!
	! Put thread back on either the interrupt thread list if it is
	! still an interrupt thread, or the CPU's free thread list, if it
	! did a release_interrupt
	! As a reminder, the regs at this point are
	!	%esi	interrupt thread
	lhz	%r5,T_FLAGS(THREAD_REG)
	andi.	%r5,%r5,T_INTR_THREAD
	beq-	softrel_intr

	!
	! This was an interrupt thread, so clear the pending interrupt flag
	! for this level
	!
	li	%r7,1
	lwz	%r6,CPU_INTR_ACTV(%r20)	! fetch interrupt flag
	sl	%r7,%r7,%r26		! bit mask
	andc	%r6,%r6,%r7
	stw	%r6,CPU_INTR_ACTV(%r20)	! clear interrupt flag
	bl	set_base_spl		! set CPU's base SPL level

	!
	! Set the thread state to free so kadb doesn't see it
	!
	! Put thread on either the interrupt pool or the free pool and
	! call swtch() to resume another thread.
	!
	lwz 	%r4,CPU_INTR_THREAD(%r20)
	li	%r5,FREE_THREAD
	stw 	THREAD_REG,CPU_INTR_THREAD(%r20) ! link thread into CPU's list
	stw	%r4,T_LINK(THREAD_REG)
	stw	%r5,T_STATE(THREAD_REG)
	bl	splhigh			! block all intrs below lock level
	mtmsr	%r22			! sti (enable interrupts)
	bl 	swtch

	! swtch() shouldn't return

softrel_intr:
	li	%r5,TS_ZOMB
	stw	%r5,T_STATE(THREAD_REG)	! set zombie so swtch will free
	bl 	swtch_from_zombie


#endif	/* lint */

/*
 * The following code is used to generate a 10 microsecond delay
 * routine.  It is initialized in pit.c.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
tenmicrosec(void)
{}

#else	/* lint */

	.globl	microdata
	ENTRY(tenmicrosec)
	sync			! force out all stores to memory
	mfpvr	%r5
	sri	%r5,%r5,16	! Version (1,3,4,20)
	cmpi	%r5,1
	bne-	.tenmicrosec_not601
	mfspr	%r3,5		! RTC lower
	addi	%r4,%r3,10000	! 10000 nsec per 10 usec
.microloop:
	mfspr	%r5,5
	cmpl	%r5,%r4
	bgelr			! return if limit exceeded
	cmpl	%r3,%r5
	ble	.microloop
!
! At this point, we think the RTCL has wrapped, so we should
! adjust our "limit" accordingly, i.e., reduce it to 10 usec.
!
	li	%r3,0
	li	%r4,10000
	b	.microloop
!
! 	Assume 10MHZ update frequency on the timebase. So, the number
!	of nanoseconds per tick of the timebase is 100 and 100 ticks
!	is 10 micro seconds.
!
.tenmicrosec_not601:
1:
	lis	%r9,tbticks_per_10usec@ha
	lwz	%r9,tbticks_per_10usec@l(%r9)
	mfspr	%r3,285
	mfspr	%r4,284
	mfspr	%r5,285
	cmpw	%r3, %r5
	bne-	1b
	addc	%r4, %r4, %r9
	addze	%r3, %r3
2:
	mfspr	%r6,285
	mfspr	%r7,284
	mfspr	%r8,285
	cmpw	%r6, %r8
	bne-	2b
	! compare if we did 100 ticks
	subfc	%r0, %r7, %r4
	subfe.	%r0, %r6, %r3
	bge+	2b
	blr
	SET_SIZE(tenmicrosec)

	.text

	ENTRY(usec_delay)
	b	drv_usecwait		! use the ddi routine

/*
 *	timebase_period = (nanosecs << NSEC_SHIFT) between each increment to 
		time base lower.
 *	tbticks_per_10usec = nanosecs between each increment to time base lower.
 *			  e.g., 100 for first 603, and 150 for first 604
 *	These are set based on OpenFirmware properties in mlsetup().
 */
	.data
	.align	2
	.globl	timebase_period
timebase_period:
	.long	100 << NSEC_SHIFT	! set in fiximp_obp().
	.size	timebase_period, 4

	.globl	tbticks_per_10usec
tbticks_per_10usec:
	.long	100			! set in fiximp_obp().
	.size	tbticks_per_10usec, 4

#endif /* lint */

/*
 * gethrtime() returns returns high resolution timer value
 */
#if defined(lint) || defined(__lint)
hrtime_t
gethrtime(void)
{
	return ((hrtime_t)0);
}

#else	/* lint */
	.text
	ENTRY_NP(gethrtime)
	mfpvr	%r5
	sri	%r5,%r5,16		! Version (1,3,4,20)
	cmpi	%r5,1
	bne-	.gethrtime_not601

	lis	%r5,1000000000>>16
	ori	%r5,%r5,1000000000 & 0xffff	! 1000000000 nsec per sec

.read_rtcu:
	mfrtcu	%r7			! sec
	mfrtcl	%r6			! nsec
	mfrtcu	%r8
	cmpl	%r7, %r8
	bne-	.read_rtcu

	mullw	%r3,%r5,%r7		! r3 = lo32(nsec_per_sec * rtcu)
	mulhwu	%r4,%r5,%r7		! r4 = hi32(nsec_per_sec * rtcu)

	addc	%r3,%r3,%r6
	addze	%r4,%r4			! 64bit addition of rtcl and r3,r4

	blr				! return 64bit nsec count in r3,r4

.gethrtime_not601:
	lis	%r6, timebase_period@ha
	lwz	%r6, timebase_period@l(%r6)
1:
	! read the current timebase value.
	mftbu	%r8
	mftb	%r7
	mftbu	%r5
	cmpl	%r8, %r5
	bne-	1b
	!
	! The time in nanoseconds is (tb >> NSEC_SHIFT) * timebase_period.
	! The shift is done partly before and partly after the multiply
	! to avoid overflowing 64 bits on the multiply, and to avoid throwing
	! away too much precision.
	!	%r8 = %tbu
	!	%r7 = %tbl
	!
	slwi	%r9, %r8, 32-NSEC_SHIFT1 ! %r9 = bits shifted from high 
	srwi	%r8, %r8, NSEC_SHIFT1
	srwi	%r7, %r7, NSEC_SHIFT1
	or	%r7, %r7, %r9		! %r8:%r7 = (tb >> NSEC_SHIFT1)

	! 64-bit multiply by the timebase_period

	mullw	%r3, %r7, %r6
	mulhwu	%r4, %r7, %r6
	mullw	%r5, %r8, %r6
	add	%r4, %r4, %r5		! r3,r4 = nsecs for the timebase value

	slwi	%r9, %r4, 32-NSEC_SHIFT2 ! %r9 = bits shifted from high 
	srwi	%r5, %r3, NSEC_SHIFT2	! low order word
#ifdef _LONG_LONG_LTOH			/* little-endian */
	srwi	%r4, %r4, NSEC_SHIFT2	! high order word
	or	%r3, %r5, %r9		! OR bits from high-order word
#else 					/* big-endian */
	srwi	%r3, %r4, NSEC_SHIFT2	! high order word
	or	%r4, %r5, %r9		! OR bits from high-order word
#endif
	blr				! return 64bit nsec count in r3,r4
	SET_SIZE(gethrtime)
#endif


/*
 * Read time base.
 */
#if defined(lint) || defined(__lint)
hrtime_t
get_time_base(void)
{
	return ((hrtime_t)0);
}
/* XXX - 601 returns zero for now */
#else
	ENTRY(get_time_base)
	mfpvr	%r5
	sri	%r5,%r5,16		! Version (1,3,4,20)
	cmpi	%r5,1
	beq-	2f			! skip on 601
1:
#ifdef _LONG_LONG_LTOH			/* little-endian */
	mftbu	%r4
	mftb	%r3
	mftbu	%r5
	cmpl	%r5, %r4
	beqlr+
#else					/* big-endian */
	mftbu	%r3
	mftb	%r4
	mftbu	%r5
	cmpl	%r5, %r3
	beqlr+
#endif
	b	1b			! miscompare. re-read
2:
	li	%r3, 0
	li	%r4, 0
	blr				! return 0 on 601
	SET_SIZE(get_time_base)
#endif /* lint */

/*
 * gethrvtime() returns high resolution thread virtual time
 */

#if defined(lint) || defined(__lint)
/*ARGSUSED*/
void
gethrvtime(timespec_t *tp)
{}
#else

	ENTRY_NP(gethrvtime)
	mflr	%r0
	stwu	%r1, -16(%r1)
	stw	%r0, 20(%r1)

	bl	gethrtime

	lhz	%r5, T_PROC_FLAG(THREAD_REG)
	andi.	%r5, %r5, TP_MSACCT
	beq+	.gethrvtime1
	lwz	%r5, T_LWP(THREAD_REG)		! micro-state acct ON
	lwz	%r6, LWP_STATE_START(%r5)
	lwz	%r7, LWP_STATE_START+4(%r5)
	lwz	%r8, LWP_ACCT_USER(%r5)
	lwz	%r9, LWP_ACCT_USER+4(%r5)
	subfc	%r3, %r6, %r3
	subfe	%r4, %r7, %r4
	add	%r3, %r3, %r8
	addc	%r4, %r4, %r9
	b	.gethrvret
.gethrvtime1:					! micro-state acct OFF
	lwz	%r5, T_LWP(THREAD_REG)
	lwz	%r6, LWP_MS_START(%r5)
	lwz	%r7, LWP_MS_START+4(%r5)
	lwz	%r8, LWP_UTIME(%r5)		! user time estimate
	subfc	%r3, %r6, %r3
	subfe	%r4, %r7, %r4			! subtract process start time
	lis	%r9, 0x98
	ori	%r9, %r9, 0x9680		! %r9 = 10000000 decimal
	mulhwu	%r10, %r8, %r9
	mullw	%r9, %r8, %r9
	cmpl	%r4, %r10
	bne	.gethrvtime2
	cmpl	%r9, %r3
.gethrvtime2:
	bge	.gethrvret			! elapsed time is greater
	mr	%r3, %r9
	mr	%r4, %r10

.gethrvret:
	lwz	%r0, 20(%r1)
	addi	%r1, %r1, 16
	mtlr	%r0
	blr
	SET_SIZE(gethrvtime)
#endif

/*
 * Fast system call wrapper to return hi-res time of day.
 * Exit:
 *	%r3 = seconds.
 *	%r4 = nanoseconds.
 */
#if defined(lint) || defined(__lint)
void
get_hrestime(void)
{}
#else	/* lint */

	ENTRY_NP(get_hrestime)
	mflr	%r0
	stwu	%r1, -16(%r1)
	stw	%r0, 20(%r1)
	la	%r3, 8(%r1)		! %r3 = address for timespec result
	bl	gethrestime
	lwz	%r4, 8+4(%r1)		! nanoseconds
	lwz	%r3, 8(%r1)		! seconds

	lwz	%r0, 20(%r1)
	addi	%r1, %r1, 16
	mtlr	%r0
	blr
	SET_SIZE(get_hrestime)
#endif

#if defined(lint) || defined(__lint)

void
pc_reset(void)
{}

#else	/* lint */

	ENTRY(pc_reset)
.patch_batu5:
	li	%r4, IBATU_LOMEM_MAPPING
.patch_batl5:
	li	%r5, IBATL_LOMEM_MAPPING
	mtspr	BAT3U, %r4		! BATU3/IBATU3 mapping for low mem
	mtspr	BAT3L, %r5		! BATL3/IBATL3 mapping for low mem

	mfmsr	%r3
	rlwinm	%r3,%r3,0,17,15		! MSR_EE off - no interrupts
	mtmsr	%r3
	isync
	ba	RESET_PADDR		! jump to low memory handler
	SET_SIZE(pc_reset)
#endif	/* lint */

#if defined(lint) || defined(__lint)

void
return_instr(void)
{}

#else	/* lint */

	ENTRY_NP(return_instr)
	blr
	SET_SIZE(return_instr)

#endif /* lint */
