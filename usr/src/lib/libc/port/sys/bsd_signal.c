/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)bsd_signal.c	1.2	96/06/07 SMI"

#ifdef __STDC__
#pragma weak bsd_signal = _bsd_signal
#endif

#include "synonyms.h"

#include <errno.h>
#include <signal.h>
#include <wait.h>

/*
 * Check for valid signal number
 */
#define	CHECK_SIG(s, code) \
	if ((s) <= 0 || (s) >= NSIG || (s) == SIGKILL || (s) == SIGSTOP) { \
		errno = EINVAL; \
		return (code); \
	}

void (*bsd_signal(int sig, void(*func)(int)))(int)
{
	struct sigaction nact;
	struct sigaction oact;

	CHECK_SIG(sig, SIG_ERR);

	nact.sa_handler = func;
	nact.sa_flags = SA_RESTART;
	(void) sigemptyset(&nact.sa_mask);
	(void) sigaddset(&nact.sa_mask, sig);

	if (sigaction(sig, &nact, &oact) == -1)
		return (SIG_ERR);

	return (oact.sa_handler);
}
