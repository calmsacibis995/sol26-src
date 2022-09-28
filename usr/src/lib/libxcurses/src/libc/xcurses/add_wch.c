/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)add_wch.c 1.1	95/12/22 SMI"

/*
 * add_wch.c
 * 
 * XCurses Library
 *
 * Copyright 1990, 1995 by Mortice Kern Systems Inc.  All rights reserved.
 *
 * This Software is unpublished, valuable, confidential property of
 * Mortice Kern Systems Inc.  Use is authorized only in accordance
 * with the terms and conditions of the source licence agreement
 * protecting this Software.  Any unauthorized use or disclosure of
 * this Software is strictly prohibited and will result in the
 * termination of the licence agreement.
 *
 * If you have any questions, please consult your supervisor.
 */

#if M_RCSID
#ifndef lint
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/add_wch.c 1.3 1995/07/07 17:58:47 ant Exp $";
#endif
#endif

#include <private.h>

int
(add_wch)(cc)
const cchar_t *cc;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("add_wch(%p)", cc);
#endif

	code = wadd_wch(stdscr, cc);

	return __m_return_code("add_wch", code);
}

int
(mvadd_wch)(y, x, cc)
int y, x;
const cchar_t *cc;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvadd_wch(%d, %d, %p)", y, x, cc);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = wadd_wch(stdscr, cc);

	return __m_return_code("mvadd_wch", code);
}

int
(mvwadd_wch)(w, y, x, cc)
WINDOW *w;
int y, x;
const cchar_t *cc;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwadd_wch(%p, %d, %d, %p)", w, y, x, cc);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = wadd_wch(w, cc);

	return __m_return_code("mvwadd_wch", code);
}
