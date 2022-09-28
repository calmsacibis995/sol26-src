/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)sigstack.c	1.1	96/06/07 SMI"

#ifdef __STDC__
#pragma weak sigstack = _sigstack
#endif

#include <sys/types.h>
#include <sys/ucontext.h>
#include <signal.h>
#include <errno.h>

_sigstack(nss, oss)
struct sigstack *nss;
struct sigstack *oss;
{
	struct sigaltstack nalt;
	struct sigaltstack oalt;
	struct sigaltstack *naltp;

	if (nss) {
		/* Assumes stack growth is down */
		nalt.ss_sp = (char *)nss->ss_sp - SIGSTKSZ;
		nalt.ss_size = SIGSTKSZ;
		nalt.ss_flags = 0;
		naltp = &nalt;
	} else
		naltp = (struct sigaltstack *)0;

	if (sigaltstack(naltp, &oalt) < 0)
		return (-1);

	if (oss) {
		/* Assumes stack growth is down */
		oss->ss_sp = (char *)oalt.ss_sp + oalt.ss_size;
		oss->ss_onstack = ((oalt.ss_flags & SS_ONSTACK) != 0);
	}
	return (0);
}
