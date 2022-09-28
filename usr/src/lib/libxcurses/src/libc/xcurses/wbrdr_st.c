/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)wbrdr_st.c 1.1	95/12/22 SMI"

/*
 * wbrdr_st.c		
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/wbrdr_st.c 1.6 1995/07/07 18:52:43 ant Exp $";
#endif
#endif

#include <private.h>

/*f
 * Draw a border around the edges of the window. The parms correspond to
 * a character and attribute for the left, right, top, and bottom sides, 
 * top left, top right, bottom left, and bottom right corners. A zero in
 * any character parm means to take the default.
 */
int
wborder_set(w, ls, rs, ts, bs, tl, tr, bl, br)
WINDOW *w;
const cchar_t *ls, *rs, *ts, *bs, *tl, *tr, *bl, *br;
{
	short oflags;
	int x, y, code;

#ifdef M_CURSES_TRACE
	__m_trace(
		"wborder_set(%p, %p, %p, %p, %p, %p, %p, %p, %p)",
		 w, ls, rs, ts, bs, tl, tr, bl, br
	);
#endif

	code = ERR;
	x = w->_curx;
	y = w->_cury;

	oflags = w->_flags & (W_FLUSH | W_SYNC_UP);
	w->_flags &= ~(W_FLUSH | W_SYNC_UP);

	/* Verticals. */
	(void) wmove(w, 0, 0);
	(void) wvline_set(w, ls, w->_maxy);
	(void) wmove(w, 0, w->_maxx-1);
	(void) wvline_set(w, rs, w->_maxy);
	
	/* Horizontals. */
	(void) wmove(w, 0, 1);
	(void) whline_set(w, ts, w->_maxx-2);
	(void) wmove(w, w->_maxy-1, 1);
	(void) whline_set(w, bs, w->_maxx-2);

	w->_flags |= oflags;

	/* Corners. */
	if (tl == (const cchar_t *) 0)
		tl = WACS_ULCORNER;
	if (tr == (const cchar_t *) 0)
		tr = WACS_URCORNER;
	if (bl == (const cchar_t *) 0)
		bl = WACS_LLCORNER;
	if (br == (const cchar_t *) 0)
		br = WACS_LRCORNER;

	if (__m_cc_replace(w, 0, 0, tl, 0) == -1)
		goto error;
	if (__m_cc_replace(w, 0, w->_maxx-1, tr, 0) == -1)
		goto error;
	if (__m_cc_replace(w, w->_maxy-1, 0, bl, 0) == -1)
		goto error;
	if (__m_cc_replace(w, w->_maxy-1, w->_maxx-1, br, 0) == -1)
		goto error;

	(void) wmove(w, y, x);

	WSYNC(w);

	code = WFLUSH(w);
error:
	return __m_return_code("wborder_set", code);
}

