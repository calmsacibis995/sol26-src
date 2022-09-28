/*
 * Copyright (c) 1994,1996 by Sun Microsystems Inc.
 * All rights reserved.
 */

#ident	"@(#)bcmp.c	1.8	96/09/20 SMI"

#include <sys/salib.h>

int
bcmp(register char *s1, register char *s2, register int len)
{
	while (len--)
		if (*s1++ != *s2++)
			return (1);
	return (0);
}
