/*
 *      Copyright (c) 1996 by Sun Microsystems, Inc.
 *      All Rights Reserved.
 */

#pragma ident	"@(#)__towctrans_bc.c	1.3	96/09/18 SMI"

#include <wctype.h>
#include <sys/localedef.h>

wint_t
__towctrans_bc(_LC_ctype_t *hdl, wint_t eucpc, wctrans_t ind)
{
	wint_t	nwc1, nwc2;

	if ((nwc1 = (wint_t) _eucpctowc(hdl->cmapp, (wchar_t) eucpc)) == WEOF)
		return (eucpc);
	nwc2 = METHOD_NATIVE(hdl, towctrans)(hdl, nwc1, ind);
	return ((wint_t) _wctoeucpc(hdl->cmapp, (wchar_t) nwc2));
}
