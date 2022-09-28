/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)__towupper_bc.c	1.4	96/09/18 SMI"

#include <wchar.h>
#include <sys/localedef.h>

wint_t
__towupper_bc(_LC_ctype_t *hdl, wint_t eucpc)
{
	wint_t	nwc1, nwc2;

	if ((nwc1 = (wint_t) _eucpctowc(hdl->cmapp, (wchar_t) eucpc)) == WEOF)
		return (eucpc);
	nwc2 = METHOD_NATIVE(hdl, towupper)(hdl, nwc1);
	return ((wint_t) _wctoeucpc(hdl->cmapp, (wchar_t) nwc2));
}
