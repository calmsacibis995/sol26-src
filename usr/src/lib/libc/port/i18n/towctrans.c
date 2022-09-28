/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)towctrans.c 1.3	96/07/15  SMI"

#include <wctype.h>
#include <string.h>
#include <sys/localedef.h>

wint_t
__towctrans_std(_LC_ctype_t *hdl, wint_t wc, wctrans_t ind)
{
	if (!hdl || (wc == (wint_t)-1) || (ind == 0) ||
		(wc > hdl->transname[ind].tmax) ||
		(wc < hdl->transname[ind].tmin))
			return (wc);

	return (hdl->transtabs[hdl->transname[ind].index]
				[wc - (hdl->transname[ind].tmin)]);
}


wint_t
towctrans(wint_t wc, wctrans_t index)
{
	return (METHOD(__lc_ctype, towctrans)(__lc_ctype, wc, index));
}
