/*
 *	Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)swtch.s	1.14	95/05/18 SMI"

/*
 * Process switching routines.
 */

#include <sys/asm_linkage.h>
#include <sys/machthread.h>
#include <sys/mmu.h>
#include <sys/psw.h>

#if defined(lint) || defined(__lint)
#include <sys/thread.h>
#else	/* lint */
#include "assym.s"
#endif	/* lint */

/*
 * resume(thread_id t)
 *
 * a thread can only run on one processor at a time. there
 * exists a window on MPs where the current thread on one
 * processor is capable of being dispatched by another processor.
 * some overlap between outgoing and incoming threads can happen
 * when they are the same thread. in this case where the threads
 * are the same, resume() on one processor will spin on the incoming
 * thread until resume() on the other processor has finished with
 * the outgoing thread.
 *
 * The MMU context changes when the resuming thread resides in a different
 * process.  Kernel threads are known by resume to reside in process 0.
 * The MMU context, therefore, only changes when resuming a thread in
 * a process different from curproc.
 *
 * resume_from_intr() is called when the thread being resumed was not
 * passivated by resume (e.g. was interrupted).  This means that the
 * resume lock is already held and that a restore context is not needed.
 * Also, the MMU context is not changed on the resume in this case.
 *
 * resume_from_zombie() is the same as resume except the calling thread
 * is a zombie and must be put on the deathrow list after the CPU is
 * off the stack.
 */

/*
 * Setup stack frame and save non volatile registers.
 */
#define	FRAMESIZE	SA(MINFRAME+(20*4))
#define SAVE_REGS()\
	mflr	%r0;\
	stw	%r0, 4(%r1);\
	stwu	%r1, -FRAMESIZE(%r1);\
	stw	%r13,MINFRAME+ 0*4(%r1);\
	stw	%r14,MINFRAME+ 1*4(%r1);\
	stw	%r15,MINFRAME+ 2*4(%r1);\
	stw	%r16,MINFRAME+ 3*4(%r1);\
	stw	%r17,MINFRAME+ 4*4(%r1);\
	stw	%r18,MINFRAME+ 5*4(%r1);\
	stw	%r19,MINFRAME+ 6*4(%r1);\
	stw	%r20,MINFRAME+ 7*4(%r1);\
	stw	%r21,MINFRAME+ 8*4(%r1);\
	stw	%r22,MINFRAME+ 9*4(%r1);\
	stw	%r23,MINFRAME+10*4(%r1);\
	stw	%r24,MINFRAME+11*4(%r1);\
	stw	%r25,MINFRAME+12*4(%r1);\
	stw	%r26,MINFRAME+13*4(%r1);\
	stw	%r27,MINFRAME+14*4(%r1);\
	stw	%r28,MINFRAME+15*4(%r1);\
	stw	%r29,MINFRAME+16*4(%r1);\
	stw	%r30,MINFRAME+17*4(%r1);\
	stw	%r31,MINFRAME+18*4(%r1);\
	mfcr	%r0;\
	stw	%r0, MINFRAME+19*4(%r1)

/*
 * Restore stack frame and save non volatile registers.
 */
#define RESTORE_REGS()\
	lwz	%r13,MINFRAME+ 0*4(%r1);\
	lwz	%r14,MINFRAME+ 1*4(%r1);\
	lwz	%r15,MINFRAME+ 2*4(%r1);\
	lwz	%r16,MINFRAME+ 3*4(%r1);\
	lwz	%r17,MINFRAME+ 4*4(%r1);\
	lwz	%r18,MINFRAME+ 5*4(%r1);\
	lwz	%r19,MINFRAME+ 6*4(%r1);\
	lwz	%r20,MINFRAME+ 7*4(%r1);\
	lwz	%r21,MINFRAME+ 8*4(%r1);\
	lwz	%r22,MINFRAME+ 9*4(%r1);\
	lwz	%r23,MINFRAME+10*4(%r1);\
	lwz	%r24,MINFRAME+11*4(%r1);\
	lwz	%r25,MINFRAME+12*4(%r1);\
	lwz	%r26,MINFRAME+13*4(%r1);\
	lwz	%r27,MINFRAME+14*4(%r1);\
	lwz	%r28,MINFRAME+15*4(%r1);\
	lwz	%r29,MINFRAME+16*4(%r1);\
	lwz	%r30,MINFRAME+17*4(%r1);\
	lwz	%r31,MINFRAME+18*4(%r1);\
	lwz	%r0, MINFRAME+19*4(%r1);\
	mtcrf	0xff, %r0;\
	addi	%r1, %r1, FRAMESIZE;\
	lwz	%r0, 4(%r1);\
	mtlr	%r0

/*
 *	Resume() needs to create a stack frame, saving all non-volatile
 *	registers there so that all that is needed to resume the thread
 *	is "t_sp" and "t_pc".  Other state that needs to be attended to
 *	is the segment registers for this thread's user space.
 *
 *	Note:
 *	On PowerPC, is there a need for a stwcx. to clear a reservation?
 *	It seems unlikely since locks are held on the way to resume().
 *
 *	Register usage:
 *
 *	    On Entry:
 *
 *		R2  - curthread
 *		R3  - newthread
 *
 *	    After non-volatile registers are saved:
 *
 *		R0  - temp
 *		R5  - temp
 *		R6  - temp
 *		R15 - old thread (outgoing thread)
 *		R14 - newthread
 *		R20 - CPU
 *
 *	The stack frame created by resume() looks like:
 *
 *		0  (%r1) - back chain
 *		4  (%r1) - Reserved for called function
 *		8  (%r1) - R13
 *		12 (%r1) - R14
 *		16 (%r1) - R15
 *		20 (%r1) - R16
 *		24 (%r1) - R17
 *		...
 *		80 (%r1) - R31
 *		84 (%r1) - condition register
 *		88 (%r1) - PAD
 *		92 (%r1) - PAD
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
resume(kthread_id_t t)
{}

#else	/* lint */

	ENTRY(resume)
	SAVE_REGS()			! Save non volatile registers
	mr	%r14,%r3		! R14 = t (new thread)
	mr	%r15, THREAD_REG	! R15 = curthread (old thread)
	lis	%r6,resume_exit@ha
	la	%r6,resume_exit@l(%r6)
	lwz	%r20,T_CPU(THREAD_REG)	! R20 = CPU
	stw	%r6,T_PC(THREAD_REG)	! Set return address as resume_exit()
					! for the current thread
	stw	%r1,T_SP(THREAD_REG)	! Save SP for the current thread

	!
	! Perform context switch callback if set.
	! This handles floating-point and/or coprocessor state saving.
	!
	lwz	%r3, T_CTX(%r15)
	cmpwi	%r3, 0
	beq+	.no_ctx_save
	mr	%r3, %r15
	bl	savectx			! savectx(curthread)
.no_ctx_save:

	lwz	%r5,T_PROCP(%r14)	! new proc
	lwz	%r0,T_PROCP(THREAD_REG)	! resuming the same process?
	cmpw	%r0,%r5
	beq-	.L2			! don't change mmu context

	/*
	 * Change the mmu context by loading the user segment registers (0-13)
	 */
	lwz	%r3, P_AS(%r5)
	lwz	%r3, A_HAT(%r3)
	lwz	%r3, HAT_CTX(%r3)	! VSID_RANGE_ID
	rlwinm	%r3, %r3, 4, 8, 31	! vsid-base = VSID_RANGE_ID << 4
	lis	%r4, SR_KU >> 16
	or	%r3, %r3, %r4		! segment register value
	addi	%r4,%r3,1
	mtsr	0,%r3
	addi	%r3,%r4,1
	mtsr	1,%r4
	addi	%r4,%r3,1
	mtsr	2,%r3
	addi	%r3,%r4,1
	mtsr	3,%r4
	addi	%r4,%r3,1
	mtsr	4,%r3
	addi	%r3,%r4,1
	mtsr	5,%r4
	addi	%r4,%r3,1
	mtsr	6,%r3
	addi	%r3,%r4,1
	mtsr	7,%r4
	addi	%r4,%r3,1
	mtsr	8,%r3
	addi	%r3,%r4,1
	mtsr	9,%r4
	addi	%r4,%r3,1
	mtsr	10,%r3
	addi	%r3,%r4,1
	mtsr	11,%r4
	addi	%r4,%r3,1
	mtsr	12,%r3
	mtsr	13,%r4
	sync
.L2:
	/*
	 * Temporarily switch to idle thread's stack.
	 */
	lwz	%r5,CPU_IDLE_THREAD(%r20) ! idle thread pointer
	lwz	%r1,T_SP(%r5)

	/*
	 * Set the idle thread as the current thread
	 */
	mtsprg	0,%r5			! set curthread
	mr	THREAD_REG,%r5

	/*
	 * unlock outgoing thread's lock, possibly dispatched by
	 * another processor.
	 */
	li	%r0,0
	eieio				! make all data visible
	stb	%r0,T_LOCK(%r15)	! free the thread lock

	/*
	 * IMPORTANT: Registers at this point must be:
	 *		R14 - newthread
	 *		R20 - CPU
	 */
	ALTENTRY(_resume_from_idle)
	/*
	 * Spin until incoming thread's mutex has been unlocked. This
	 * mutex is unlocked when it becomes safe for the thread to run.
	 */
	la	%r3,T_LOCK(%r14)
	bl	lock_set

	/*
	 * Fix CPU structure to indicate new running thread.
	 * Set pointer in new thread to the CPU structure.
	 */
#if defined(MP)
	lwz	%r3,T_CPU(%r14)
	cmpw    %r3,%r20
	beq+	.L6

	lwz	%r5,CPU_SYSINFO_CPUMIGRATE(%r3)
	addi	%r5,%r5,1 		! cpu_sysinfo.cpumigrate++
	stw	%r5,CPU_SYSINFO_CPUMIGRATE(%r3)

#endif
	stw	%r20,T_CPU(%r14)	! set new thread's CPU pointer
.L6:
	mtsprg	0,%r14			! set curthread
	mr	THREAD_REG,%r14
	stw	%r14,CPU_THREAD(%r20)	! set CPU's thread pointer
	lwz	%r0,T_LWP(%r14)	 	! set associated lwp to
	lwz	%r5,T_STACK(%r14)
	stw	%r0,CPU_LWP(%r20) 	! CPU's lwp ptr
	mtsprg	1,%r5			! set kernel stack pointer
					! for entry from userland

	lwz	%r1,T_SP(%r14)		! switch to new thread's stack

	lwz	%r15,T_PC(%r14)

	!
	! Restore resuming thread's context (FP/coprocessor)
	!
	lwz	%r3, T_CTX(%r14)
	cmpwi	%r3, 0
	beq+	.no_ctx_restore
	mr	%r3, %r14
	bl	restorectx		! restorectx(newthread)
.no_ctx_restore:

	/*
	 * Set priority as low as possible, blocking all interrupt
	 * threads that may be active.
	 */
	bl	spl0			! call spl0

	mfmsr	%r3			! enable interrupts unconditionally
	ori	%r3,%r3,MSR_EE		! (needed for kpreempt getting to
	mtmsr	%r3			! with interrupts disabled)

	! Transfer to the resuming thread's PC
	mtlr	%r15
	blrl				! intentionally set LR (debug?)

#endif	/* lint */


#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
resume_from_intr(kthread_id_t t)
{}

#else	/* lint */

	ENTRY(resume_from_intr)
	SAVE_REGS();			! Save non volatile registers
	mr	%r16, THREAD_REG	! R16 = curthread
	lis	%r6,resume_exit@ha
	la	%r6,resume_exit@l(%r6)
	lwz	%r20,T_CPU(THREAD_REG)	! R20 = CPU
	stw	%r6,T_PC(THREAD_REG)	! Set return address as resume_exit()
					! for the current thread
	stw	%r1,T_SP(THREAD_REG)	! Save SP for the current thread

	lwz	%r1,T_SP(%r3)		! switch to new thread's stack
	lwz	%r15,T_PC(%r3)

	li	%r0,0
	mtsprg	0,%r3			! set curthread
	mr	THREAD_REG,%r3
	stw	%r3,CPU_THREAD(%r20)	! set CPU's thread pointer

	/*
	 * Need to determine if this is the clock thread or not.
	 */
	lis	%r4,clock_thread@ha
	lwz	%r4,clock_thread@l(%r4)
	cmp	%r4,%r16
	beq	1f

	/*
	 * unlock outgoing thread's mutex, possibly dispatched by
	 * another processor.
	 */
	eieio				! make all data visible
	stw	%r0,T_LOCK(%r16)	! free the thread's mutex

	/*
	 * Set priority to 0 unless interrupt threads are blocked,
	 * in which case we set spl to the level of the highest priority
	 * blocked interrupt thread.
	 */
	bl	spl0			! call spl0

	/* Transfer to the resuming thread's PC */
	mtlr	%r15
	blrl				! intentionally set LR (debug?)
	/* NO RETURN */

1:
	/*
	 * Set priority to priority of the interrupted thread.
	 * This old priority was saved at 12(T_STACK) (at the bottom
	 * of the stack).
	 */
	lwz	%r5,T_STACK(%r16)
	lwz	%r3,12(%r5)		! oldpri saved in clock_intr

	/*
	 * unlock outgoing thread's mutex, possibly dispatched by
	 * another processor.
	 */
	eieio				! make all data visible
	stw	%r0,T_LOCK(%r16)	! free the thread's mutex

	bl	splx			! call splx

	/* Transfer to the resuming thread's PC */
	mtlr	%r15
	blrl				! intentionally set LR (debug?)
	/* NO RETURN */

	SET_SIZE(resume_from_intr)

#endif	/* lint */


#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
resume_from_zombie(kthread_id_t t)
{}

#else	/* lint */

	ENTRY(resume_from_zombie)
	/* SAVEREGS()						*/
	/* Note: Don't bother saving anything for this thread.	*/
	mr	%r14,%r3		! R14 = t (new thread)
	mr	%r15, THREAD_REG	! R15 = curthread
	lwz	%r20,T_CPU(THREAD_REG)	! R20 = CPU

	lwz	%r5,T_PROCP(%r14)	! new proc
	lwz	%r0,T_PROCP(THREAD_REG)! resuming the same process?
	cmpw	%r0,%r5
	beq-	.L2_2			! don't change mmu context

	/*
	 * Change the mmu context by loading the user segment registers (0-13)
	 */
	lwz	%r3, P_AS(%r5)
	lwz	%r3, A_HAT(%r3)
	lwz	%r3, HAT_CTX(%r3)	! VSID_RANGE_ID
	rlwinm	%r3, %r3, 4, 8, 31	! vsid-base = VSID_RANGE_ID << 4
	lis	%r4, SR_KU >> 16
	or	%r3, %r3, %r4		! segment register value
	addi	%r4,%r3,1
	mtsr	0,%r3
	addi	%r3,%r4,1
	mtsr	1,%r4
	addi	%r4,%r3,1
	mtsr	2,%r3
	addi	%r3,%r4,1
	mtsr	3,%r4
	addi	%r4,%r3,1
	mtsr	4,%r3
	addi	%r3,%r4,1
	mtsr	5,%r4
	addi	%r4,%r3,1
	mtsr	6,%r3
	addi	%r3,%r4,1
	mtsr	7,%r4
	addi	%r4,%r3,1
	mtsr	8,%r3
	addi	%r3,%r4,1
	mtsr	9,%r4
	addi	%r4,%r3,1
	mtsr	10,%r3
	addi	%r3,%r4,1
	mtsr	11,%r4
	addi	%r4,%r3,1
	mtsr	12,%r3
	mtsr	13,%r4
	sync
.L2_2:
	/*
	 * Temporarily switch to idle thread's stack so that the zombie
	 * thread's stack can be reclaimed by the reaper.
	 */
	lwz	%r5,CPU_IDLE_THREAD(%r20) ! idle thread pointer
	lwz	%r1,T_SP(%r5)

	/*
	 * Set the idle thread as the current thread
	 */
	mtsprg	0,%r5			! set curthread
	mr	THREAD_REG,%r5

	/*
	 * Put the zombie on death-row if it is a detached thread, otherwise
	 * it was put onto a per process zombie list by lwp_exit().
	 */
	mr	%r3, %r15
	bl	reapq_add		! reapq_add(curthread);
	b	_resume_from_idle	! finish job of resume

	SET_SIZE(resume_from_zombie)

#endif	/* lint */

/*
 * resume_exit():
 *	Restore the stack frame (saved by resume()) to resume the execution
 *	of the thread and to return to the caller of the resume() (typically
 *	switch()).  R1 has the saved T_SP value for this thread.  R2 has
 *	curthread.
 *
 *	The system stack at entry to this routine would look like:
 *
 *		0  (%r1) - back chain
 *		4  (%r1) - Reserved for called function
 *		8  (%r1) - R13
 *		12 (%r1) - R14
 *		16 (%r1) - R15
 *		20 (%r1) - R16
 *		24 (%r1) - R17
 *		...
 *		80 (%r1) - R31
 *		84 (%r1) - condition register
 *		88 (%r1) - PAD
 *		92 (%r1) - PAD
 *		96 (%r1) - previous back chain
 *		100 (%r1) - link register
 */
#if defined(lint)

/* ARGSUSED */
void
resume_exit()
{}

#else	/* lint */

	ENTRY(resume_exit)
	RESTORE_REGS();			! restore non-volatiles
	blr				! and return
	SET_SIZE(resume_exit)

#endif	/* lint */

/*
 * thread_start()
 *
 * This function is called to start all kernel-only threads.  The
 * semantics of these kernel threads is to call the function passed
 * to thread_load, and if/when it returns, call thread_exit().
 * thread_load() is given 2 arguments for kernel threads, we load
 * these up from where they were saved in thread_load().
 *
 * We enter this function with the following state:
 *
 *	%r1/sp	points to t_stk (T_STACK), having room for a MINFRAME
 *		sized stack frame (16 bytes, or 4 words) of info
 *	%r2	curthread
 *	*sp	0 (terminator for stack trace)
 *	*(sp+8)	entry point for kernel thread
 *	*(sp+12)	function argument "arg"
 *	*(sp+16)	function argument "len"
 */

#if defined(lint) || defined(_lint)

void
thread_start(void)
{}

#else   /* lint */

	ENTRY(thread_start)
	lwz	%r0,8(%r1)	! entry point
	lwz	%r3,12(%r1)	! arg 1 (arg)
	mtlr	%r0
	lwz	%r4,16(%r1)	! arg 2 (len)
	blrl			! call function()
	bl	thread_exit	! call thread_exit()
				! NO RETURN
	SET_SIZE(thread_start)

#endif  /* lint */
