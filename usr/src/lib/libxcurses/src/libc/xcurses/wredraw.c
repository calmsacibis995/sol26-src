/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)wredraw.c 1.1	95/12/22 SMI"

/*
 * wredraw.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/wredraw.c 1.1 1995/06/20 14:34:17 ant Exp $";
#endif
#endif

#include <private.h>

int
wredrawln(w, y, n)
WINDOW *w;
int y, n;
{
#ifdef M_CURSES_TRACE
	__m_trace("wredrawln(%p, %d, %d)", w, y, n);
#endif

	w->_flags |= W_REDRAW_WINDOW;
 
        for (; y < w->_maxy && 0 < n; ++y, --n) {
                w->_first[y] = 0;
                w->_last[y] = w->_maxx;
        }

	return __m_return_code("wredrawln", OK);
}
