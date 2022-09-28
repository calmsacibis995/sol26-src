/*
 * Copyright (c) 1992-1993, by Sun Microsystems, Inc.
 */

#ident	"@(#)lock_prim.s	1.47	96/04/17 SMI"

#if defined(lint) || defined(__lint)
#include <sys/types.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <vm/page.h>
#include <sys/mutex_impl.h>
#else	/* lint */
#include "assym.s"
#endif	/* lint */

#include <sys/asm_linkage.h>
#include <sys/asm_misc.h>
#include <sys/regset.h>

/* #define DEBUG */


#ifndef DISP_LOCK_STATS
/*
 * disp_lock_enter - get a dispatcher lock after raising spl to block other
 *	dispatcher activity.
 * - %o0 contains address of lock byte.
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
disp_lock_enter(disp_lock_t *mp)
{}

#else	/* lint */

	ENTRY(disp_lock_enter)
.L0:
	call	splhigh			/ block disp activity
	movl	%gs:CPU_THREAD, %ecx	/ avoid AGI below
	movl	4(%esp), %edx		/ lock address
	lock
	btsw	$0, (%edx)
	jc	.L1			/ lock was already set; spin
	movw	%eax, T_OLDSPL(%ecx)	/ save old spl in thread
	ret
	/*
	 * lock failed - spin. But first, drop spl and check for panicking or
	 * 	spinning on a uniprocessor.
	 */
.L1:
	pushl	%eax
	call	splx			/ drop spl
	addl	$4, %esp

	cmpl	$1, ncpus		/ if UP
	je	.L3			/ then panic

	movl	4(%esp), %edx		/ lock address
.L2:
	testb	$1, (%edx)		/ if lock is free
	jz	.L0			/ then try again
	cmpl	$0, panicstr		/ if not panic
	je	.L2			/ then spin
	ret				/ else return

.L3:
	pushl	$.lock_panic_msg
	call	panic			/ panic - will not return
	ret
	SET_SIZE(disp_lock_enter)

#endif	/* lint */

/*
 * disp_lock_exit - drop dispatcher lock.
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
disp_lock_exit(disp_lock_t *mp)
{}

#else	/* lint */

	ENTRY(disp_lock_exit)
	ALTENTRY(disp_mutex_exit)	/* XXX */
	LOADCPU(%ecx)
	movl	4(%esp), %eax
#ifdef DEBUG
	testb	$1, (%eax)
	jz	.disp_lock_exit_panic	/ lock not held - panic
#endif
	xorl	%edx, %edx
	cmpb	%dl, CPU_KPRUNRUN(%ecx)
	jne	.LL0

	movl	CPU_THREAD(%ecx), %ecx
	movb	%dl, (%eax)		/ #LOCK need not be done per Intel
					/ They swear that the Write buffer
					/ problem was an Alpha Pentium bug
	movw	T_OLDSPL(%ecx), %edx
	pushl	%edx
	call	splx			/ reset priority 
	addl	$4, %esp
	ret

.LL0:
	movl	CPU_THREAD(%ecx), %ecx
	movb	%dl, (%eax)
	movw	T_OLDSPL(%ecx), %edx
	pushl	%edx
	call	splx			/ reset priority 
	movl	$-1, (%esp)
	call	kpreempt
	addl	$4, %esp
	ret


#ifdef DEBUG
.disp_lock_exit_panic:
	pushl	$.disp_lock_exit_msg
	call	panic			/ panic - will not return
#endif
	SET_SIZE(disp_lock_exit)

#ifdef DEBUG
	.data
.disp_lock_exit_msg:
	.string "disp_lock_exit: lock not held"
	.text
#endif

#endif	/* lint */

/*
 * disp_lock_enter_high - Get lock without changing spl level.
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
disp_lock_enter_high(disp_lock_t *mp)
{}

#else	/* lint */

/* The definition for this is above lock_set to avoid a jmp */

#endif	/* lint */

/*
 * disp_lock_exit_nopreempt - Drop dispatcher lock without checking preemption.
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */    
void
disp_lock_exit_nopreempt(disp_lock_t *mp)
{}

#else	/* lint */

	ENTRY(disp_lock_exit_nopreempt)
	movl	%gs:CPU_THREAD, %ecx
	movl	4(%esp), %eax
#ifdef DEBUG
	testb	$1, (%eax)
	jz	.disp_lock_exit_panic	/ lock not held - panic
#endif
	xorl	%edx, %edx
	movb	%dl, (%eax)
	movw	T_OLDSPL(%ecx), %edx
	pushl	%edx
	call	splx			/ reset priority 
	addl	$4, %esp
	ret
	SET_SIZE(disp_lock_exit_nopreempt)

#endif	/* lint */

/*
 * Clear dispatcher spinlock, but leave the spl high.
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
disp_lock_exit_high(disp_lock_t *mp)
{}

#else	/* lint */

	ENTRY(disp_lock_exit_high)
	movl	4(%esp), %eax
#ifdef DEBUG
	testb	$1, (%eax)
	jz	.disp_lock_exit_panic	/ lock not held - panic
#endif
	movb	$0, (%eax)		/ drop the lock
	ret
	SET_SIZE(disp_lock_exit_high)

#endif	/* lint */

#endif	/* DISP_LOCK_STATS */

/*
 * lock_try(lp), ulock_try(lp)
 *	- returns non-zero on success.
 *	- doesn't block interrupts so don't use this to spin on a lock.
 *	
 *      ulock_try() is for a lock in the user address space.
 *      For all V7/V8 sparc systems they are same since the kernel and
 *      user are mapped in a user' context.
 *      For V9 platforms the lock_try and ulock_try are different impl.
 *	ulock_try() is here for compatiblility reasons only. We also
 * 	added in some checks under #ifdef DEBUG.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
lock_try(lock_t *lp)
{ return (0); }

/* ARGSUSED */
int
ulock_try(lock_t *lp)
{ return (0); }

#else	/* lint */

	ENTRY(lock_try)
	movl	$-1,%edx
	movl	4(%esp),%ecx
	xorl	%eax,%eax
	xchgb	%dl, (%ecx)	/using dl will avoid partial
	testb	%dl,%dl		/stalls on P6 ?
	setz	%al
	ret
	SET_SIZE(lock_try)

	ENTRY(ulock_try)
#ifdef DEBUG
	cmpl	$KERNELBASE, 4(%esp)		/ test uaddr < KERNELBASE
	jb	ulock_pass			/ uaddr < KERNELBASE, proceed

	pushl	$.ulock_panic_msg
	call	panic

#endif /* DEBUG */

ulock_pass:
	movl	$1,%eax
	movl	4(%esp),%ecx
	xchgb	%al, (%ecx)
	xorb	$1, %al
	ret
	SET_SIZE(ulock_try)

#ifdef DEBUG
	.data
.ulock_panic_msg:
	.string "ulock_try: Argument is above KERNELBASE"
	.text
#endif	/* DEBUG */

#endif	/* lint */

/*
 * lock_clear(lp)
 *	- unlock lock without changing interrupt priority level.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
lock_clear(lock_t *lp)
{}

/* ARGSUSED */
void
ulock_clear(lock_t *lp)
{}

#else	/* lint */

	ENTRY(lock_clear)
	movl	4(%esp),%eax
	xorl	%ecx,%ecx
	movb	%cl, (%eax)
	ret
	SET_SIZE(lock_clear)

	ENTRY(ulock_clear)
#ifdef DEBUG
	cmpl	$KERNELBASE, 4(%esp)		/ test uaddr < KERNELBASE
	jb	ulock_clr			/ uaddr < KERNELBASE, proceed

	pushl	$.ulock_clear_msg
	call	panic
#endif

ulock_clr:
	movl	4(%esp),%eax
	xorl	%ecx,%ecx
	movb	%cl, (%eax)
	ret
	SET_SIZE(ulock_clear)

#ifdef DEBUG
	.data
.ulock_clear_msg:
	.string "ulock_clear: Argument is above KERNELBASE"
	.text
#endif	/* DEBUG */


#endif	/* lint */

/*
 * lock_set_spl(lp, pl)
 * 	Returns old pl.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
lock_set_spl(lock_t *lp, int pl)
{ return (0); }

#else	/* lint */

	ENTRY(lock_set_spl)
.LSS1:
	movl	8(%esp), %eax	/ get priority level
	pushl	%eax
	call	splr		/ raise priority level
	movl 	8(%esp), %ecx	/ 8(esp) here gets us *lock
	movl	$-1, %edx
	addl	$4, %esp
	xchgb	%dl,(%ecx)	/ try to set lock
	testb	%dl,%dl
	jnz	.lock_set_spin	/ lock alread held - go spin
.LSS4:
	ret			/ return old priority level in eax

	/
	/ Spin on lock without using an atomic operation to prevent the caches
	/ from unnecessarily moving ownership of the line around.
	/ also drop the spl during the spin.
	/
.lock_set_spin:
	cmpl	$0, panicstr
	jne	.LSS4

.LSS2:
	pushl	%eax		/ returned by splr
	call	splx		/ drop spl
	addl	$4, %esp

	movl 	4(%esp), %ecx
.LSS6:
	testb	$1, (%ecx)	/ lock appears to be free, try again
	je	.LSS1
	/
	/ Debug - see if there is only one CPU.  
	/
	cmpl	$1, ncpus
	jne	.LSS6

	pushl	$.lock_panic_msg
	call panic
	ret
	SET_SIZE(lock_set_spl)

	.data
.lock_panic_msg:
	.string "lock_set: lock held and only one CPU"
	.text

#endif	/* lint */

/*
 * void
 * lock_init(lp)
 */

#if defined(lint)

/* ARGSUSED */
void
lock_init(lock_t *lp)
{}

#else	/* lint */

	ENTRY(lock_init)
	movl	4(%esp), %eax
	movb	$0, (%eax)
	ret
	SET_SIZE(lock_init)

#endif	/* lint */

/*
 * void
 * lock_set(lp)
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
lock_set(lock_t *lp)
{}

#else	/* lint */

	ENTRY(disp_lock_enter_high)

	ENTRY(lock_set)
	movl	4(%esp), %eax
.LS1:
	movl	$-1, %edx
	xchgb	%dl,(%eax)	/ try to set lock
	testb	%dl,%dl
	jnz	.LS3			/ lock already held - go spin
.LS2:
	ret				/ return
	/
	/ Spin on lock without using an atomic operation to prevent the caches
	/ from unnecessarily moving ownership of the line around.
	/
.LS3:
	cmpb	$0, (%eax)		/ lock appears to be free, try again
	je	.LS1
	cmpl	$0, panicstr		/ Has system paniced?  
	jne	.LS2			/ after panic, feign success 
	cmpl	$1, ncpus		/ Is this an MP machine ?
	jne	.LS3			/    yes, keep going
	pushl	$.lock_panic_msg	/ only 1 CPU - will never get lock
	call panic			/ so panic
	ret
	SET_SIZE(lock_set)

#endif	/* lint */

/*
 * lock_clear_splx(lp, s)
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
lock_clear_splx(lock_t *lp, int s)
{}

#else	/* lint */

	ENTRY(lock_clear_splx)
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	movb	$0, (%eax)		/ clear lock
	pushl	%edx			/ old priority level
	call	splx
	addl	$4, %esp
	ret

	SET_SIZE(lock_clear_splx)

#endif	/* lint */


/*
 * mutex_enter() and mutex_exit().
 * 
 * These routines do the simple case of mutex_enter when the lock is not
 * held, and mutex_exit when no thread is waiting for the lock.
 * If anything complicated is going on, the C version (mutex_adaptive_enter) 
 * is called.
 *
 * These routines do not do lock tracing.  If that is needed,
 * a separate mutex type could be used.
 *
 * If you don't think this is tricky, look closer.
 *
 * Also see comments in i86/sys/mutex_impl.h.
 *
 * On 386, we do pushf+cli/popf to protect the critical section.
 * We do this rather than cli/sti in case mutex_enter() gets called
 * with interrupts disabled.  It shouldn't.
 *
 * On 486, we use cmpxchgl to set the lock and owner atomically.
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
mutex_enter(kmutex_t *lp)
{}
/* ARGSUSED */
int
mutex_adaptive_tryenter(mutex_impl_t *lp)
{ return (0); }

#else

	ENTRY_NP(mutex_x86_install)
	/
	/ This code overwrites mutex_enter() and mutex_adaptive_tryenter()
	/ with the appropriate CPU-specific versions the first time either
	/ routine is invoked.  This is extremely vile -- the right solution
	/ is to have krtld link in the appropriate version for us, but that
	/ technology is not yet available.  For now, read and be frightened.
	/
	pushfl
	cli
	movw	cputype, %ax
	andw	$CPU_ARCH, %ax
	cmpw	$I86_386_ARCH, %ax
	jne	.mutex_486_install

	movl	$.mutex_enter_386, %ecx
	movl	$.mutex_enter_386_end, %edx
	subl	%ecx, %edx
	pushl	%edx				/ len
	pushl	$mutex_enter			/ dst
	pushl	%ecx				/ src

	movl	$.mutex_adaptive_tryenter_386, %ecx
	movl	$.mutex_adaptive_tryenter_386_end, %edx
	subl	%ecx, %edx
	pushl	%edx				/ len
	pushl	$mutex_adaptive_tryenter	/ dst
	pushl	%ecx				/ src

	movl	$mutex_enter, %ecx
	subl	$.mutex_enter_386, %ecx		/ relocation distance
	movl	$.mutex_enter_386_jmp + 1, %eax	/ disp32 to be relocated

	jmp	.mutex_install_finish

.mutex_486_install:

	movl	$.mutex_enter_486, %ecx
	movl	$.mutex_enter_486_end, %edx
	subl	%ecx, %edx
	pushl	%edx				/ len
	pushl	$mutex_enter			/ dst
	pushl	%ecx				/ src

	movl	$.mutex_adaptive_tryenter_486, %ecx
	movl	$.mutex_adaptive_tryenter_486_end, %edx
	subl	%ecx, %edx
	pushl	%edx				/ len
	pushl	$mutex_adaptive_tryenter	/ dst
	pushl	%ecx				/ src

	movl	$mutex_enter, %ecx
	subl	$.mutex_enter_486, %ecx		/ relocation distance
	movl	$.mutex_enter_486_jnz + 2, %eax	/ disp32 to be relocated

.mutex_install_finish:

	movl	(%eax), %edx	/ %edx = current value of disp32
	subl	%ecx, %edx	/ %edx = relocated disp32 (compensate for bcopy)
	movl	%edx, (%eax)	/ store the adjusted disp32

	call	ovbcopy		/ install mutex_adaptive_tryenter()
	addl	$12, %esp	/ pop args
	call	ovbcopy		/ install mutex_enter()
	addl	$12, %esp	/ pop args

	popfl			/ enable interrupts
	popl	%eax		/ %eax = where we were called from
	jmp	*%eax		/ now do the intercepted mutex operation

	SET_SIZE(mutex_x86_install)

	ENTRY_NP(mutex_enter)

	pushl	$mutex_enter			/ bootstrap code - overwritten
	jmp	mutex_x86_install		/ by first invocation

.mutex_enter_386:
	movl	%gs:CPU_THREAD, %eax		/ eax = thread ptr
	movl	4(%esp), %ecx			/ ecx = lock ptr
	pushfl					/ critical section
	cli
	lock
	btsl	$31, M_OWNER(%ecx)		/ try to get lock
	jc	.miss386			/ didn't get it
	movl	%eax, M_OWNER(%ecx)		/ set owner to current thread
	popfl					/ reset interrupt enable bit
	ret
.miss386:
	popfl					/ reset interrupt enable bit
.mutex_enter_386_jmp:
	jmp	mutex_adaptive_enter		/ manually relocated!
.mutex_enter_386_end:

.mutex_enter_486:
	movl	%gs:CPU_THREAD, %edx		/ edx = thread ptr
	movl	4(%esp), %ecx			/ ecx = lock ptr
	xorl	%eax, %eax			/ eax = 0 (unheld adaptive)
	lock
	cmpxchgl %edx, M_OWNER(%ecx)
.mutex_enter_486_jnz:
	jnz	mutex_adaptive_enter		/ manually relocated!
	ret
.mutex_enter_486_end:

	SET_SIZE(mutex_enter)

	ENTRY(mutex_adaptive_tryenter)

	pushl	$mutex_adaptive_tryenter	/ bootstrap code - overwritten
	jmp	mutex_x86_install		/ by first invocation

.mutex_adaptive_tryenter_386:
	movl	%gs:CPU_THREAD, %eax		/ eax = thread ptr
	movl	4(%esp), %ecx			/ ecx = lock ptr
	pushfl					/ critical section
	cli
	lock
	btsl	$31, M_OWNER(%ecx)		/ try to get lock
	jc	.tryfail386			/ didn't get it
	movl	%eax, M_OWNER(%ecx)		/ set owner to current thread
	popfl					/ reset interrupt enable bit
	ret
.tryfail386:
	xorl	%eax, %eax			/ eax = 0 (tryenter failed)
	popfl					/ reset interrupt enable bit
	ret
.mutex_adaptive_tryenter_386_end:

.mutex_adaptive_tryenter_486:
	movl	%gs:CPU_THREAD, %edx		/ edx = thread ptr
	movl	4(%esp), %ecx			/ ecx = lock ptr
	xorl	%eax, %eax			/ eax = 0 (unheld adaptive)
	lock
	cmpxchgl %edx, M_OWNER(%ecx)		/ CF = success ? 0 : 1
	sbbl	%eax, %eax			/ eax = success ? 0 : -1
	incl	%eax				/ eax = success ? 1 : 0
	ret
.mutex_adaptive_tryenter_486_end:

	SET_SIZE(mutex_adaptive_tryenter)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
mutex_exit(kmutex_t *lp)
{}

void
mutex_exit_nofault(void)		/* label where fault might occur */
{}

/* ARGSUSED */
void
mutex_exit_fault(kmutex_t *lp)		/* label where fault continues */
{}

#else

	ENTRY(mutex_exit)
	/*
	 * We must be sure this is an adaptive mutex (or call the right
	 * function to handle it), but we also want to be sure the current
	 * thread really owns the mutex.  We assume the mutex is adaptive,
	 * and if the owner isn't right, go to C and let it do the type check.
	 * If it is adaptive and not owned, the C code will panic.
	 */
	movl	4(%esp), %eax
	movl	%gs:CPU_THREAD, %edx
	xorl	%ecx, %ecx
	cmpl	%edx, M_OWNER(%eax)
	jne	mutex_vector_exit	/ wrong type or wrong owner
	movl	%ecx, M_OWNER(%eax)	/ clear owner AND lock

	/
	/ Check the waiters field.
	/
	/ As soon as the owner/lock was cleared above, the mutex could be
	/ deallocated by another CPU, so the load of the waiters field
	/ could fault.  The trap code detects data faults that occur with 
	/ the PC at mutex_exit_nofault and continues at mutex_exit_fault.
	/
	ALTENTRY(mutex_exit_nofault)
	movl	M_WAITERS(%eax), %ecx	/ struct adaptive mutex is defined
					/ such that M_WAITERS is long aligned
	testl	$0xffff, %ecx		/ check for waiting threads
	jnz	.do_wakeup		/ lock wanted - do wakeup
	ret
.do_wakeup:
	pushl	%ecx
	pushl	%eax
	call	mutex_adaptive_release	
	addl	$8, %esp
	ret
	SET_SIZE(mutex_exit)

	/
	/ Continue here after getting fault on checking waiters field.
	/ Just return, since the fault indicates the mutex must've been freed.
	/
	ENTRY(mutex_exit_fault)
	ret
	SET_SIZE(mutex_exit_fault)

#endif	/* lint */

/*
 * asm_mutex_spin_enter(kmutex_t *)
 *
 * For use by assembly interrupt handler only.
 * Does not change spl, since the interrupt handler is assumed to be
 * running at high level already.
 * Traps may be off, so cannot panic.
 * Does not keep statistics on the lock.
 *
 * Entry:	%l6 - points to mutex
 * 		%l7 - address of call (returns to %l7+8)
 * Uses:	%l6, %l5
 */
#if !(defined(lint) || defined(__lint))

	ENTRY_NP(asm_mutex_spin_enter)
	/ XXX only needed by high priority devices - skip for now
	ret
	SET_SIZE(asm_mutex_spin_enter)

#endif	/* lint */

/*
 * asm_mutex_spin_exit(kmutex_t *)
 *
 * For use by assembly interrupt handler only.
 * Does not change spl, since the interrupt handler is assumed to be
 * running at high level already.
 *
 * Entry:	%l6 - points to mutex
 * 		%l7 - address of call (returns to %l7+8)
 * Uses:	none
 */
#if !(defined(lint) || defined(__lint))

	ENTRY_NP(asm_mutex_spin_exit)
	/ XXX only needed by high priority devices - skip for now
	ret
	SET_SIZE(asm_mutex_spin_exit)

#endif	/* lint */

/*
 * _get_pc()
 *
 * return the return address from a procedure call
 */

#if defined(lint) || defined(__lint)

greg_t
_get_pc(void)
{ return (0); }

#else	/* lint */

	ENTRY(_get_pc)
	movl	(%esp),%eax
	ret
	SET_SIZE(_get_pc)

#endif	/* lint */

/*
 * _get_sp()
 *
 * get the current stack pointer.
 */

#if defined(lint) || defined(__lint)

greg_t
_get_sp(void)
{ return (0); }

#else	/* lint */

	ENTRY(_get_sp)
	movl	%esp,%eax
	ret
	SET_SIZE(_get_sp)

#endif	/* lint */

/*
 * lock_mutex_flush()
 *
 * guarantee writes are flushed.
 */

#if defined(lint) || defined(__lint)

void
lock_mutex_flush(void)
{ return; }

#else	/* lint */

	ENTRY(lock_mutex_flush)
	lock
	xorl	$0, (%esp) /	flush the write buffer
	ret
	SET_SIZE(lock_mutex_flush)

#endif	/* lint */

/*
 * thread_onproc()
 * Set thread in onproc state for the specified CPU.
 * Also set the thread lock pointer to the CPU's onproc lock.
 * Since the new lock isn't held, the store ordering is important.
 * If not done in assembler, the compiler could reorder the stores.
 */
#if defined(lint) || defined(__lint)

void
thread_onproc(kthread_id_t t, cpu_t *cp)
{
	t->t_state = TS_ONPROC;
	t->t_lockp = &cp->cpu_thread_lock;
}

#else	/* lint */

	ENTRY(thread_onproc)
	movl	4(%esp), %eax
	movl	8(%esp), %ecx
	addl	$CPU_THREAD_LOCK, %ecx	/ pointer to disp_lock while running
	movl	$ONPROC_THREAD, T_STATE(%eax)	/ set state to TS_ONPROC
	movl	%ecx, T_LOCKP(%eax)	/ store new lock pointer
	ret
	SET_SIZE(thread_onproc)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
hat_mlist_enter(struct page *pp)
{}

#else	/* lint */

	.globl	hat_page_lock

	.text

	.globl	i486mmu_mlist_exit
	.globl	hat_page_lock
	.globl	hat_mlist_enter
	.globl	hat_mlist_tryenter
	.globl	hat_mlist_exit
	.globl	i386mmu_mlist_enter
	.globl	i386mmu_mlist_tryenter
	.globl	i386mmu_mlist_exit
	.globl	cpuarchtype

	/ The following bits are used in pp->p_inuse

	.globl	hat_mlist_enter
	/ bit 0 inuse bit
	/ bit 1 wanted bit
	/ bit 2 if set indicates an hme has to be dropped of pp->p_mapping
hat_mlist_enter:
	cmpw	$I86_386_ARCH, cpuarchtype
	je	call_i386_mlist_enter
i486_mlist_enter:
	movl	4(%esp), %eax
	leal	PP_INUSE(%eax), %ecx	/ pp->p_inuse
pp_inuse_maybe_cleared:
	lock
	btsw	$P_INUSE, (%ecx)
	jc	pp_inuse_set
	ret

pp_inuse_set:
	lock
	btsw	$P_WANTED, (%ecx)	/ set the wanted bit

	pushl	$hat_page_lock
	call	mutex_enter
	addl	$0x04, %esp

	movl	4(%esp), %eax
	leal	PP_INUSE(%eax), %ecx	/ pp->p_inuse
	btw	$P_INUSE, (%ecx)
	jc	pp_inuse_stillset	/ make sure lock bit is still set

	pushl	$hat_page_lock
	call	mutex_exit
	addl	$0x04, %esp
	jmp	i486_mlist_enter

pp_inuse_stillset:
	btw	$P_WANTED, (%ecx)	/ make sure wanted is still set
					/ mlist_exit could have cleared it and
					/ another guy could have set inuse
	jc	i486_mlist_enter_sleep

	pushl	$hat_page_lock
	call	mutex_exit
	addl	$0x04, %esp
	jmp	i486_mlist_enter

i486_mlist_enter_sleep:
	movl	4(%esp), %eax
	pushl	$hat_page_lock
	leal	PP_CV(%eax), %eax
	pushl	%eax
	call	cv_wait
	addl	$0x08, %esp

	pushl	$hat_page_lock
	call	mutex_exit
	addl	$0x04, %esp
	jmp	i486_mlist_enter	

call_i386_mlist_enter:
	movl	$i386mmu_mlist_enter, %eax
	jmp	*%eax
#endif	/* lint */

	

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
hat_mlist_exit(struct page *pp)
{}

#else	/* lint */

	.globl	hat_mlist_exit
hat_mlist_exit:
	cmpw	$I86_386_ARCH, cpuarchtype
	je	call_i386_mlist_exit
i486_mlist_exit:
	movl	4(%esp), %eax
	leal	PP_INUSE(%eax), %ecx	/ pp->p_inuse
	lock
	btrw	$P_HMEUNLOAD,(%ecx)		/ test for hmeunload bit
	jc	hme_unload_set		/ if not equal its set call c function
	xorl	%eax, %eax
	xorl	%edx, %edx
	orb	$P_INUSE_VALUE, %al
	lock
	.byte 0x0f, 0xb0, 0x11		/ cmpxchgb %edx, (%ecx)
	jne	pp_wanted_set		/ if equal we cleared p_inuse
	ret
hme_unload_set:
	movl	4(%esp), %eax
	pushl	%eax
	call 	i486mmu_mlist_exit	/ c function to drop hme from
					/ pp->p_mapping list
	addl	$0x04, %esp
	jmp	i486_mlist_exit
	
pp_wanted_set:
	andb	$P_HMEUNLOAD_VALUE, %al
	jne	i486_mlist_exit
	
	pushl	$hat_page_lock	/ wanted was set, we need to get 
					/ i86mmu_lock before we could signal
	call	mutex_enter
	addl	$0x04, %esp

	movl	4(%esp), %eax
	leal	PP_INUSE(%eax), %ecx	/ pp->p_inuse
	xorl	%eax, %eax
	xorl	%edx, %edx
	orb	$P_INUSE_WANTED_VALUE, %al	
	lock
	.byte 0x0f, 0xb0, 0x11		/ cmpxchgb %edx, (%ecx)
	je	i486_mlist_exit_wakeup 
	pushl	$hat_page_lock	/ We could not clear wanted and inuse
					/ hmeunload bit could have been set
	call	mutex_exit
	addl	$0x04, %esp
	jmp	i486_mlist_exit

i486_mlist_exit_wakeup:
	movl	4(%esp), %eax
	leal	PP_CV(%eax), %eax
	pushl	%eax
	call	cv_broadcast
	addl	$0x04, %esp

	pushl	$hat_page_lock
	call	mutex_exit
	addl	$0x04, %esp
	ret
call_i386_mlist_exit:
	movl	$i386mmu_mlist_exit, %eax
	jmp	*%eax
#endif	/* lint */
	
	
		
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
hat_mlist_tryenter(struct page *pp)
{}

#else	/* lint */

	.globl	hat_mlist_tryenter
hat_mlist_tryenter:
	cmpw	$I86_386_ARCH, cpuarchtype
	je	call_i386_mlist_tryenter
	movl	4(%esp), %eax
	leal	PP_INUSE(%eax), %ecx	/ pp->p_inuse
	lock
	btsw	$P_INUSE, (%ecx)
	jc	tryenter_pp_inuse_set
	movl	$0x01, %eax
	ret

tryenter_pp_inuse_set:
	xorl	%eax, %eax
	ret
call_i386_mlist_tryenter:
	movl	$i386mmu_mlist_tryenter, %eax
	jmp	*%eax
#endif	/* lint */
	

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
set_p_hmeunload(struct page *pp)
{}

#else	/* lint */
	.globl	set_p_hmeunload
set_p_hmeunload:
	movl	4(%esp), %eax
	leal	PP_INUSE(%eax), %eax
	lock
	btsw	$P_HMEUNLOAD, (%eax)
	ret

#endif	/* lint */
