/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)wclrbot.c 1.1	95/12/22 SMI"

/*
 * wclrbot.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/wclrbot.c 1.3 1995/06/21 20:30:58 ant Exp $";
#endif
#endif

#include <private.h>

/*f
 * Erase from the current cursor location right and down to the end of
 * the screen. The cursor position is not changed.
 */
int
wclrtobot(w)
WINDOW *w;
{
	int x;

#ifdef M_CURSES_TRACE
	__m_trace("wclrtobot(%p) from (%d, %d)", w, w->_cury, w->_curx);
#endif

	x = __m_cc_first(w, w->_cury, w->_curx);
	if (__m_cc_erase(w, w->_cury, x, w->_maxy-1, w->_maxx-1) != 0)
		return __m_return_code("wclrtobot", ERR);

	WSYNC(w);

	return __m_return_code("wclrtobot", WFLUSH(w));
}
