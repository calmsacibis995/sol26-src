/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)winchn.c 1.1	95/12/22 SMI"

/*
 * winchn.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/winchn.c 1.1 1995/06/13 20:54:33 ant Exp $";
#endif
#endif

#include <private.h>

int
winchnstr(w, chs, n)
WINDOW *w;
chtype *chs;
int n;
{
	int x, eol;
	cchar_t *cp;

#ifdef M_CURSES_TRACE
	__m_trace("winchnstr(%p, %p, %d)", w, chs, n);
#endif

	eol = (n < 0 || w->_maxx < w->_curx + n) ? w->_maxx : w->_curx + n;
 
        for (cp = w->_line[w->_cury], x = w->_curx; x < eol; ++x, ++chs) {
		if ((*chs = __m_cc_chtype(&cp[x])) == (chtype) ERR)
			return __m_return_code("winchnstr", ERR);
	}

	/* For an unbounded buffer or a buffer with room remaining,
	 * null terminate the buffer.
	 */
	if (n < 0 || eol < w->_curx + n) 
		*chs = 0;

	return __m_return_code("winchnstr", OK);
}
