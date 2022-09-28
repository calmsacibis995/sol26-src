/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)setcchar.c 1.1	95/12/22 SMI"

/*
 * setcchar.c
 * 
 * XCurses Library
 *
 * Copyright 1990, 1995 by Mortice Kern Systems Inc.  All rights reserved.
 *
 * This Software is unpublished, valuable, confidential property of
 * Mortice Kern Systems Inc.  Use is authorized only in accordance
 * with the terms and conditions of the source licence agreement
 * protecting this Software.  Any unauthorized use or disclosure of
 * this Software is strictly prohibited and will result in the
 * termination of the licence agreement.
 *
 * If you have any questions, please consult your supervisor.
 */

#if M_RCSID
#ifndef lint
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/setcchar.c 1.1 1995/06/07 13:58:33 ant Exp $";
#endif
#endif

#include <private.h>

int
setcchar(cchar_t *cc, const wchar_t *wcs, attr_t at, short co, const void *opts)
{
	int i;

#ifdef M_CURSES_TRACE
	__m_trace("setcchar(%p, %p, %x, %d, %p)", cc, wcs, at, co, opts);
#endif
	
	i = __m_wcs_cc(wcs, at, co, cc);

	return __m_return_code("setcchar", i < 0 || wcs[i] != '\0' ? ERR : OK);
}
