/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)addchn.c 1.1	95/12/22 SMI"

/*
 * addchn.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/addchn.c 1.1 1995/05/30 13:39:22 ant Exp $";
#endif
#endif

#include <private.h>

#undef addchnstr

int
addchnstr(chs, n)
const chtype *chs;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("addchnstr(%p, %d)", chs, n);
#endif

	code = waddchnstr(stdscr, chs, n);

	return __m_return_code("addchnstr", code);
}

#undef mvaddchnstr

int
mvaddchnstr(y, x, chs, n)
int y, x;
const chtype *chs;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvaddchnstr(%d, %d, %p, %d)", y, x, chs, n);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = waddchnstr(stdscr, chs, n);

	return __m_return_code("mvaddchnstr", code);
}

#undef mvwaddchnstr

int
mvwaddchnstr(w, y, x, chs, n)
WINDOW *w;
int y, x;
const chtype *chs;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwaddchnstr(%p, %d, %d, %p, %d)", w, y, x, chs, n);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = waddchnstr(w, chs, n);

	return __m_return_code("mvwaddchnstr", code);
}

#undef addchstr

int
addchstr(chs)
const chtype *chs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("addchstr(%p)", chs);
#endif

	code = waddchnstr(stdscr, chs, -1);

	return __m_return_code("addchstr", code);
}

#undef mvaddchstr

int
mvaddchstr(y, x, chs)
int y, x;
const chtype *chs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvaddchstr(%d, %d, %p)", y, x, chs);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = waddchnstr(stdscr, chs, -1);

	return __m_return_code("mvaddchstr", code);
}

#undef mvwaddchstr

int
mvwaddchstr(w, y, x, chs)
WINDOW *w;
int y, x;
const chtype *chs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwaddchstr(%p, %d, %d, %p)", w, y, x, chs);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = waddchnstr(w, chs, -1);

	return __m_return_code("mvwaddchstr", code);
}

#undef waddchstr

int
waddchstr(w, chs)
WINDOW *w;
const chtype *chs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("waddchstr(%p, %p)", w, chs);
#endif

	code = waddchnstr(w, chs, -1);

	return __m_return_code("waddchstr", code);
}

