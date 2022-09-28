/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)bkgrndst.c 1.1	95/12/22 SMI"

/*
 * bkgrndst.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/bkgrndst.c 1.3 1995/08/28 19:22:20 danv Exp $";
#endif
#endif

#include <private.h>

void
(bkgrndset)(bg)
const cchar_t *bg;
{
#ifdef M_CURSES_TRACE
	__m_trace("bkgrndset(%p)", bg);
#endif

	stdscr->_bg = *bg;

	__m_return_void("bkgrndset");
}

void
(wbkgrndset)(w, bg)
WINDOW *w;
const cchar_t *bg;
{
#ifdef M_CURSES_TRACE
	__m_trace("wbkgrndset(%p, %p)", w, bg);
#endif

	w->_bg = *bg;

	__m_return_void("wbkgrndset");
}

int
(getbkgrnd)(bg)
cchar_t *bg;
{
#ifdef M_CURSES_TRACE
	__m_trace("getbkgrnd(%p)", bg);
#endif

	*bg = stdscr->_bg;

	return __m_return_code("getbkgrnd", OK);
}

int
(wgetbkgrnd)(w, bg)
WINDOW *w;
cchar_t *bg;
{
#ifdef M_CURSES_TRACE
	__m_trace("wgetbkgrnd(%p, %p)", w, bg);
#endif

	*bg = w->_bg;

	return __m_return_code("wgetbkgrnd", OK);
}
