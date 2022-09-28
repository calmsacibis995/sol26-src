/* @(#)machlwp.c	1.10 95/08/04 SMI */

#ifdef __STDC__
	#pragma weak _lwp_makecontext = __lwp_makecontext
#endif / *__STDC__ */

#include "synonyms.h"
#include <sys/stack.h>
#include <sys/ucontext.h>
#include <sys/lwp.h>

#undef SA
/*
 * _lwp_makecontext() should assign the new lwp's SP *within* and at top of
 * the address range [stk, stk + stksize]. The standard SA(X) macro
 * rounds *up* X, which could result in the stack aligned address falling
 * above this range. So define a local SA(X) macro which rounds down X.
 */
#define	SA(X) ((X) & ~(STACK_ALIGN-1))

void
_lwp_makecontext(ucp, func, arg, private, stk, stksize)
	ucontext_t *ucp;
	void (*func)();
	void *arg;
	void *private;
	caddr_t stk;
	size_t stksize;
{

	int *stack;
	extern void _lwp_exit();

	getcontext(ucp);	/* needed to load segment registers */
	ucp->uc_mcontext.gregs[EIP] = (int)func;

	stack = (int *) SA((int)((int)stk + stksize));
	*--stack = 0;
	*--stack = (int) arg;
	*--stack = (int)_lwp_exit;	/* return here if function returns */
	ucp->uc_mcontext.gregs[UESP] = (int) stack;

	if (ucp->uc_mcontext.gregs[GS] == 0) {
		ucp->uc_mcontext.gregs[GS] = __setupgs(private);

	}
}

static void **freegsmem = NULL;
static lwp_mutex_t freegslock;

__setupgs(void *private)
{
	int sel;
	void **priptr;
	
	_lwp_mutex_lock(&freegslock);
	if ((priptr = freegsmem) !=  NULL)
		freegsmem = *freegsmem;
	_lwp_mutex_unlock(&freegslock);
	if (priptr == NULL)
		priptr = (void **)malloc(2*sizeof(void *));
	if (priptr) {
		sel = __alloc_selector(priptr, 2*sizeof(void *));
		priptr[0] = private;
		priptr[1] = priptr;
		return (sel);
	}
	return (0);
}

__freegs(int sel)
{
	extern void *_getpriptr();
	void *priptr = _getpriptr();
	/*
	 * This is a gross hack that forces the runtime linker to create
	 * bindings for the following global functions. A better solution
	 * is to define static functions that do the same thing as these
	 * global functions. This doesn't work so well either because
	 * static functions can only be called from the file where they
	 * are defined. Scoping is probably the best solution. We could
	 * define new library private functions that can be used by anyone
	 * within this library, and these functions should then be resolved
	 * at link time.
	 * XXX
	 */
	extern int _lwp_mutex_lock(), _lwp_mutex_unlock();
	extern void __free_selector();
	int (*lockfunc)() = &_lwp_mutex_lock;
	int (*unlockfunc)() = &_lwp_mutex_unlock;
	void (*freeselfunc)() = &__free_selector;
	
	if (_lwp_self() != 1) {
		(*lockfunc)(&freegslock);
		*(void **)priptr = freegsmem;
		freegsmem = (void **)priptr;
		(*unlockfunc)(&freegslock);
	}
	(*freeselfunc)(sel);
}

void
_lwp_freecontext(ucp)
	ucontext_t *ucp;
{
	if (ucp->uc_mcontext.gregs[GS] != 0) {
		__freegs(ucp->uc_mcontext.gregs[GS]);
		ucp->uc_mcontext.gregs[GS] = 0;
	}
}
