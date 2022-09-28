/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)clrbot.c 1.1	95/12/22 SMI"

/*
 * clrbot.c
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

#ifdef M_RCSID
#ifndef lint
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/clrbot.c 1.1 1995/06/07 13:54:35 ant Exp $";
#endif
#endif

#include <private.h>

#undef clrtobot

/*f
 * Erase from the current cursor location right and down to the end of
 * the screen. The cursor position is not changed.
 */
int
clrtobot()
{
	int x, value;

#ifdef M_CURSES_TRACE
	__m_trace("clrtobot(void) from (%d, %d)", stdscr->_cury, stdscr->_curx);
#endif

	x = __m_cc_first(stdscr, stdscr->_cury, stdscr->_curx);
	value = __m_cc_erase(
		stdscr, stdscr->_cury, x, stdscr->_maxy-1, stdscr->_maxx-1
	);

	return __m_return_code("clrtobot", value == 0 ? OK : ERR);
}
