/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)dupwin.c 1.1	95/12/22 SMI"

/*
 * dupwin.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/dupwin.c 1.1 1995/05/26 18:41:41 ant Exp $";
#endif
#endif

#include <private.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

/*f
 * Make an exact duplicate of the window.
 */
WINDOW * 
dupwin(w) 
WINDOW *w;
{
	int i;
	WINDOW *v;

#ifdef M_CURSES_TRACE
	__m_trace("dupwin(%p)", w);
#endif

	/* Create (sub)window with same dimensions. */
	v = __m_newwin(w->_parent, w->_maxy, w->_maxx, w->_begy, w->_begx);	
	if (v == (WINDOW *) 0)
		return __m_return_pointer("dupwin", (WINDOW *) 0);

        /* Same software scroll region. */
        v->_top = w->_top;
        v->_bottom = w->_bottom;
                
        /* Same window input settings. */
        v->_vmin = w->_vmin;
        v->_vtime = w->_vtime;

	/* Same window flags. */
        v->_flags = w->_flags;

	/* Copy dirty region markers. */
	(void) memcpy(
		v->_first, w->_first,
		(v->_maxy + v->_maxy) * sizeof *v->_first
	);

	if (v->_parent == (WINDOW *) 0) {
		/* For a regular window, copy source window lines in the 
		 * correct order, because the pointers in _line[] may have 
		 * been shuffled.  A subwindow is not subject to this, 
		 * because it shares its data with its parent window.
		 */
		for (i = 0; i < w->_maxy; ++i) {
			(void) memcpy(
				v->_line[i], w->_line[i], 
				v->_maxx * sizeof **v->_line
			);
		}
	}

	return __m_return_pointer("dupwin", v);
}
