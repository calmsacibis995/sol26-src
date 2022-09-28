/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)addnws.c 1.1	95/12/22 SMI"

/*
 * addnws.c
 * 
 * XCurses Library
 *
 * Copyright 1990, 1995 by Mortice Kern Systems Inc.  All rights reserved.
 *
 * This Software is unpublished, valuable, confidential property of
 * Mortice Kern Systems Inc.  Use is authorized only in awcsordance
 * with the terms and conditions of the source licence agreement
 * protecting this Software.  Any unauthorized use or disclosure of
 * this Software is strictly prohibited and will result in the
 * termination of the licence agreement.
 *
 * If you have any questions, please consult your supervisor.
 */

#if M_RCSID
#ifndef lint
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/addnws.c 1.2 1995/05/18 20:55:00 ant Exp $";
#endif
#endif

#include <private.h>

#undef addnwstr

int
addnwstr(wcs, n)
const wchar_t *wcs;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("addnwstr(%p, %d)", wcs, n);
#endif

	code = waddnwstr(stdscr, wcs, n);

	return __m_return_code("addnwstr", code);
}

#undef mvaddnwstr

int
mvaddnwstr(y, x, wcs, n)
int y, x;
const wchar_t *wcs;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvaddnwstr(%d, %d, %p, %d)", y, x, wcs, n);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = waddnwstr(stdscr, wcs, n);

	return __m_return_code("mvaddnwstr", code);
}

#undef mvwaddnwstr

int
mvwaddnwstr(w, y, x, wcs, n)
WINDOW *w;
int y, x;
const wchar_t *wcs;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwaddnwstr(%p, %d, %d, %p, %d)", w, y, x, wcs, n);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = waddnwstr(w, wcs, n);

	return __m_return_code("mvwaddnwstr", code);
}

#undef addwstr

int
addwstr(wcs)
const wchar_t *wcs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("addwstr(%p)", wcs);
#endif

	code = waddnwstr(stdscr, wcs, -1);

	return __m_return_code("addwstr", code);
}

#undef mvaddwstr

int
mvaddwstr(y, x, wcs)
int y, x;
const wchar_t *wcs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvaddwstr(%d, %d, %p)", y, x, wcs);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = waddnwstr(stdscr, wcs, -1);

	return __m_return_code("mvaddwstr", code);
}

#undef mvwaddwstr

int
mvwaddwstr(w, y, x, wcs)
WINDOW *w;
int y, x;
const wchar_t *wcs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwaddwstr(%p, %d, %d, %p)", w, y, x, wcs);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = waddnwstr(w, wcs, -1);

	return __m_return_code("mvwaddwstr", code);
}

#undef waddwstr

int
waddwstr(w, wcs)
WINDOW *w;
const wchar_t *wcs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("waddwstr(%p, %p)", w, wcs);
#endif

	code = waddnwstr(w, wcs, -1);

	return __m_return_code("waddwstr", code);
}

