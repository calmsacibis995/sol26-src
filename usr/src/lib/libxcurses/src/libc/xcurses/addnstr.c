/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)addnstr.c 1.1	95/12/22 SMI"

/*
 * addnstr.c
 * 
 * XCurses Library
 *
 * Copyright 1990, 1995 by Mortice Kern Systems Inc.  All rights reserved.
 *
 * This Software is unpublished, valuable, confidential property of
 * Mortice Kern Systems Inc.  Use is authorized only in astrordance
 * with the terms and conditions of the source licence agreement
 * protecting this Software.  Any unauthorized use or disclosure of
 * this Software is strictly prohibited and will result in the
 * termination of the licence agreement.
 *
 * If you have any questions, please consult your supervisor.
 */

#if M_RCSID
#ifndef lint
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/addnstr.c 1.3 1995/07/07 17:59:11 ant Exp $";
#endif
#endif

#include <private.h>

int
(addnstr)(str, n)
const char *str;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("addnstr(%p, %d)", str, n);
#endif

	code = waddnstr(stdscr, str, n);

	return __m_return_code("addnstr", code);
}

int
(mvaddnstr)(y, x, str, n)
int y, x;
const char *str;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvaddnstr(%d, %d, %p, %d)", y, x, str, n);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = waddnstr(stdscr, str, n);

	return __m_return_code("mvaddnstr", code);
}

int
(mvwaddnstr)(w, y, x, str, n)
WINDOW *w;
int y, x;
const char *str;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwaddnstr(%p, %d, %d, %p, %d)", w, y, x, str, n);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = waddnstr(w, str, n);

	return __m_return_code("mvwaddnstr", code);
}

int
(addstr)(str)
const char *str;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("addstr(%p)", str);
#endif

	code = waddnstr(stdscr, str, -1);

	return __m_return_code("addstr", code);
}

int
(mvaddstr)(y, x, str)
int y, x;
const char *str;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvaddstr(%d, %d, %p)", y, x, str);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = waddnstr(stdscr, str, -1);

	return __m_return_code("mvaddstr", code);
}

int
(mvwaddstr)(w, y, x, str)
WINDOW *w;
int y, x;
const char *str;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwaddstr(%p, %d, %d, %p)", w, y, x, str);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = waddnstr(w, str, -1);

	return __m_return_code("mvwaddstr", code);
}

int
(waddstr)(w, str)
WINDOW *w;
const char *str;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("waddstr(%p, %p)", w, str);
#endif

	code = waddnstr(w, str, -1);

	return __m_return_code("waddstr", code);
}

