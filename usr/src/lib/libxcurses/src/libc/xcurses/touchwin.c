/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)touchwin.c 1.1	95/12/22 SMI"

/*
 * touchwin.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/touchwin.c 1.3 1995/06/15 18:43:04 ant Exp $";
#endif
#endif

#include <private.h>

int
(touchwin)(w)
WINDOW *w;
{
#ifdef M_CURSES_TRACE
	__m_trace("touchwin(%p)", w);
#endif

	return __m_return_code("touchwin", wtouchln(w, 0, w->_maxy, 1));
}

int
(untouchwin)(w)
WINDOW *w;
{
#ifdef M_CURSES_TRACE
	__m_trace("untouchwin(%p)", w);
#endif

	return __m_return_code("untouchwin", wtouchln(w, 0, w->_maxy, 0));
}

int
(touchline)(w, y, n)
WINDOW *w;
int y, n;
{
#ifdef M_CURSES_TRACE
	__m_trace("touchline(%p, %d, %d)", w, y, n);
#endif

	return __m_return_code("touchline", wtouchln(w, y, n, 1));
}

