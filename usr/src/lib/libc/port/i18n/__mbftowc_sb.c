/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)__mbftowc_sb.c 1.2	96/07/02  SMI"

#include <sys/localedef.h>

int
__mbftowc_sb(_LC_charmap_t * hdl, char *ts, wchar_t *wchar,
		int (*f)(), int *peekc)
{
	unsigned char *s = (unsigned char *)ts;
	int c;

	if ((c = (*f)()) < 0)
		return (0);

	*s = c;

	*wchar = (wchar_t)*s;
	return (1);

}
