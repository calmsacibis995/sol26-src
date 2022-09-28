/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)inch.c 1.1	95/12/22 SMI"

/*
 * inch.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/inch.c 1.1 1995/06/12 20:24:39 ant Exp $";
#endif
#endif

#include <private.h>

chtype
(inch)()
{
	chtype ch;

#ifdef M_CURSES_TRACE
	__m_trace("inch(void)");
#endif

	ch = winch(stdscr);

	return __m_return_chtype("inch", ch);
}

chtype
(mvinch)(y, x)
int y, x;
{
	chtype ch;

#ifdef M_CURSES_TRACE
	__m_trace("mvinch(%d, %d)", y, x);
#endif

	if ((ch = (chtype) wmove(stdscr, y, x)) != (chtype) ERR)
		ch = winch(stdscr);

	return __m_return_chtype("mvinch", ch);
}

chtype
(mvwinch)(w, y, x)
WINDOW *w;
int y, x;
{
	chtype ch;

#ifdef M_CURSES_TRACE
	__m_trace("mvwinch(%p, %d, %d)", w, y, x);
#endif

	if ((ch = (chtype) wmove(w, y, x)) != (chtype) ERR)
		ch = winch(w);

	return __m_return_chtype("mvwinch", ch);
}
