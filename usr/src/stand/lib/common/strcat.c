/*
 * Copyright (c) 1994,1996 by Sun Microsystems Inc.
 * All rights reserved.
 */

#ident	"@(#)strcat.c	1.1	96/09/20 SMI"

#include <sys/salib.h>

char *
strcat(register char *s1, register char *s2)
{
	char *os1 = s1;

	while (*s1++)
		;
	--s1;
	while (*s1++ = *s2++)
		;
	return (os1);
}
