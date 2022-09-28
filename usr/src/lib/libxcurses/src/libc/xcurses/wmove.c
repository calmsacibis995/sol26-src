/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)wmove.c 1.1	95/12/22 SMI"

/*
 * wmove.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/wmove.c 1.1 1995/05/12 18:30:18 ant Exp $";
#endif
#endif

#include <private.h>

int
wmove(w, y, x)
WINDOW *w;
int y, x;
{
#ifdef M_CURSES_TRACE
	__m_trace("wmove(%p,  %d, %d)", w, y, x);
#endif
	if (y < 0 || w->_maxy <= y || x < 0 || w->_maxx <= x)
		return __m_return_code("wmove", ERR);
	
	w->_cury = y;
	w->_curx = x;

	return __m_return_code("wmove", OK);
}
