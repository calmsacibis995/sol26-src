/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__wcstombs_euc.c 1.2	96/07/02  SMI"

#include <sys/localedef.h>
#include <limits.h>

size_t
__wcstombs_euc(_LC_charmap_t * hdl, char *s, const wchar_t *pwcs, size_t n)
{
	int	val = 0;
	int	total = 0;
	char	temp[MB_LEN_MAX];
	int	i;

	for (;;) {
		if (s && (total == n))
			break;
		if (*pwcs == 0) {
			if (s)
				*s = '\0';
			break;
		}
		if ((val = wctomb(temp, *pwcs++)) == -1)
			return (val);
		total += val;
		if (s && (total > n)) {
			total -= val;
			break;
		}
		if (s != 0) {
			for (i = 0; i < val; i++)
				*s++ = temp[i];
		}
	}
	return (total);
}
