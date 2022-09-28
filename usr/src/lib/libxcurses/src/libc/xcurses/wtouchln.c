/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)wtouchln.c 1.1	95/12/22 SMI"

/*
 * wtouchln.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/wtouchln.c 1.1 1995/05/12 20:06:28 ant Exp $";
#endif
#endif

#include <private.h>

/*f
 * Given a window, start from line y, and mark n lines either as touched
 * or untouched since the last call to wrefresh().
 */
int
wtouchln(w, y, n, bf)
WINDOW *w;
int y, n, bf;
{
	int first, last;

#ifdef M_CURSES_TRACE
	__m_trace("wtouchln(%p, %d, %d, %d)", w, y, n, bf);
#endif
	first = bf ? 0 : w->_maxx;
	last = bf ? w->_maxx : -1;

	for (; y < w->_maxy && 0 < n; ++y, --n) {
		w->_first[y] = first; 
		w->_last[y] = last; 
	}

	return __m_return_code("wtouchln", OK);
}
