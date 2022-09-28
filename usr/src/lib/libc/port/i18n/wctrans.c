/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)wctrans.c 1.2	96/07/02  SMI"

#include <wctype.h>
#include <string.h>
#include <sys/localedef.h>

wctrans_t
__wctrans_std(_LC_ctype_t *hdl, const char *name)
{
	int i;

	for (i = 1; i <= hdl->ntrans; i++)
		if (strcmp(name, hdl->transname[i].name) == 0)
			return (i);

	return (0);
}

wctrans_t
wctrans(const char *name)
{
	return (METHOD(__lc_ctype, wctrans)(__lc_ctype, name));
}
