/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)getnstr.c 1.1	95/12/22 SMI"

/*
 * getnstr.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/getnstr.c 1.1 1995/06/06 19:11:25 ant Exp $";
#endif
#endif

#include <private.h>

int
(getnstr)(str, n)
char *str;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("getnstr(%p, %d)", str, n);
#endif

	code = wgetnstr(stdscr, str, n);

	return __m_return_code("getnstr", code);
}

int
(mvgetnstr)(y, x, str, n)
int y, x;
char *str;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvgetnstr(%d, %d, %p, %d)", y, x, str, n);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = wgetnstr(stdscr, str, n);

	return __m_return_code("mvgetnstr", code);
}

int
(mvwgetnstr)(w, y, x, str, n)
WINDOW *w;
int y, x;
char *str;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwgetnstr(%p, %d, %d, %p, %d)", w, y, x, str, n);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = wgetnstr(w, str, n);

	return __m_return_code("mvwgetnstr", code);
}

int
(getstr)(str)
char *str;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("getstr(%p)", str);
#endif

	code = wgetnstr(stdscr, str, -1);

	return __m_return_code("getstr", code);
}

int
(mvgetstr)(y, x, str)
int y, x;
char *str;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvgetstr(%d, %d, %p)", y, x, str);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = wgetnstr(stdscr, str, -1);

	return __m_return_code("mvgetstr", code);
}

int
(mvwgetstr)(w, y, x, str)
WINDOW *w;
int y, x;
char *str;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwgetstr(%p, %d, %d, %p)", w, y, x, str);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = wgetnstr(w, str, -1);

	return __m_return_code("mvwgetstr", code);
}


int
(wgetstr)(w, str)
WINDOW *w;
char *str;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("wgetstr(%p, %p)", w, str);
#endif

	code = wgetnstr(w, str, -1);

	return __m_return_code("wgetstr", code);
}

