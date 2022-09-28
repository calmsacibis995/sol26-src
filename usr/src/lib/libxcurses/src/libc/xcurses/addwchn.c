/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)addwchn.c 1.1	95/12/22 SMI"

/*
 * addwchn.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/addwchn.c 1.1 1995/05/30 13:39:41 ant Exp $";
#endif
#endif

#include <private.h>

#undef add_wchnstr

int
add_wchnstr(ccs, n)
const cchar_t *ccs;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("add_wchnstr(%p, %d)", ccs, n);
#endif

	code = wadd_wchnstr(stdscr, ccs, n);

	return __m_return_code("add_wchnstr", code);
}

#undef mvadd_wchnstr

int
mvadd_wchnstr(y, x, ccs, n)
int y, x;
const cchar_t *ccs;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvadd_wchnstr(%d, %d, %p, %d)", y, x, ccs, n);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = wadd_wchnstr(stdscr, ccs, n);

	return __m_return_code("mvadd_wchnstr", code);
}

#undef mvwadd_wchnstr

int
mvwadd_wchnstr(w, y, x, ccs, n)
WINDOW *w;
int y, x;
const cchar_t *ccs;
int n;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwadd_wchnstr(%p, %d, %d, %p, %d)", w, y, x, ccs, n);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = wadd_wchnstr(w, ccs, n);

	return __m_return_code("mvwadd_wchnstr", code);
}

#undef add_wchstr

int
add_wchstr(ccs)
const cchar_t *ccs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("add_wchstr(%p)", ccs);
#endif

	code = wadd_wchnstr(stdscr, ccs, -1);

	return __m_return_code("add_wchstr", code);
}

#undef mvadd_wchstr

int
mvadd_wchstr(y, x, ccs)
int y, x;
const cchar_t *ccs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvadd_wchstr(%d, %d, %p)", y, x, ccs);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = wadd_wchnstr(stdscr, ccs, -1);

	return __m_return_code("mvadd_wchstr", code);
}

#undef mvwadd_wchstr

int
mvwadd_wchstr(w, y, x, ccs)
WINDOW *w;
int y, x;
const cchar_t *ccs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwadd_wchstr(%p, %d, %d, %p)", w, y, x, ccs);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = wadd_wchnstr(w, ccs, -1);

	return __m_return_code("mvwadd_wchstr", code);
}

#undef wadd_wchstr

int
wadd_wchstr(w, ccs)
WINDOW *w;
const cchar_t *ccs;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("wadd_wchstr(%p, %p)", w, ccs);
#endif

	code = wadd_wchnstr(w, ccs, -1);

	return __m_return_code("wadd_wchstr", code);
}

