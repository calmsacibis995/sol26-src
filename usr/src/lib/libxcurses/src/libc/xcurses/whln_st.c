/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)whln_st.c 1.1	95/12/22 SMI"

/*
 * whln_st.c		
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

#ifdef M_RCSID
#ifndef lint
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/whln_st.c 1.4 1995/07/07 18:53:05 ant Exp $";
#endif
#endif

#include <private.h>

int
whline_set(w, h, n)
WINDOW *w;
const cchar_t *h;
int n;
{
	int x, width;

#ifdef M_CURSES_TRACE
	__m_trace("whline_set(%p, %p, %d)",  w, h, n);
#endif

	if (h == (const cchar_t *) 0)
		h = WACS_HLINE;

	n += w->_curx;
	if (w->_maxx < n)
		n = w->_maxx;

	for (x = w->_curx; x < n; x += width)
                if ((width = __m_cc_replace(w, w->_cury, x, h, 0)) == -1)
			return __m_return_code("whline_set", ERR);

	WSYNC(w);

	return __m_return_code("whline_set", WFLUSH(w));
}

int
wvline_set(w, v, n)
WINDOW *w;
const cchar_t *v;
int n;
{
	int y;

#ifdef M_CURSES_TRACE
	__m_trace("wvline_set(%p, %p, %d)",  w, v, n);
#endif

	if (v == (const cchar_t *) 0)
		v = WACS_VLINE;

	n += w->_cury;
	if (w->_maxy < n)
		n = w->_maxy;

	for (y = w->_cury; y < n; ++y)
                if (__m_cc_replace(w, y, w->_curx, v, 0) == -1)
			return __m_return_code("wvline_set", ERR);

	WSYNC(w);

	return __m_return_code("wvline_set", WFLUSH(w));
}

