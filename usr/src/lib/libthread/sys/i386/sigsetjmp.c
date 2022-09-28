#pragma ident "@(#)sigsetjmp.c	1.2	95/09/08 SMI"

#ifdef __STDC__
#pragma weak sigsetjmp  = _sigsetjmp
#pragma weak siglongjmp = _siglongjmp

#pragma weak _ti_sigsetjmp  = _sigsetjmp
#pragma weak _ti_siglongjmp = _siglongjmp
#endif /* __STDC__ */

#include "libthread.h"
#include <sys/ucontext.h>
#include <sys/types.h>
#include <setjmp.h>

/*
 * _sigsetjmp() sets up the jmp buffer so that on a subsequent _siglongjmp()
 * to this buffer, control first goes to __thr_jmpret() below. The stack
 * has been carefully crafted so that on return from __thr_jmpret(), the
 * thread returns to the caller of _sigsetjmp(), at the place right after
 * the call. __thr_jmpret() is needed for correct switching of signal masks
 * in an MT process.
 */
static int
__thr_jmpret(int val, sigjmp_buf env)
{
	register ucontext_t *ucp = (ucontext_t *)env;

	if (ucp->uc_flags & UC_SIGMASK)
		thr_sigsetmask(SIG_SETMASK, &(ucp->uc_sigmask), NULL);
	return (val);
}

/*
 * _sigsetjmp() for threads is a C routine, unlike the libc version. which is
 * an assembly routine. It is different from the libc version in two main areas:
 *
 *	1. Since this is a C routine, the caller's fp has to be stored
 *	2. Since this is for threads, the thread (not LWP) mask has to be saved
 * The frame pointer has to be stored correctly in EBP, so that __thr_jmpret()
 * above has the correct frame pointer on entry. Also, the EIP, UESP, and return
 * pc, etc. are carefully stored as follows:
 * 	EBP <- *fp / fp points to a word containing the caller's frame pointer.
 *		   / Since __thr_jmpret() is a C routine, it will expect its
 *		   / EBP on entry to be the caller's fp. This assignment ensures
 *		   / this.
 *	EIP  <- &__thr_jmpret / ensure that longjmp first jumps to __thr_jmpret
 *	UESP <- fp+4   / pop the fp to get at stack as it looks on entry
 *	EDX  <- *(fp+4) / first word on stack is the return pc; store it in a
 *		       / temporary register that _siglongjmp() can read.
 */
int
_sigsetjmp(sigjmp_buf env, int savemask)
{
	ucontext_t *ucp = (ucontext_t *)env;
	greg_t *cpup;
	char *fp;

	fp = _getfp();	/* this routine's frame pointer */
	ucp->uc_flags = UC_ALL;

	/*
	 * get the current context and store in jmpbuf. ucp points to jmpbuf
	 */
	__getcontext(ucp);

	/*
	 * Then modify the context/jmpbuf appropriately for the longjmp()
	 * to work so that the longjmp switch occurs to the __thr_jmpret()
	 * routine above, and *then* returns to the caller of _sigsetjmp().
	 */
	if (!savemask)
		ucp->uc_flags &= ~UC_SIGMASK;
	/*
	 * Always save the thread mask in the ucontext. In an MT process, a
	 * thread's context includes the thread mask, not the system known
	 * LWP mask.
	 */
	ucp->uc_sigmask = curthread->t_hold;
	cpup = (greg_t *)&ucp->uc_mcontext.gregs;
	cpup[EBP] = *((greg_t *)fp);
	cpup[EIP] = (greg_t)&__thr_jmpret;
	cpup[UESP] = (greg_t)(fp + 4);
	cpup[EDX] = *((greg_t *)(fp + 4));
	return (0);
}

void
_siglongjmp(sigjmp_buf env, int val)
{
	ucontext_t ucl = *((ucontext_t *)env);
	register greg_t *cpup;
	long *sp;

	/*
	 * First empty the signal mask. It is being passed to setcontext
	 * below which impacts the LWP mask. The signal mask stored in the
	 * jump buf is not touched since "ucl" is a local copy of the jumpbuf
	 * The signal mask is switched, if necessary, in __thr_jmpret() which
	 * has access to the jump buf, via one of its arguments.
	 */
	sigemptyset(&ucl.uc_sigmask);
	if (ucl.uc_flags & UC_SIGMASK) {
		/*
		 * If the jmp buf has a saved signal mask, the current mask
		 * cannot be changed until the longjmp to the new context
		 * occurs. Until then, to make the signal mask change atomic
		 * with respect to the switch, mask all signals on the current
		 * thread, which flushes all signals for this thread. Now,
		 * this thread should not receive any signals until the mask
		 * switch occurs in __thr_jmpret().
		 */
		thr_sigsetmask(SIG_SETMASK, &_totalmasked, NULL);
	}
	cpup = (greg_t *)&ucl.uc_mcontext.gregs;

	/*
	 * If val is non-zero, store it in the ucontext's EAX register to
	 * simulate a return from __thr_jmpret() (i.e. _sigsetjmp()) with this
	 * value. Since cpup points to a local copy, the jmp buf is not touched
	 * and so a second call to _siglongjmp() with the same jmp buf works
	 * as expected.
	 */
	if (val)
		cpup[EAX] = val;
	else
		cpup[EAX] = 1;

	sp = (long *)cpup[UESP]; /* "sp" is the stack being switched to */

	/*
	 * Take the return pc for _sigsetjmp() (pointing to the caller of
	 * _sigsetjmp()) and store it into the first word that the stack
	 * points to. i386 calling convention expects the return address
	 * to be the first word pointed to by %esp on entry to a function.
	 */
	*(sp) = (long)cpup[EDX];

	/*
	 * The following two lines serve to dummy-up the stack for the call to
	 * __thr_jmpret() with arguments that it needs. The first word on the
	 * stack is the return pc, stored by the above line. The second word on
	 * the stack is the first argument, and the third word on the stack is
	 * the second argument. Since sp is a "long *", adding 1 and 2 works.
	 */
	*(sp+1) = cpup[EAX];
	*(sp+2) = (long)env;

	setcontext(&ucl);
}
