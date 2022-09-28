/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)in_wch.c 1.1	95/12/22 SMI"

/*
 * in_wch.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/in_wch.c 1.2 1995/06/14 15:30:53 ant Exp $";
#endif
#endif

#include <private.h>

int
(in_wch)(cc)
cchar_t *cc;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("in_wch(%p)", cc);
#endif

	code = win_wch(stdscr, cc);

	return __m_return_code("in_wch", code);
}

int
(mvin_wch)(y, x, cc)
int y, x;
cchar_t *cc;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvin_wch(%d, %d, %p)", y, x, cc);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = win_wch(stdscr, cc);

	return __m_return_code("mvin_wch", code);
}

int
(mvwin_wch)(w, y, x, cc)
WINDOW *w;
int y, x;
cchar_t *cc;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwin_wch(%p, %d, %d, %p)", w, y, x, cc);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = win_wch(w, cc);

	return __m_return_code("mvwin_wch", code);
}
