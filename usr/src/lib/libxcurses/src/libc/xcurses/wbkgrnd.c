/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)wbkgrnd.c 1.1	95/12/22 SMI"

/*
 * wbkgrnd.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/wbkgrnd.c 1.4 1995/06/21 20:30:57 ant Exp $";
#endif
#endif

#include <private.h>
#include <string.h>

/*f
 * Combine the new background setting with every position in the window.
 * The background is any combination of attributes and a character.
 * Only the attribute part is used to set the background of non-blank
 * characters, while both character and attributes are used for blank
 * positions.
 */
int
wbkgrnd(w, bg)
WINDOW *w;
const cchar_t *bg;
{
	short y, x;
	cchar_t old_bg, *cp;

#ifdef M_CURSES_TRACE
	__m_trace("wbkgrnd(%p, %p)", w, bg);
#endif

	old_bg = w->_bg;
	w->_bg = *bg;
	
	for (y = 0; y < w->_maxy; ++y) {
		for (cp = w->_line[y], x = 0; x < w->_maxx; ++x) {
			old_bg._f = cp->_f;
			if (__m_cc_compare(cp, &w->_bg, 0)) {
				/* Replace entire background character. */
				*cp = *bg;
			} else {
				/* Disable old background attributes. */
				cp->_at &= ~old_bg._at;

				/* Enable new background and current
				 * foreground.  The foreground is included
				 * in case there was overlap with the old
				 * background and the foreground.
				 */
				cp->_at |= bg->_at | w->_fg._at;
			}
		}

		/* Mark line as touched. */
		w->_first[y] = 0;
		w->_last[y] = x;
	}

	WSYNC(w);

	return __m_return_code("wbkgrnd", WFLUSH(w));
}
