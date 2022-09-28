/*
 * Copyright (c) 1994,1996 by Sun Microsystems Inc.
 * All rights reserved.
 */

#ident	"@(#)bzero.c	1.1	96/09/20 SMI"

#include <sys/salib.h>

void
bzero(register char *p, register int n)
{
	register char zeero = 0;

	while (n > 0)
		*p++ = zeero, n--;	/* Avoid clr for 68000, still... */
}
