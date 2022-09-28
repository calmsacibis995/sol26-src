/*
 * Copyright (c) 1994,1996 by Sun Microsystems Inc.
 * All rights reserved.
 */

#ident	"@(#)strchr.c	1.1	96/09/20 SMI"

#include <sys/salib.h>

/*
 * Return the ptr in sp at which the character c first appears;
 * NULL if not found
 */
char *
strchr(register char *sp, register char c)
{
	do {
		if (*sp == (char)c)
			return ((char *)sp);
	} while (*sp++);
	return (0);
}
