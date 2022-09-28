/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)wscrl.c 1.1	95/12/22 SMI"

/*
 * wscrl.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/wscrl.c 1.4 1995/07/26 17:43:20 ant Exp $";
#endif
#endif

#include <private.h>
#include <string.h>

/*f
 * For positive n scroll the window up n lines (line i+n becomes i);
 * otherwise scroll the window down n lines.
 */
int
wscrl(w, n)
WINDOW *w;
int n;
{
	int y, x, width, start, finish, to; 

#ifdef M_CURSES_TRACE
	__m_trace("wscrl(%p, %d)", w, n);
#endif

	if (n == 0)
		return __m_return_code("wscrl", OK);

	/* Shuffle pointers in order to scroll.  The region 
	 * from start to finish inclusive will be moved to
	 * either the top or bottom of _line[].
	 */
	if (0 < n) {
		start = w->_top;
		finish = w->_top + n - 1;
		to = w->_bottom;
	} else {
		start = w->_bottom + n;
		finish = w->_bottom - 1;
		to = w->_top;
	}

	/* Blank out new lines. */
	if (__m_cc_erase(w, start, 0, finish, w->_maxx-1) == -1)
		return __m_return_code("wscrl", ERR);

	/* Scroll lines by shuffling pointers. */
	(void) __m_ptr_move((void **) w->_line, w->_maxy, start, finish, to);

	if ((w->_flags & W_FULL_WINDOW)
	&& w->_top == 0 && w->_bottom == w->_maxy)
		w->_scroll += n;
	else
		w->_scroll = 0;

	(void) wtouchln(w, 0, w->_maxy, 1);

	WSYNC(w);

	return __m_return_code("wscrl", WFLUSH(w));
}
