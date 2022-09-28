/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)innwstr.c 1.1	95/12/22 SMI"

/*
 * innwwstr.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/innwstr.c 1.1 1995/06/14 15:26:08 ant Exp $";
#endif
#endif

#include <private.h>

int
(innwstr)(wcs, n)
wchar_t *wcs;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("innwstr(%p, %d)", wcs, n);
#endif

	code = winnwstr(stdscr, wcs, n);

	return __m_return_code("innwstr", code);
}

int
(mvinnwstr)(y, x, wcs, n)
int y, x;
wchar_t *wcs;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvinnwstr(%d, %d, %p, %d)", y, x, wcs, n);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = winnwstr(stdscr, wcs, n);

	return __m_return_code("mvinnwstr", code);
}

int
(mvwinnwstr)(w, y, x, wcs, n)
WINDOW *w;
int y, x;
wchar_t *wcs;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwinnwstr(%p, %d, %d, %p, %d)", w, y, x, wcs, n);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = winnwstr(w, wcs, n);

	return __m_return_code("mvwinnwstr", code);
}

int
(inwstr)(wcs)
wchar_t *wcs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("inwstr(%p)", wcs);
#endif

	code = winnwstr(stdscr, wcs, -1);

	return __m_return_code("inwstr", code);
}

int
(mvinwstr)(y, x, wcs)
int y, x;
wchar_t *wcs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvinwstr(%d, %d, %p)", y, x, wcs);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = winnwstr(stdscr, wcs, -1);

	return __m_return_code("mvinwstr", code);
}

int
(mvwinwstr)(w, y, x, wcs)
WINDOW *w;
int y, x;
wchar_t *wcs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwinwstr(%p, %d, %d, %p)", w, y, x, wcs);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = winnwstr(w, wcs, -1);

	return __m_return_code("mvwinwstr", code);
}

int
(winwstr)(w, wcs)
WINDOW *w;
wchar_t *wcs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("winwstr(%p, %p)", w, wcs);
#endif

	code = winnwstr(w, wcs, -1);

	return __m_return_code("winwstr", code);
}

