/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)__wcsxfrm_bc.c	1.2	96/07/01 SMI"

#include <wchar.h>
#include <sys/localedef.h>

size_t
__wcsxfrm_bc(_LC_collate_t *hdl, wchar_t *ws1, const wchar_t *ws2, size_t n)
{
	wchar_t *nws2;
	wchar_t *ws, *wd, wc;
	size_t  result;

	if ((nws2 = (wchar_t *) malloc((wcslen(ws2) + 1) * sizeof (wchar_t)))
	    == NULL)
		return (0);
	for (ws = (wchar_t *) ws2, wd = nws2; wc = *ws; ws++)
		*wd++ = _eucpctowc(hdl->cmapp, wc);
	*ws = wc;
	result = METHOD_NATIVE(hdl, wcsxfrm)(hdl, ws1,
					(const wchar_t *) nws2, n);
	free((void *) nws2);
	return (result);
}
