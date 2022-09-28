/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)attron.c 1.1	95/12/22 SMI"

/*
 * attron.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/attron.c 1.3 1995/07/07 17:59:14 ant Exp $";
#endif
#endif

#include <private.h>

int
attron(int at)
{
	cchar_t cc;

#ifdef M_CURSES_TRACE
        __m_trace("attron(%lx)", at);
#endif

	(void) __m_chtype_cc((chtype) at, &cc);
	stdscr->_fg._at |= cc._at;

	return __m_return_code("attron", OK);
}

int
attroff(int at)
{
	cchar_t cc;

#ifdef M_CURSES_TRACE
        __m_trace("attroff(%lx)", (long) at);
#endif

	(void) __m_chtype_cc((chtype) at, &cc);
	stdscr->_fg._at &= ~cc._at;

	return __m_return_code("attroff", OK);
}

int
attrset(int at)
{
	cchar_t cc;

#ifdef M_CURSES_TRACE
        __m_trace("attrset(%lx)", (long) at);
#endif

	(void) __m_chtype_cc((chtype) at, &cc);
	stdscr->_fg._co = cc._co;
	stdscr->_fg._at = cc._at;

	return __m_return_code("attrset", OK);
}

chtype
(COLOR_PAIR)(short co)
{
	chtype ch;

#ifdef M_CURSES_TRACE
        __m_trace("COLOR_PAIR(%d)", co);
#endif

	ch = (chtype)(co) << __COLOR_SHIFT;

	return __m_return_chtype("COLOR_PAIR", ch);
}
	
short
(PAIR_NUMBER)(chtype at)
{
	short pair;

#ifdef M_CURSES_TRACE
        __m_trace("PAIR_NUMBER(%ld)", at);
#endif

	pair = (short) ((at & A_COLOR) >> __COLOR_SHIFT);

	return __m_return_int("PAIR_NUMBER", pair);
}
