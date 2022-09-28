/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)deleteln.c 1.1	95/12/22 SMI"

/*
 * deleteln.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/deleteln.c 1.1 1995/05/16 12:49:31 ant Exp $";
#endif
#endif

#include <private.h>

#undef deleteln

int
deleteln()
{
#ifdef M_CURSES_TRACE
	__m_trace("deleteln(void)");
#endif

	return __m_return_code("deleteln", winsdelln(stdscr, -1));
}

#undef insertln

int
insertln()
{
#ifdef M_CURSES_TRACE
	__m_trace("insertln(void)");
#endif

	return __m_return_code("insertln", winsdelln(stdscr, 1));
}

#undef insdelln

int
insdelln(n)
int n;
{
#ifdef M_CURSES_TRACE
	__m_trace("insdelln(%d)", n);
#endif

	return __m_return_code("insdelln", winsdelln(stdscr, n));
}

#undef wdeleteln

int
wdeleteln(w)
WINDOW *w;
{
#ifdef M_CURSES_TRACE
	__m_trace("wdeleteln(%p)", w);
#endif

	return __m_return_code("wdeleteln", winsdelln(w, -1));
}

#undef winsertln

int
winsertln(w)
WINDOW *w;
{
#ifdef M_CURSES_TRACE
	__m_trace("winsertln(%p)", w);
#endif

	return __m_return_code("winsertln", winsdelln(w, 1));
}
