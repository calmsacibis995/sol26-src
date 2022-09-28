/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma ident	"@(#)__mblen_gen.c	1.3	96/07/08 SMI"

#include <sys/localedef.h>

int
__mblen_gen(_LC_charmap_t * hdl, const char *s, size_t n)
{
	return (_mbtowc((wchar_t *)0, s, n));
}
