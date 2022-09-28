/*
 * Copyright (c) 1994,1996 by Sun Microsystems Inc.
 * All rights reserved.
 */

#ident	"@(#)strncmp.c	1.1	96/09/20 SMI"

#include <sys/salib.h>

int
strncmp(register char *s1, register char *s2, register int n)
{
	while (--n >= 0 && *s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
	return (n < 0 ? 0 : *s1 - *--s2);
}
