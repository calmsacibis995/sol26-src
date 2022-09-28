/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)delch.c 1.1	95/12/22 SMI"

/*
 * delch.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/delch.c 1.1 1995/06/05 19:54:29 ant Exp $";
#endif
#endif

#include <private.h>

int
(delch)()
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("delch(void)");
#endif

	code = wdelch(stdscr);

	return __m_return_code("delch", code);
}

int
(mvdelch)(y, x)
int y, x;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvdelch(%d, %d)", y, x);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = wdelch(stdscr);

	return __m_return_code("mvdelch", code);
}

int
(mvwdelch)(w, y, x)
WINDOW *w;
int y, x;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwdelch(%p, %d, %d)", w, y, x);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = wdelch(w);

	return __m_return_code("mvwdelch", code);
}
