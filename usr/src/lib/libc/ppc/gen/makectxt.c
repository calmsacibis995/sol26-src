/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */
#pragma ident "@(#)makectxt.c 1.6       96/01/04 SMI"
#ifdef __STDC__
	#pragma weak makecontext = _makecontext
#endif
#include "synonyms.h"
#include <stdarg.h>
#include <ucontext.h>
#include <sys/stack.h>
#include <sys/reg.h>

void
#ifdef __STDC__
makecontext(ucontext_t *ucp, void (*func)(), int argc, ...)
#else
makecontext(ucp, func, argc, va_alist)
ucontext_t *ucp;
void (*func)();
int argc;
va_dcl
#endif
{

	register greg_t *reg;
	int *sp, *tsp;
	int argno, ofargs;
	va_list ap;

	reg = ucp->uc_mcontext.gregs;
	reg[R_PC] = (greg_t)func;

	sp = (int *)ucp->uc_stack.ss_sp;
	*sp-- = (int)ucp->uc_link;		/* sp-- stack grows down */

#ifdef __STDC__
	va_start(ap, ...);
#else
	va_start(ap, va_alist);
#endif
	argno = 0;
	if ((ofargs = (argc - 8)) > 0)
		sp -= ofargs;	/* Set aside space for overflow arguments */
	tsp = sp;

	/*
	 * Copy the parameters, the first 8 to registers and the
	 * remaining to the stack.
	 */

	while (argno < argc) {
		if (argno < 8)
			reg[R_R3 + argno] = va_arg(ap, int);
		else
			*tsp++ =  va_arg(ap, int);
		argno++;
	}

#ifdef	XXXPPC
	sp--;				/* `hidden' param	*/
	sp -= 16;			/* XXXPPC Register save area   */
#endif

	sp = (int*)SA((int)sp+MINFRAME);	/* Align sp */
	*sp = 0;				/* Null back-chain */


	ucp->uc_stack.ss_sp = (char *) sp;
	reg[R_R1] = (int)sp;		/* sp (when done)	*/
	reg[R_LR] = (int)setcontext;	/* return pc		*/
}
