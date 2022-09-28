/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)wsyncup.c 1.1	95/12/22 SMI"

/*
 * wsyncup.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/wsyncup.c 1.1 1995/06/21 20:04:28 ant Exp $";
#endif
#endif

#include <private.h>

int
syncok(WINDOW *w, bool bf)
{
#ifdef M_CURSES_TRACE
	__m_trace("syncok(%p, %d)", w, bf);
#endif

	w->_flags &= ~W_SYNC_UP;
	if (bf)
		w->_flags |= W_SYNC_UP;

	return __m_return_code("syncok", OK);
}

void
wsyncup(WINDOW *w)
{
	int y, py;
	WINDOW *p;

#ifdef M_CURSES_TRACE
	__m_trace("wsyncup(%p)", w);
#endif

	for (p = w->_parent; p != (WINDOW *) 0; w = p, p = w->_parent) {
		/* Update the parent's dirty region from the child's. */
		for (py = w->_begy - p->_begy, y = 0; y < w->_maxy; ++y, ++py) {
			if (0 <= w->_last[y]) {
				p->_first[py] = w->_begx + w->_first[y]; 
				p->_last[py] = w->_begx + w->_last[y]; 
			}
		}
	}

	__m_return_void("wsyncup");
}

void
wcursyncup(WINDOW *w)
{
	int y, py;
	WINDOW *p;

#ifdef M_CURSES_TRACE
	__m_trace("wcursyncup(%p)", w);
#endif

	for (p = w->_parent; p != (WINDOW *) 0; w = p, p = w->_parent) {
		p->_cury = w->_begy + w->_cury;
		p->_curx = w->_begx + w->_curx;
	}

	__m_return_void("wcursyncup");
}

