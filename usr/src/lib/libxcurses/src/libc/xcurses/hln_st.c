/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)hln_st.c 1.1	95/12/22 SMI"

/*
 * hln_st.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/hln_st.c 1.1 1995/05/29 19:59:32 ant Exp $";
#endif
#endif

#include <private.h>

#undef hline_set

int
hline_set(const cchar_t *h, int n)
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("hline_set(%p, %d)", h, n);
#endif

	code = whline_set(stdscr, h, n);

	return __m_return_code("hline_set", code);
}

#undef mvhline_set

int
mvhline_set(int y, int x, const cchar_t *h, int n)
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvhline_set(%d, %d, %p, %d)", y, x, h, n);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = whline_set(stdscr, h, n);

	return __m_return_code("mvhline_set", code);
}

#undef mvwhline_set

int
mvwhline_set(WINDOW *w, int y, int x, const cchar_t *h, int n)
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwhline_set(%p, %d, %d, %p, %d)", w, y, x, h, n);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = whline_set(w, h, n);

	return __m_return_code("mvwhline_set", code);
}

#undef vline_set

int
vline_set(const cchar_t *v, int n)
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("vline_set(%p, %d)", v, n);
#endif

	code = wvline_set(stdscr, v, n);

	return __m_return_code("vline_set", code);
}

#undef mvvline_set

int
mvvline_set(int y, int x, const cchar_t *v, int n)
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvvline_set(%d, %d, %p, %d)", y, x, v, n);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = wvline_set(stdscr, v, n);

	return __m_return_code("mvvline_set", code);
}

#undef mvwvline_set

int
mvwvline_set(WINDOW *w, int y, int x, const cchar_t *v, int n)
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwvline_set(%p, %d, %d, %p, %d)", w, y, x, v, n);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = wvline_set(w, v, n);

	return __m_return_code("mvwvline_set", code);
}

