/*
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 */

#ident "@(#)prom_gettime.c	1.5	94/08/09 SMI"

#include <sys/promif.h>

#ifndef I386BOOT
#include <sys/promimpl.h>
#endif

/*
 * For SunMON PROMs we forge a timer as a simple counter
 * so that (at least) time is increasing ..
 */
u_int
prom_gettime(void)
{
	static int pretend = 0;

#ifdef I386BOOT
	static int subpretend = 0;

	if (++subpretend > 20) {
		pretend++;
		subpretend = 0;
	}
	return (pretend);
#else
	return (pretend++);
#endif
}
