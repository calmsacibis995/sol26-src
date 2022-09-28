/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)sigaction.c	1.11	96/10/15 SMI"	/* SVr4 1.5	*/

#include "synonyms.h"
#include <signal.h>
#include <errno.h>
#include <siginfo.h>
#include <ucontext.h>

void (*_siguhandler[NSIG])() = { 0 };

static void
sigacthandler(sig, sip, uap)
	int sig;
	siginfo_t *sip;
	ucontext_t *uap;
{
	(*_siguhandler[sig])(sig, sip, uap);

	/*
	 * If this is a floating point exception and the queue
	 * is non-empty, pop the top entry from the queue.  This
	 * is to maintain expected behavior.
	 */
	if ((sig == SIGFPE) && uap->uc_mcontext.fpregs.fpu_qcnt) {
		fpregset_t *fp = &uap->uc_mcontext.fpregs;

		if (--fp->fpu_qcnt > 0) {
			unsigned char i;
			struct fq *fqp;

			fqp = fp->fpu_q;
			for (i = 0; i < fp->fpu_qcnt; i++)
				fqp[i] = fqp[i+1];
		}
	}

	setcontext(uap);
}

_libc_sigaction(sig, nact, oact)
	int sig;
	const struct sigaction *nact;
	struct sigaction *oact;
{
	struct sigaction tact;
	register struct sigaction *tactp;
	void (*ohandler)();

	if (sig <= 0 || sig >= NSIG) {
		errno = EINVAL;
		return (-1);
	}

	ohandler = _siguhandler[sig];

	if (tactp = (struct sigaction *)nact) {
		tact = *nact;
		tactp = &tact;
		/*
		 * To be compatible with the behavior of SunOS 4.x:
		 * If the new signal handler is SIG_IGN or SIG_DFL,
		 * do not change the signal's entry in the handler array.
		 * This allows a child of vfork(2) to set signal handlers
		 * to SIG_IGN or SIG_DFL without affecting the parent.
		 */
		if (tactp->sa_handler != SIG_DFL &&
		    tactp->sa_handler != SIG_IGN) {
			_siguhandler[sig] = tactp->sa_handler;
			tactp->sa_handler = sigacthandler;
		}
	}

	if (__sigaction(sig, tactp, oact) == -1) {
		_siguhandler[sig] = ohandler;
		return (-1);
	}

	if (oact && oact->sa_handler != SIG_DFL && oact->sa_handler != SIG_IGN)
		oact->sa_handler = ohandler;

	return (0);
}
