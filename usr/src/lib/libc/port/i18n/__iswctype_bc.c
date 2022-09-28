/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__iswctype_bc.c 1.3	96/09/18  SMI"

#include <wchar.h>
#include <sys/localedef.h>

/*
 * EUC Process Code mode wrapper method for iswctype()
 */

int
__iswctype_bc(_LC_ctype_t *hdl, wint_t eucpc, wctype_t mask)
{
	wint_t nwc;

	if ((nwc = (wint_t) _eucpctowc(hdl->cmapp, (wchar_t) eucpc)) == WEOF)
		return (0);
	return (METHOD_NATIVE(hdl, iswctype)(hdl, nwc, mask));
}
