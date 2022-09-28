/*
 *      Copyright (c) 1996 by Sun Microsystems, Inc.
 *      All Rights Reserved.
 */

#ident	"@(#)__mbstowcs_sb.c	1.4	96/09/23 SMI"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/localedef.h>

size_t
__mbstowcs_sb(_LC_charmap_t *hdl, wchar_t *pwcs, const char *s, size_t n)
{

	int	i;

	if (s == NULL)
		return (0);

	if (pwcs == 0)
		return (strlen(s));

	for (i = 0; i < n; i++) {
		*pwcs = (wchar_t)*s;
		pwcs++;

		if (*s == '\0')
			break;
		else
			s++;

	}
	return ((size_t)i);

}
