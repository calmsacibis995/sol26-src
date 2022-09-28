/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)wdelch.c 1.1	95/12/22 SMI"

/*
 * wdelch.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/wdelch.c 1.3 1995/06/21 20:30:59 ant Exp $";
#endif
#endif

#include <private.h>

/*f
 * Delete the character under the cursor; all characters to the right of
 * the cursor on the same line are moved to the left by one position and
 * the last character on the line is filled with a blank. The cursor
 * position does not change.
 */
int
wdelch(w)
WINDOW *w;
{
	int next, width, y, x;

#ifdef M_CURSES_TRACE
	__m_trace("wdelch(%p) at (%d,%d)", w, w->_cury, w->_curx);
#endif

	y = w->_cury;
	x = w->_curx;

	next = __m_cc_next(w, y, x);
	x = __m_cc_first(w, y, x);

	/* Determine the character width to delete. */
	width = __m_cc_width(&w->_line[y][x]);
	
	/* Shift line left to erase the character under the cursor. */
	(void) memcpy(
		&w->_line[y][x], &w->_line[y][next],
		(w->_maxx - next) * sizeof **w->_line
	);

	/* Add blank(s) to the end of line based on the width 
	 * of the character that was deleted.
	 */
	(void) __m_cc_erase(w, y, w->_maxx - width, y, w->_maxx - 1);

	/* Set dity region markers. */
	if (x < w->_first[y])
		w->_first[y] = x;
	w->_last[y] = w->_maxx;

	WSYNC(w);

	return __m_return_code("wdelch", WFLUSH(w));
}
