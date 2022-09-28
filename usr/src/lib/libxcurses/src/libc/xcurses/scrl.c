/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)scrl.c 1.1	95/12/22 SMI"

/*
 * scrl.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/scrl.c 1.1 1995/05/12 19:37:24 ant Exp $";
#endif
#endif

#include <private.h>

#undef scroll

int
scroll(w)
WINDOW *w;
{
#ifdef M_CURSES_TRACE
	__m_trace("scroll(%p)", w);
#endif

	return __m_return_code("scroll", wscrl(w, 1));
}

#undef scrl

int
scrl(n)
int n;
{
#ifdef M_CURSES_TRACE
	__m_trace("scrl(%d)", n);
#endif

	return __m_return_code("scrl", wscrl(stdscr, n));
}
