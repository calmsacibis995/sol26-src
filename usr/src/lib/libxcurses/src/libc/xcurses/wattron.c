/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)wattron.c 1.1	95/12/22 SMI"

/*
 * wattron.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/wattron.c 1.1 1995/06/07 14:11:56 ant Exp $";
#endif
#endif

#include <private.h>

int
wattron(WINDOW *w, int at)
{
	cchar_t cc;

#ifdef M_CURSES_TRACE
        __m_trace("wattron(%p, %ld)", w, at);
#endif

	(void) __m_chtype_cc((chtype) at, &cc);
	w->_fg._at |= cc._at;

	return __m_return_code("wattron", OK);
}

int
wattroff(WINDOW *w, int at)
{
	cchar_t cc;

#ifdef M_CURSES_TRACE
        __m_trace("wattroff(%p, %ld)", w, at);
#endif

	(void) __m_chtype_cc((chtype) at, &cc);
	w->_fg._at &= ~cc._at;

	return __m_return_code("wattroff", OK);
}

int
wattrset(WINDOW *w, int at)
{
	cchar_t cc;

#ifdef M_CURSES_TRACE
        __m_trace("wattrset(%p, %ld)", w, at);
#endif

	(void) __m_chtype_cc((chtype) at, &cc);
	w->_fg._co = cc._co;
	w->_fg._at = cc._at;

	return __m_return_code("wattrset", OK);
}
