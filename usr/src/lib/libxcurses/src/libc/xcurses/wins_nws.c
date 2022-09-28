/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)wins_nws.c 1.1	95/12/22 SMI"

/*
 * wins_nws.c
 *
 * XCurses Library
 *
 * Copyright 1990, 1994 by Mortice Kern Systems Inc.  All rights reserved.
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

#ifdef M_RCSID
#ifndef lint
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/wins_nws.c 1.3 1995/09/27 18:49:50 ant Exp $";
#endif
#endif

#include <private.h>

int
wins_nwstr(w, wcs, n)
WINDOW *w;
const wchar_t *wcs;
int n;
{
	cchar_t cc;
	int i, y, x;  

#ifdef M_CURSES_TRACE
	__m_trace("wins_nwstr(%p, %p, n)", w, wcs, n);
#endif

	y = w->_cury; 
	x = w->_curx;

	if (n < 0)
		n = INT_MAX;

	for ( ; *wcs != '\0' && 0 < n; n -= i, wcs += i) {
		if ((i = __m_wcs_cc(wcs, w->_bg._at, w->_bg._co, &cc)) < 0
		|| __m_wins_wch(w, y, x, &cc, &y, &x) == ERR)
			return __m_return_code("wins_nwstr", ERR);
	}

	WSYNC(w);

	return __m_return_code("wins_nwstr", WFLUSH(w));
}
