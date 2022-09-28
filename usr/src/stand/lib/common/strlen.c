/*
 * Copyright (c) 1994,1996 by Sun Microsystems Inc.
 * All rights reserved.
 */

#ident	"@(#)strlen.c	1.1	96/09/20 SMI"

#include <sys/salib.h>

int
strlen(register char *s)
{
	register int n;

	n = 0;
	while (*s++)
		n++;
	return (n);
}
