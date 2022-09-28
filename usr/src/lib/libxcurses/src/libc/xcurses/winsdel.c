/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)winsdel.c 1.1	95/12/22 SMI"

/*
 * winsdel.c		
 *
 * XCurses Library
 *
 * Copyright 1990, 1995 by Mortice Kern Systems.  All rights reserved.
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/winsdel.c 1.4 1995/06/21 20:31:02 ant Exp $";
#endif
#endif

#include <private.h>
#include <stdlib.h>

/*f
 * Insert/delete rows from a window.
 *
 * Positive N inserts rows and negative N deletes.  The size of the
 * window remains fixed so that rows insert/deleted will cause rows to
 * disappear/appear at the end of the window.
 *
 * NOTE: This routine called in doupdate.c with curscr as window.  
 */
int
winsdelln(w, n)
WINDOW *w;
int n;
{
	int row;

#ifdef M_CURSES_TRACE
	__m_trace("winsdelln(%p, %d)", w, n);
#endif

	/* Check bounds and limit if necessary. */
	if (w->_maxy < w->_cury + abs(n))
		n = (w->_maxy - w->_cury + 1) * (n < 0 ? -1 : 1);

	/* Insert/delete accomplished by pointer shuffling. */
	if (n < 0) {
		/* Delete n lines from current cursor line. */
		(void) __m_ptr_move(
			(void **) w->_line, w->_maxy, 
			w->_cury, w->_cury - (n+1), w->_maxy
		);

		/* Blank lines come in at the bottom of the screen. */
		row = w->_maxy + n;
	} else {
		/* Insert n lines before current cursor line. */
		(void) __m_ptr_move(
			(void **) w->_line, w->_maxy, 
			w->_maxy - n, w->_maxy-1, w->_cury
		);

		/* Blank lines inserted at the cursor line. */
		row = w->_cury;
	}

	/* Clear inserted/deleted lines. */ 
	(void) __m_cc_erase(w, row, 0, row + abs(n), w->_maxx-1);

	/* Mark from the cursor line to the end of window as dirty. */ 
	(void) wtouchln(w, w->_cury, w->_maxy - w->_cury, 1);

	/* If we insert/delete lines at the top of the screen and we're,
	 * permitted to scroll, then treat the action as a scroll.
	 */
	if (w->_scroll && w->_cury == 0 && n != 0 && (w->_flags & W_FULL_WINDOW)
	&& w->_top == 0 && w->_bottom == w->_maxy)
                w->_scroll += n;
        else
                w->_scroll = 0;

	WSYNC(w);

	return __m_return_code("winsdelln", WFLUSH(w));
}

