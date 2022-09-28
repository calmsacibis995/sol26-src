/*
 * Copyright (c) 1994,1996 by Sun Microsystems Inc.
 * All rights reserved.
 */

#ident	"@(#)strncat.c	1.1	96/09/20 SMI"

#include <sys/salib.h>

/*
 * Concatenate s2 to s1, truncating or null-padding to always
 * copy n bytes. Return s1.
 */
char *
strncat(register char *s1, register char *s2, register int n)
{
	char *os1 = s1;

	while (*s1++)
		;
	--s1;
	while (--n > 0 && (*s1++ = *s2++))
		;
	if (n > 0)
		while (--n >= 0)
			*s1++ = '\0';
	else
		*s1 = '\0';
	return (os1);
}
