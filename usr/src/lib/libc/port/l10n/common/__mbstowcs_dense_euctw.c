/*
 *      Copyright (c) 1996 by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma	ident	"@(#)__mbstowcs_dense_euctw.c	1.3	96/07/02 SMI"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/localedef.h>

size_t
__mbstowcs_dense_euctw(_LC_charmap_t *hdl, wchar_t *pwcs, const char *s,
				size_t n)
{

	int	val;
	size_t	i, count;

	if (pwcs == 0)
		count = strlen(s);
	else
		count = n;

	for (i = 0; i < count; i++) {
		/* if ((val = mbtowc(pwcs, s, MB_CUR_MAX)) == -1) */
		if ((val = __mbtowc_dense_euctw(hdl, pwcs, s, MB_CUR_MAX))
						== -1)
			return (val);
		if (val == 0)
			break;
		s += val;
		if (pwcs != NULL)
			pwcs++;
	}
	return (i);
}
