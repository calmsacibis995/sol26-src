/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)setitimer.c	1.1	96/05/09 SMI"

#ifndef LINT
static char rcsid[] = "$Id: setitimer.c,v 8.1 1994/12/15 06:23:51 vixie Exp $";
#endif

/*
 * Setitimer emulation routine for UNICOS BIND.
 */
#if !defined(_CRAY)
int __bindcompat_setitimer;
#else
#include <sys/time.h>

int
__setitimer(int which, const struct itimerval *value,
	    struct itimerval *ovalue)
{
	if (alarm(value->it_value.tv_sec) >= 0)
		return (0);
	else
		return (-1);
}
#endif
