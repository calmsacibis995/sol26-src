/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)chgat.c 1.1	95/12/22 SMI"

/*
 * chgat.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/chgat.c 1.1 1995/06/05 19:04:08 ant Exp $";
#endif
#endif

#include <private.h>

int
(chgat)(int n, attr_t at, short co, const void *opts)
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("chgat(%d, %x, %d, %p)", n, at, co, opts);
#endif

	code = wchgat(stdscr, n, at, co, opts);

	return __m_return_code("chgat", code);
}

int
(mvchgat)(int y, int x, int n, attr_t at, short co, const void *opts)
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvchgat(%d, %d, %d, %x, %d, %p)", y, x, n, at, co, opts);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = wchgat(stdscr, n, at, co, opts);

	return __m_return_code("mvchgat", code);
}

int
(mvwchgat)(
	WINDOW *w, int y, int x, int n, attr_t at, short co, const void *opts)
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace(
		"mvwchgat(%p, %d, %d, %d, %x, %d, %p)", 
		w, y, x, n, at, co, opts
	);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = wchgat(w, n, at, co, opts);

	return __m_return_code("mvwchgat", code);
}
