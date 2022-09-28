/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)inchn.c 1.1	95/12/22 SMI"

/*
 * inchn.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/inchn.c 1.1 1995/06/13 21:05:53 ant Exp $";
#endif
#endif

#include <private.h>

int
(inchnstr)(chs, n)
chtype *chs;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("inchnstr(%p, %d)", chs, n);
#endif

	code = winchnstr(stdscr, chs, n);

	return __m_return_code("inchnstr", code);
}

int
(mvinchnstr)(y, x, chs, n)
int y, x;
chtype *chs;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvinchnstr(%d, %d, %p, %d)", y, x, chs, n);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = winchnstr(stdscr, chs, n);

	return __m_return_code("mvinchnstr", code);
}

int
(mvwinchnstr)(w, y, x, chs, n)
WINDOW *w;
int y, x;
chtype *chs;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwinchnstr(%p, %d, %d, %p, %d)", w, y, x, chs, n);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = winchnstr(w, chs, n);

	return __m_return_code("mvwinchnstr", code);
}

int
(inchstr)(chs)
chtype *chs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("inchstr(%p)", chs);
#endif

	code = winchnstr(stdscr, chs, -1);

	return __m_return_code("inchstr", code);
}

int
(mvinchstr)(y, x, chs)
int y, x;
chtype *chs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvinchstr(%d, %d, %p)", y, x, chs);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = winchnstr(stdscr, chs, -1);

	return __m_return_code("mvinchstr", code);
}

int
(mvwinchstr)(w, y, x, chs)
WINDOW *w;
int y, x;
chtype *chs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwinchstr(%p, %d, %d, %p)", w, y, x, chs);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = winchnstr(w, chs, -1);

	return __m_return_code("mvwinchstr", code);
}

int
(winchstr)(w, chs)
WINDOW *w;
chtype *chs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("winchstr(%p, %p)", w, chs);
#endif

	code = winchnstr(w, chs, -1);

	return __m_return_code("winchstr", code);
}

