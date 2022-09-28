/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)hln.c 1.1	95/12/22 SMI"

/*
 * hln.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/hln.c 1.1 1995/05/29 19:59:30 ant Exp $";
#endif
#endif

#include <private.h>

#undef hline

int
hline(chtype h, int n)
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("hline(%ld, %d)", h, n);
#endif

	code = whline(stdscr, h, n);

	return __m_return_code("hline", code);
}

#undef mvhline

int
mvhline(int y, int x, chtype h, int n)
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvhline(%d, %d, %ld, %d)", y, x, h, n);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = whline(stdscr, h, n);

	return __m_return_code("mvhline", code);
}

#undef mvwhline

int
mvwhline(WINDOW *w, int y, int x, chtype h, int n)
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwhline(%p, %d, %d, %ld, %d)", w, y, x, h, n);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = whline(w, h, n);

	return __m_return_code("mvwhline", code);
}

#undef vline

int
vline(chtype v, int n)
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("vline(%ld, %d)", v, n);
#endif

	code = wvline(stdscr, v, n);

	return __m_return_code("vline", code);
}

#undef mvvline

int
mvvline(int y, int x, chtype v, int n)
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvvline(%d, %d, %ld, %d)", y, x, v, n);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = wvline(stdscr, v, n);

	return __m_return_code("mvvline", code);
}

#undef mvwvline

int
mvwvline(WINDOW *w, int y, int x, chtype v, int n)
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwvline(%p, %d, %d, %ld, %d)", w, y, x, v, n);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = wvline(w, v, n);

	return __m_return_code("mvwvline", code);
}

