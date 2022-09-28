/*
 * Copyright (c) 1994,1996 by Sun Microsystems Inc.
 * All rights reserved.
 */

#ident	"@(#)strcmp.c	1.1	96/09/20 SMI"

#include <sys/salib.h>

/*
 * Compare strings:  s1>s2: >0  s1==s2: 0  s1<s2: <0
 */
int
strcmp(register char *s1, register char *s2)
{
	while (*s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
	return (*s1 - *--s2);
}
