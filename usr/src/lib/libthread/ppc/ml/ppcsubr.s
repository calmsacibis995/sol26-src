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

	.ident "@(#)ppcsubr.s 1.19	96/05/30 SMI"
#include <sys/asm_linkage.h>
#include <sys/trap.h>
#include <assym.s>
#include <thread.h>

#ifdef PIC
#include "PIC.h"
#endif


#define TLS_REG		%r2
#define	THREAD_REG	%r2

/* PROBE_SUPPORT begin */
/*
 * _thread_probe_getfunc()
 *
 * return the address of the current thread's tpdp
 * CAUTION: This code does not work if TLS is defined.
 */
	ENTRY(_thread_probe_getfunc)
#ifdef TLS
	mr 	%r3, TLS_REG		! disable if compiled with TLS
#else
	lwz	%r3, T_TPDP(THREAD_REG)	! return thread probe data
#endif
	blr
	SET_SIZE(_thread_probe_getfunc)
/* PROBE_SUPPORT end */


/*
 * getsp()
 *
 * return the current sp (for debugging)
 */
	ENTRY(_getsp)
	mr	%r3, %r1
	blr
	SET_SIZE(_getsp)

	ENTRY(_getfp)
	lwz	%r3, 0(%r1)
	blr
	SET_SIZE(_getfp)
/*
 * getpc()
 *
 * return the current pc of getpc()'s caller
 */
	ENTRY(_getpc)
	mflr	%r3
	blr
	SET_SIZE(_getpc)
/*
 * getcaller()
 *
 * return the pc of the calling point in the routine which called the routine
 * which called getcaller()
 */
	ENTRY(_getcaller)
	lwz	%r3, 0(%r1)
	lwz	%r3, 4(%r3)
	blr
	SET_SIZE(_getcaller)

/*
 * caller()
 *
 * return the address of our caller's caller.
 */
	ENTRY(caller)
	lwz	%r3, 0(%r1)
	lwz	%r3, 4(%r3)
	blr
	SET_SIZE(caller)

/*
 * _whereami()
 *
 * Like _getpc(), but the return value is passed by reference.
 *
 */
	ENTRY(_whereami)
	mflr	%r12
	stw	%r12, 0(%r3)
	blr
	SET_SIZE(_whereami)

/*
 * _tlsbase()
 *
 * returns the base address for a thread's TLS segment.
 */
	ENTRY(_tlsbase)
	mr	%r3, TLS_REG
	blr
	SET_SIZE(_tlsbase)
/*
 * _curthread()
 *
 * return the value of the currently active thread.
 */
	ENTRY(_curthread)
	mr	%r3, THREAD_REG
	blr
	SET_SIZE(_curthread)

/*
 * _flush_store()
 * 
 */
	ENTRY(_flush_store)
	eieio
	blr
	SET_SIZE(_flush_store)

/*
 * _save_thread(uthread_t *t)
 */
	ENTRY(_save_thread)
	lwz	%r4, 0(%r1)
	stw	%r4, T_SP(%r3)		! save sp of caller of _resume()
	lwz	%r5, 4(%r4)
	stw	%r5, T_PC(%r3)		! save pc of caller of _resume()
	stw	%r13, T_R13(%r3)
	stw	%r14, T_R14(%r3)
	stw	%r15, T_R15(%r3)
	stw	%r16, T_R16(%r3)
	stw	%r17, T_R17(%r3)
	stw	%r18, T_R18(%r3)
	stw	%r19, T_R19(%r3)
	stw	%r20, T_R20(%r3)
	stw	%r21, T_R21(%r3)
	stw	%r22, T_R22(%r3)
	stw	%r23, T_R23(%r3)
	stw	%r24, T_R24(%r3)
	stw	%r25, T_R25(%r3)
	stw	%r26, T_R26(%r3)
	stw	%r27, T_R27(%r3)
	stw	%r28, T_R28(%r3)
	stw	%r29, T_R29(%r3)
	stw	%r30, T_R30(%r3)
	stw	%r31, T_R31(%r3)
	mfcr	%r0
	stw	%r0, T_CR(%r3)
	blr
	SET_SIZE(_save_thread)

/*
 * _switch_stack(caddr_t stk, uthread_t *ot, uthread_t *t)
 *
 *  release curthread's t_lock,
 *  return old thread.
 *  and switch to new stack, "stk".
 */
	ENTRY(_switch_stack)
	mflr	%r0
	mr	%r1, %r3		! switch to new stack
	stwu	%r1, -32(%r1)
	stw	%r0, 36(%r1)
	stw	%r5, 8(%r1)
	addi	%r3, %r4, T_LOCK
#ifdef	PIC
	bl	_lwp_mutex_unlock@PLT
#else
	bl	_lwp_mutex_unlock	!_lwp_mutex_unlock(&t->t_lock)
#endif
	lwz	%r0, 36(%r1)
	lwz	%r3, 8(%r1)		! return t
	mtlr	%r0
	blr
	SET_SIZE(_switch_stack)	
/*
 * _init_cpu(t)
 *
 * set the initial cpu to point at the initial thread.
 */
	ENTRY(_init_cpu)
	mr	THREAD_REG, %r3
	blr
	SET_SIZE(_init_cpu)	
/*
 * _threadjmp(uthread_t *t, uthread_t *ot)
 *
 * jump to thread "t" and do some clean up if old thread "ot"
 * is a zombie.
 */
	ENTRY(_threadjmp)
#ifdef TLS
	mr	%r4, TLS_REG
	lwz	TLS_REG, T_TLS(%r3)
#else
	mr	THREAD_REG, %r3
#endif
	lwz	%r0, T_CR(%r3)
	mtcrf	0xff, %r0
	lwz	%r0, T_PC(%r3)  		! return pc
	lwz	%r1, T_SP(%r3)  		! return sp
	lwz	%r13, T_R13(%r3)
	lwz	%r14, T_R14(%r3)
	lwz	%r15, T_R15(%r3)
	lwz	%r16, T_R16(%r3)
	lwz	%r17, T_R17(%r3)
	lwz	%r18, T_R18(%r3)
	lwz	%r19, T_R19(%r3)
	lwz	%r20, T_R20(%r3)
	lwz	%r21, T_R21(%r3)
	lwz	%r22, T_R22(%r3)
	lwz	%r23, T_R23(%r3)
	lwz	%r24, T_R24(%r3)
	lwz	%r25, T_R25(%r3)
	lwz	%r26, T_R26(%r3)
	lwz	%r27, T_R27(%r3)
	lwz	%r28, T_R28(%r3)
	lwz	%r29, T_R29(%r3)
	lwz	%r30, T_R30(%r3)
	lwz	%r31, T_R31(%r3)
	mtlr	%r0
	mr	%r3, %r4
#ifdef	PIC
	b	_resume_ret@PLT			! _resume_ret(oldthread);
#else
	b	_resume_ret
#endif
	blr
	SET_SIZE(_threadjmp)
	

/*
 * _stack_switch(size_t stk)
 *
 * force the current thread to start running on the stack
 * specified by "stk".
 */
	ENTRY(_stack_switch)
	mr	%r1, %r3
	blr
	SET_SIZE(_stack_switch)

/*
 * _savefpu(t)
 *
 * save the current floating point state to thread "t".
 */
	ENTRY(_savefpu)
	stfd	%f14, T_F14(%r3)	! save non-volatiles
	mffs	%f14
	stfd	%f14, T_FPSCR(%r3)
	stfd	%f15, T_F15(%r3)
	stfd	%f16, T_F16(%r3)
	stfd	%f17, T_F17(%r3)
	stfd	%f18, T_F18(%r3)
	stfd	%f19, T_F19(%r3)
	stfd	%f20, T_F20(%r3)
	stfd	%f21, T_F21(%r3)
	stfd	%f22, T_F22(%r3)
	stfd	%f23, T_F23(%r3)
	stfd	%f24, T_F24(%r3)
	stfd	%f25, T_F25(%r3)
	stfd	%f26, T_F26(%r3)
	stfd	%f27, T_F27(%r3)
	stfd	%f28, T_F28(%r3)
	stfd	%f29, T_F29(%r3)
	stfd	%f30, T_F30(%r3)
	stfd	%f31, T_F31(%r3)
	li	%r4, 1
	stw	%r4, T_FPVALID(%r3)	! make valid
	blr
	SET_SIZE(_savefpu)

/*
 * _restorefpu(t)
 *
 * restore thread's "t" floating point state into the current cpu.
 */
	ENTRY(_restorefpu)
	lfd	%f14, T_FPSCR(%r3)
	mtfsf	0xff, %f14		! restore fpscr
	lfd	%f14, T_F14(%r3)
	lfd	%f15, T_F15(%r3)
	lfd	%f16, T_F16(%r3)
	lfd	%f17, T_F17(%r3)
	lfd	%f18, T_F18(%r3)
	lfd	%f19, T_F19(%r3)
	lfd	%f20, T_F20(%r3)
	lfd	%f21, T_F21(%r3)
	lfd	%f22, T_F22(%r3)
	lfd	%f23, T_F23(%r3)
	lfd	%f24, T_F24(%r3)
	lfd	%f25, T_F25(%r3)
	lfd	%f26, T_F26(%r3)
	lfd	%f27, T_F27(%r3)
	lfd	%f28, T_F28(%r3)
	lfd	%f29, T_F29(%r3)
	lfd	%f30, T_F30(%r3)
	lfd	%f31, T_F31(%r3)
	li	%r4, 0
	stw	%r4, T_FPVALID(%r3)	! make invalid
	blr
	SET_SIZE(_restorefpu)


/*
 * _getlwpfpu()
 *
 * returns 1 if lwp has initialized fpu.
 */

	ENTRY(_getlwpfpu)
	li	%r0, -1
	li	%r3, SC_GETLWPFPU
	sc
	blr
	SET_SIZE(_getlwpfpu)

/*
 * _thread_start()
 *
 * the current register set was crafted by _thread_call() to contain
 * an address of a function in register %r14 and its arg in register
 * %r15. thr_exit() is called if the procedure returns.
 */
	ENTRY(_thread_start)
#ifdef TLS
#ifdef PIC
	PIC_SETUP();
	mflr	%r10
	lwz	THREAD_REG, _thread@got(%r10)
	bl	_lwp_self@PLT
#else	/* !PIC */
	lis	THREAD_REG, _thread@ha
	ori	THREAD_REG, _thread@l
	bl	_lwp_self
#endif	/* PIC */
	add	THREAD_REG, TLS_REG, THREAD_REG 
	stw	%r3, T_LWPID(THREAD_REG)
#else	/* !TLS */
#ifdef	PIC
	bl	_lwp_self@PLT
#else	/* !PIC */
	bl	_lwp_self
#endif	/* PIC */
	stw	%r3, T_LWPID(THREAD_REG)
#endif	/* TLS */
	lwz	%r3, T_USROPTS(THREAD_REG)
	andi.	%r3, %r3, THR_BOUND
	beq+	1f
	bl	_sc_setup
1:	
	mr	%r3, %r15
	mtlr	%r14		! call func(arg);
	blrl

#ifdef	PIC
	bl	thr_exit@PLT	!destroy thread if it returns
#else	/* !PIC */
	bl	thr_exit	!destroy thread if it returns
#endif	/* PIC */
	SET_SIZE(_thread_start)
/*
 * lwp_terminate(thread)
 *
 * This is called when a bound thread does a thr_exit(). The
 * exiting thread is placed on deathrow and switches to a dummy stack.
 */
	.data
	.global _SHAREDFRAME;
	.align 3
	.skip 64*4
_SHAREDFRAME:
	.skip 24*4
	.text

	ENTRY(_lwp_terminate)
#ifdef PIC
	bl	_reapq_add@PLT
	PIC_SETUP();
	mflr	%r10
	lwz	%r1, _SHAREDFRAME@got(%r10) ! use SHAREDFRAME as stack
	lwz	%r3, _reaplockp@got(%r10)   ! %r3 points to _reaplockp
#else
	bl	_reapq_add		! _reapq_add(thread)
	lis	%r1, _SHAREDFRAME@ha	! stop running on thread's stack
	ori	%r1, _SHAREDFRAME@l	! use SHAREDFRAME as stack
	lis	%r3, _reaplockp@ha	! here, %r3 is the address of _reaplockp
	ori	%r3, _reaplockp@l
#endif
					! IMPORTANT NOTE:
					! after switching to this stack, no
					! subsequent calls should be such that
					! they use the stack - specifically
					! remove the possibility that the 
					! dynamic linker is called after the 
					! stack switch, to resolve symbols
					! like _lwp_mutex_unlock, _lwp_exit
					! and _reaplock which are referenced 
					! below. 
					! This is ensured by resolving them in
					! _t0init() and storing the addresses of
					! these symbols into libthread pointers
					! and de-referencing these pointers 
					! here.
					! Make sure that, in the future, if 
					! any other symbols are added in the 
					! code below besides these, they should
					! also have the matching libthread
					! pointers, initialized in _t0init().

	lwz	%r3, 0(%r3)		! read _reaplockp for _reaplock address
	li     	THREAD_REG, 0		! invalidate curthread by setting it to
					! zero so that a debugger knows that the
					! thread has disappeared.
#ifdef PIC
	PIC_SETUP();
	mflr	%r10
	lwz	%r6, _lwp_mutex_unlockp@got(%r10)
#else
	lis	%r6, _lwp_mutex_unlockp@ha
	ori	%r6, _lwp_mutex_unlockp@l
#endif
	lwz 	%r6, 0(%r6)		!
	mtlr	%r6
	blrl				! jump to _lwp_mutex_unlock()
					! Note that _lwp_exit() or 
					! _lwp_mutex_unlock(), etc. should never
					! result in calls to any functions that
					! are unbound, otherwise we have a 
					! deadlock, e.g. tracing, etc. If
					! _lwp_exit needs to be traced, trace it
					! in the kernel - not at user level.
#ifdef PIC
	PIC_SETUP();
	mflr	%r10
	lwz	%r6, _lwp_exitp@got(%r10)
#else
	lis	%r6, _lwp_exitp@ha
	ori	%r6, _lwp_exitp@l
#endif
	lwz 	%r6, 0(%r6)		! Read _lwp_exitp for addr. of _lwp_exit
	mtlr	%r6
	blr				! jump to _lwp_exit()
	SET_SIZE(_lwp_terminate)

/*
 * __getxregsize()
 *
 * return the size of the extra register state.
 */
	ENTRY(__getxregsize)
	li	%r3, 0
	blr
	SET_SIZE(__getxregsize)

/*
 * The following .init section gets called by crt1.s through _init(). It takes
 * over control from crt1.s by making sure that the return from _init() goes
 * to the _tcrt routine above.
 */
	.section	.init,ax
	.align  2
#if defined(PIC)
	bl	_t0init@PLT			! setup the primordial thread
#else
	bl	_t0init				! setup the primordial thread
#endif


/* Cancellation stuff */

/*
 * _ex_unwind(void (*func)(void *), void *arg)
 *
 * unwinds two frames and invokes "func" with "arg"
 * supposed to be in libC and libc.
 *
 * Before this call stack is - f4 f3 f2 f1 f0
 * After this call stack is -  f4 f3 f2 func (as if "call f1" is replaced
 *					      by the"call func" in f2)
 * The usage in practice is to call _ex_wind from f0 with argument as
 * _ex_unwind(f0, 0) ( assuming f0 does not take an argument )
 * So, after this call to _ex_unwind from f0 the stack will look like
 *
 * 	f4 f3 f2 f0 - as if f2 directly called f0 (f1 is removed)
 */

	ENTRY(_ex_unwind_local)
	lwz	%r1, 0(%r1)		! pop f0's frame
	mtctr	%r3			! prepare to call (*func)
	lwz	%r1, 0(%r1)		! pop f1's frame
	mr	%r3, %r4		! pass argument "arg" as first argument
	lwz	%r0, 4(%r1)		! f1's return address to f2
	mtlr	%r0			!     needs to be in the link register
	bctr				! call func
	SET_SIZE(_ex_unwind_local)

/*
 * _ex_clnup_handler(void *arg, void  (*clnup)(void *),	
 *					void (*tcancel)(void))
 *
 * This function goes one frame down, execute the cleanup handler with
 * argument arg and invokes func.
 */
	ENTRY(_ex_clnup_handler)
	lwz	%r1, 0(%r1)		! pop one stack frame
	stwu	%r1, -SA(MINFRAME+4)(%r1) ! create a stack frame for us
	stw	%r5, MINFRAME(%r1)	! save tcancel for use after clnup
	mtctr	%r4			! prepare to call clnup
	bctrl				! call clnup
	lwz	%r5, MINFRAME(%r1)	! restore tcancel
	addi	%r1, %r1, SA(MINFRAME+4) ! pop our frame
	lwz	%r0, 4(%r1)		! pick up return address
	mtctr	%r5
	mtlr	%r0			! tcancel return point
	bctr				! call tcancel
	SET_SIZE(_ex_clnup_handler)

/*
 * _tcancel_all()
 * It jumps to _t_cancel with caller's fp
 */
	ENTRY(_tcancel_all)
	lwz	%r3, 0(%r1)		! fp as first argument
/*
 * NOTE: we use "b" and not "bl" so that _t_cancel will return to our caller
 */
#if defined(PIC)
	b	_t_cancel@PLT		! call t_tcancel(fp)
#else
	b	_t_cancel		! call t_tcancel(fp)
#endif
	SET_SIZE(_tcancel_all)
