/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)get_wch.c 1.1	95/12/22 SMI"

/*
 * get_wch.c
 * 
 * XCurses Library
 *
 * Copyright 1990, 1995 by Mortice Kern Systems Inc.  All rights reserved.
 *
 * This Software is unpublished, valuable, confidential property of
 * Mortice Kern Systems Inc.  Use is authorized only in awcpordance
 * with the terms and conditions of the source licence agreement
 * protecting this Software.  Any unauthorized use or disclosure of
 * this Software is strictly prohibited and will result in the
 * termination of the licence agreement.
 *
 * If you have any questions, please consult your supervisor.
 */

#if M_RCSID
#ifndef lint
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/get_wch.c 1.1 1995/05/25 17:56:22 ant Exp $";
#endif
#endif

#include <private.h>

#undef get_wch

int
get_wch(wcp)
wint_t *wcp;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("get_wch(%p)", wcp);
#endif

	code = wget_wch(stdscr, wcp);

	return __m_return_code("get_wch", code);
}

#undef mvget_wch

int
mvget_wch(y, x, wcp)
int y, x;
wint_t *wcp;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvget_wch(%d, %d, %p)", y, x, wcp);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = wget_wch(stdscr, wcp);

	return __m_return_code("mvget_wch", code);
}

#undef mvwget_wch

int
mvwget_wch(w, y, x, wcp)
WINDOW *w;
int y, x;
wint_t *wcp;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwget_wch(%p, %d, %d, %p)", w, y, x, wcp);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = wget_wch(w, wcp);

	return __m_return_code("mvwget_wch", code);
}

