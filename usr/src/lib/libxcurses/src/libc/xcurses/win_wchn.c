/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)win_wchn.c 1.1	95/12/22 SMI"

/*
 * win_wchn.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/win_wchn.c 1.1 1995/06/14 15:26:05 ant Exp $";
#endif
#endif

#include <private.h>

int
win_wchnstr(w, ccs, n)
WINDOW *w;
cchar_t *ccs;
int n;
{
	int x, eol;
	cchar_t *cp, null = { 0 };

#ifdef M_CURSES_TRACE
	__m_trace("win_wchnstr(%p, %p, %d)", w, ccs, n);
#endif

	eol = (n < 0 || w->_maxx < w->_curx + n) ? w->_maxx : w->_curx + n;
 
        for (cp = w->_line[w->_cury], x = w->_curx; x < eol; ++ccs) {
		*ccs = *cp;
		ccs->_f = 1;

		x = __m_cc_next(w, w->_cury, x);
	}

	/* For an unbounded buffer or a buffer with room remaining,
	 * null terminate the buffer.
	 */
	if (n < 0 || eol < w->_curx + n) 
		*ccs = null;

	return __m_return_code("win_wchnstr", OK);
}
