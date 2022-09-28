/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)getch.c 1.1	95/12/22 SMI"

/*
 * getch.c
 * 
 * XCurses Library
 *
 * Copyright 1990, 1995 by Mortice Kern Systems Inc.  All rights reserved.
 *
 * This Software is unpublished, valuable, confidential property of
 * Mortice Kern Systems Inc.  Use is authorized only in awcpordance
 * with the terms and conditions of the source licence agreement
 * protecting this Software.  Any unauthorized use or disclosure of
 * this Software is strictly prohibited and will result in the
 * termination of the licence agreement.
 *
 * If you have any questions, please consult your supervisor.
 */

#if M_RCSID
#ifndef lint
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/getch.c 1.1 1995/05/25 17:56:14 ant Exp $";
#endif
#endif

#include <private.h>

#undef getch

int
getch()
{
	int value;

#ifdef M_CURSES_TRACE
	__m_trace("getch(void)");
#endif

	value = wgetch(stdscr);

	return __m_return_int("getch", value);
}

#undef mvgetch

int
mvgetch(y, x)
int y, x;
{
	int value;

#ifdef M_CURSES_TRACE
	__m_trace("mvgetch(%d, %d)", y, x);
#endif

	if (wmove(stdscr, y, x))
		return __m_return_int("mvgetch", EOF);

	value = wgetch(stdscr);

	return __m_return_int("mvgetch", value);
}

#undef mvwgetch

int
mvwgetch(w, y, x)
WINDOW *w;
int y, x;
{
	int value;

#ifdef M_CURSES_TRACE
	__m_trace("mvwgetch(%p, %d, %d)", w, y, x);
#endif

	if (wmove(w, y, x))
		return __m_return_int("mvgetch", EOF);

	value = wgetch(w);

	return __m_return_int("mvwgetch", value);
}

