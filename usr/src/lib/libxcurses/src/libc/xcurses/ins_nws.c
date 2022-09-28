/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)ins_nws.c 1.1	95/12/22 SMI"

/*
 * ins_nws.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/ins_nws.c 1.1 1995/06/15 15:15:08 ant Exp $";
#endif
#endif

#include <private.h>

int
(ins_nwstr)(wcs, n)
const wchar_t *wcs;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("ins_nwstr(%p, %d)", wcs, n);
#endif

	code = wins_nwstr(stdscr, wcs, n);

	return __m_return_code("ins_nwstr", code);
}

int
(mvins_nwstr)(y, x, wcs, n)
int y, x;
const wchar_t *wcs;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvins_nwstr(%d, %d, %p, %d)", y, x, wcs, n);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = wins_nwstr(stdscr, wcs, n);

	return __m_return_code("mvins_nwstr", code);
}

int
(mvwins_nwstr)(w, y, x, wcs, n)
WINDOW *w;
int y, x;
const wchar_t *wcs;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwins_nwstr(%p, %d, %d, %p, %d)", w, y, x, wcs, n);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = wins_nwstr(w, wcs, n);

	return __m_return_code("mvwins_nwstr", code);
}

int
(ins_wstr)(wcs)
const wchar_t *wcs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("ins_wstr(%p)", wcs);
#endif

	code = wins_nwstr(stdscr, wcs, -1);

	return __m_return_code("ins_wstr", code);
}

int
(mvins_wstr)(y, x, wcs)
int y, x;
const wchar_t *wcs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvins_wstr(%d, %d, %p)", y, x, wcs);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = wins_nwstr(stdscr, wcs, -1);

	return __m_return_code("mvins_wstr", code);
}

int
(mvwins_wstr)(w, y, x, wcs)
WINDOW *w;
int y, x;
const wchar_t *wcs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwins_wstr(%p, %d, %d, %p)", w, y, x, wcs);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = wins_nwstr(w, wcs, -1);

	return __m_return_code("mvwins_wstr", code);
}

int
(wins_wstr)(w, wcs)
WINDOW *w;
const wchar_t *wcs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("wins_wstr(%p, %p)", w, wcs);
#endif

	code = wins_nwstr(w, wcs, -1);

	return __m_return_code("wins_wstr", code);
}

