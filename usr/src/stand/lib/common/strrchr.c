/*
 * Copyright (c) 1994,1996 by Sun Microsystems Inc.
 * All rights reserved.
 */

#ident	"@(#)strrchr.c	1.1	96/09/20 SMI"

#include <sys/salib.h>

/*
 * Return the ptr in sp at which the character c last
 * appears, or NULL if not found.
 */
char *
strrchr(register char *sp, char c)
{
	register char *r;

	r = '\0';
	do {
		if (*sp == c)
			r = sp;
	} while (*sp++);
	return (r);
}
