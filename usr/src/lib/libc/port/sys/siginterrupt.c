/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)siginterrupt.c	1.2	96/10/07 SMI"

#ifdef __STDC__
#pragma weak siginterrupt = _siginterrupt
#endif


#include "synonyms.h"

#include <errno.h>
#include <signal.h>

/*
 * Check for valid signal number
 */
#define	CHECK_SIG(s, code) \
	if ((s) <= 0 || (s) >= NSIG) { \
		errno = EINVAL; \
		return (code); \
	}

int
siginterrupt(int sig, int flag)
{
	struct sigaction act;

	CHECK_SIG(sig, (int)SIG_ERR);

	sigaction(sig, NULL, &act);
	if (flag)
		act.sa_flags &= ~SA_RESTART;
	else
		act.sa_flags |= SA_RESTART;
	return (sigaction(sig, &act, NULL));
}
