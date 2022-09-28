/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)wclear.c 1.1	95/12/22 SMI"

/*
 * wclear.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/wclear.c 1.2 1995/07/21 12:58:52 ant Exp $";
#endif
#endif

#include <private.h>

/*
 * Erase window and clear screen next update.
 */
int
(wclear)(w)
WINDOW *w;
{
	int value;

#ifdef M_CURSES_TRACE
	__m_trace("wclear(%p)", w);
#endif

	w->_flags |= W_CLEAR_WINDOW;
	value = werase(w);

	return __m_return_code("wclear", value == 0 ? OK : ERR);
}

/*
 * Erase window.
 */
int
(werase)(w)
WINDOW *w;
{
	int value;

#ifdef M_CURSES_TRACE
	__m_trace("werase(%p)", w);
#endif

	w->_cury = 0;
	w->_curx = 0;
	value = __m_cc_erase(w, 0, 0, w->_maxy-1, w->_maxx-1);

	return __m_return_code("werase", value == 0 ? OK : ERR);
}

