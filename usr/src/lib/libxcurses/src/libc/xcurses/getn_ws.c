/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)getn_ws.c 1.1	95/12/22 SMI"

/*
 * getn_ws.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/getn_ws.c 1.1 1995/07/06 14:01:35 ant Exp $";
#endif
#endif

#include <private.h>

int
(getn_wstr)(wis, n)
wint_t *wis;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("getn_wstr(%p, %d)", wis, n);
#endif

	code = wgetn_wstr(stdscr, wis, n);

	return __m_return_code("getn_wstr", code);
}

int
(mvgetn_wstr)(y, x, wis, n)
int y, x;
wint_t *wis;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvgetn_wstr(%d, %d, %p, %d)", y, x, wis, n);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = wgetn_wstr(stdscr, wis, n);

	return __m_return_code("mvgetn_wstr", code);
}

int
(mvwgetn_wstr)(w, y, x, wis, n)
WINDOW *w;
int y, x;
wint_t *wis;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwgetn_wstr(%p, %d, %d, %p, %d)", w, y, x, wis, n);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = wgetn_wstr(w, wis, n);

	return __m_return_code("mvwgetn_wstr", code);
}

int
(get_wstr)(wis)
wint_t *wis;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("get_wstr(%p)", wis);
#endif

	code = wgetn_wstr(stdscr, wis, -1);

	return __m_return_code("get_wstr", code);
}

int
(mvget_wstr)(y, x, wis)
int y, x;
wint_t *wis;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvget_wstr(%d, %d, %p)", y, x, wis);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = wgetn_wstr(stdscr, wis, -1);

	return __m_return_code("mvget_wstr", code);
}

int
(mvwget_wstr)(w, y, x, wis)
WINDOW *w;
int y, x;
wint_t *wis;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwget_wstr(%p, %d, %d, %p)", w, y, x, wis);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = wgetn_wstr(w, wis, -1);

	return __m_return_code("mvwget_wstr", code);
}


int
(wget_wstr)(w, wis)
WINDOW *w;
wint_t *wis;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("wget_wstr(%p, %p)", w, wis);
#endif

	code = wgetn_wstr(w, wis, -1);

	return __m_return_code("wget_wstr", code);
}

