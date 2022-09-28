/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */

#pragma ident "@(#)machlwp.c 1.4	96/01/04 SMI"

#ifdef __STDC__
	#pragma weak _lwp_makecontext = __lwp_makecontext
#endif / *__STDC__ */

#include "synonyms.h"
#include <sys/stack.h>
#include <sys/ucontext.h>
#include <sys/lwp.h>
#include <sys/reg.h>

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

	ucp->uc_mcontext.gregs[R_PC] = (int)func;
	ucp->uc_mcontext.gregs[R_R3] = (int)arg;
	ucp->uc_mcontext.gregs[R_R1] = SA((int)((int)stk + stksize));
	ucp->uc_mcontext.gregs[R_R14] = (int)private;
	ucp->uc_mcontext.gregs[R_LR] = (int)_lwp_exit;
}
