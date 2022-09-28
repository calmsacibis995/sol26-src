/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)innstr.c 1.1	95/12/22 SMI"

/*
 * innstr.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/innstr.c 1.1 1995/06/14 15:26:08 ant Exp $";
#endif
#endif

#include <private.h>

int
(innstr)(s, n)
char *s;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("innstr(%p, %d)", s, n);
#endif

	code = winnstr(stdscr, s, n);

	return __m_return_code("innstr", code);
}

int
(mvinnstr)(y, x, s, n)
int y, x;
char *s;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvinnstr(%d, %d, %p, %d)", y, x, s, n);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = winnstr(stdscr, s, n);

	return __m_return_code("mvinnstr", code);
}

int
(mvwinnstr)(w, y, x, s, n)
WINDOW *w;
int y, x;
char *s;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwinnstr(%p, %d, %d, %p, %d)", w, y, x, s, n);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = winnstr(w, s, n);

	return __m_return_code("mvwinnstr", code);
}

int
(instr)(s)
char *s;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("instr(%p)", s);
#endif

	code = winnstr(stdscr, s, -1);

	return __m_return_code("instr", code);
}

int
(mvinstr)(y, x, s)
int y, x;
char *s;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvinstr(%d, %d, %p)", y, x, s);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = winnstr(stdscr, s, -1);

	return __m_return_code("mvinstr", code);
}

int
(mvwinstr)(w, y, x, s)
WINDOW *w;
int y, x;
char *s;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwinstr(%p, %d, %d, %p)", w, y, x, s);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = winnstr(w, s, -1);

	return __m_return_code("mvwinstr", code);
}

int
(winstr)(w, s)
WINDOW *w;
char *s;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("winstr(%p, %p)", w, s);
#endif

	code = winnstr(w, s, -1);

	return __m_return_code("winstr", code);
}

