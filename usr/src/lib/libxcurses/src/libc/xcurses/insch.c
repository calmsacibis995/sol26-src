/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)insch.c 1.1	95/12/22 SMI"

/*
 * insch.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/insch.c 1.1 1995/05/11 21:16:17 ant Exp $";
#endif
#endif

#include <private.h>

#undef insch

int
insch(ch)
chtype ch;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("insch(%p)", ch);
#endif

	code = winsch(stdscr, ch);

	return __m_return_code("insch", code);
}

#undef mvinsch

int
mvinsch(y, x, ch)
int y, x;
chtype ch;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvinsch(%d, %d, %p)", y, x, ch);
#endif

	if ((code = wmove(stdscr, y, x)) == OK)
		code = winsch(stdscr, ch);

	return __m_return_code("mvinsch", code);
}

#undef mvwinsch

int
mvwinsch(w, y, x, ch)
WINDOW *w;
int y, x;
chtype ch;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("mvwinsch(%p, %d, %d, %p)", w, y, x, ch);
#endif

	if ((code = wmove(w, y, x)) == OK)
		code = winsch(w, ch);

	return __m_return_code("mvwinsch", code);
}
