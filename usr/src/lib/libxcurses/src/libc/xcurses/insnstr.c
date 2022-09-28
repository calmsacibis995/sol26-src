/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)insnstr.c 1.1	95/12/22 SMI"

/*
 * insnstr.c
 * 
 * XCurses Library
 *
 * Copyright 1990, 1995 by Mortice Kern Systems Inc.  All rights reserved.
 *
 * This Software is unpublished, valuable, confidential property of
 * Mortice Kern Systems Inc.  Use is authorized only in ambsordance
 * with the terms and conditions of the source licence agreement
 * protecting this Software.  Any unauthorized use or disclosure of
 * this Software is strictly prohibited and will result in the
 * termination of the licence agreement.
 *
 * If you have any questions, please consult your supervisor.
 */

#if M_RCSID
#ifndef lint
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/insnstr.c 1.1 1995/06/15 17:35:00 ant Exp $";
#endif
#endif

#include <private.h>

int
(insnstr)(mbs, n)
const char *mbs;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("insnstr(%p, %d)", mbs, n);
#endif

	code = winsnstr(stdscr, mbs, n);

	return __m_return_code("insnstr", code);
}

int
(mvinsnstr)(y, x, mbs, n)
int y, x;
const char *mbs;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvinsnstr(%d, %d, %p, %d)", y, x, mbs, n);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = winsnstr(stdscr, mbs, n);

	return __m_return_code("mvinsnstr", code);
}

int
(mvwinsnstr)(w, y, x, mbs, n)
WINDOW *w;
int y, x;
const char *mbs;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwinsnstr(%p, %d, %d, %p, %d)", w, y, x, mbs, n);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = winsnstr(w, mbs, n);

	return __m_return_code("mvwinsnstr", code);
}

int
(insstr)(mbs)
const char *mbs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("insstr(%p)", mbs);
#endif

	code = winsnstr(stdscr, mbs, -1);

	return __m_return_code("insstr", code);
}

int
(mvinsstr)(y, x, mbs)
int y, x;
const char *mbs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvinsstr(%d, %d, %p)", y, x, mbs);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = winsnstr(stdscr, mbs, -1);

	return __m_return_code("mvinsstr", code);
}

int
(mvwinsstr)(w, y, x, mbs)
WINDOW *w;
int y, x;
const char *mbs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwinsstr(%p, %d, %d, %p)", w, y, x, mbs);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = winsnstr(w, mbs, -1);

	return __m_return_code("mvwinsstr", code);
}

int
(winsstr)(w, mbs)
WINDOW *w;
const char *mbs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("winsstr(%p, %p)", w, mbs);
#endif

	code = winsnstr(w, mbs, -1);

	return __m_return_code("winsstr", code);
}

