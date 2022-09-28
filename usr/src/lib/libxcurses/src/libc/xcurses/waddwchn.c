/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)waddwchn.c 1.1	95/12/22 SMI"

/*
 * waddwchn.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/waddwchn.c 1.3 1995/06/21 20:30:56 ant Exp $";
#endif
#endif

#include <private.h>

int
wadd_wchnstr(w, cp, n)
WINDOW *w;
const cchar_t *cp;
int n;
{
	int x, width;

#ifdef M_CURSES_TRACE
	__m_trace("wadd_wchnstr(%p, %p, %d)", w, cp, n);
#endif

	if (n < 0 || w->_maxx < (n += w->_curx))
		n = w->_maxx;

	for (x = w->_curx; x < n && cp->_n != 0; x += width, ++cp)
		width = __m_cc_replace(w, w->_cury, x, cp, 0);

	WSYNC(w);

	return __m_return_code("wadd_wchnstr", WFLUSH(w));
}
