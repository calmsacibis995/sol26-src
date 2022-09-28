/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)mvwin.c 1.1	95/12/22 SMI"

/*
 * mvwin.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/mvwin.c 1.3 1995/06/15 19:19:58 ant Exp $";
#endif
#endif

#include <private.h>

/*f
 * Move window so that the upper left-hand corner is at (x,y). If the move
 * would cause the window to be off the screen, it is an error and the
 * window is not moved.  Moving subwindows is allowed, but should be 
 * avoided.
 */
int
mvwin(w, by, bx)
WINDOW *w;
int by, bx;
{
	int i, dx, dy;
	WINDOW *parent = w->_parent;

#ifdef M_CURSES_TRACE
	__m_trace("mvwin(%p, %d, %d)", w, by, bx);
#endif

	/* Check lower bounds of new window position. */
	if (by < 0 || bx < 0)
		return __m_return_code("mvwin", ERR);

	if (parent == (WINDOW *) 0) {
		/* Check upper bounds of normal window. */
		if (lines < by + w->_maxy || columns < bx + w->_maxx)
			return __m_return_code("mvwin", ERR);
	} else {
		/* Check upper bounds of sub-window. */
		if (parent->_begy + parent->_maxy < by + w->_maxy 
		|| parent->_begx + parent->_maxx < bx + w->_maxx)
			return __m_return_code("mvwin", ERR);

		/* Move the sub-window's line pointers to the parent 
		 * window's data. 
		 */
		dy = by - parent->_begy;
		dx = bx - parent->_begx;

		for (i = 0; i <= w->_maxy; ++i)
			w->_line[i] = &parent->_line[dy++][dx];
	}

	w->_begy = by;
	w->_begx = bx;
	(void) wtouchln(w, 0, w->_maxy, 1);

	return __m_return_code("mvwin", OK);
}

int
mvderwin(w, py, px)
WINDOW *w;
int py, px;
{
	int code;
	WINDOW *parent;

#ifdef M_CURSES_TRACE
	__m_trace("mvderwin(%p, %d, %d)", w, py, px);
#endif

	parent = w->_parent;

	if (parent == (WINDOW *) 0)
		return __m_return_code("mvderwin", ERR);

	/* Absolute screen address. */
	py += parent->_begy;
	px += parent->_begx;

	code = mvwin(w, py, px);

	return __m_return_code("mvderwin", code);
}
