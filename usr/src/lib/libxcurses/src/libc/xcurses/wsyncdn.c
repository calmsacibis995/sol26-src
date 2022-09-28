/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)wsyncdn.c 1.1	95/12/22 SMI"

/*
 * wsyncdn.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/wsyncdn.c 1.1 1995/06/21 20:04:26 ant Exp $";
#endif
#endif

#include <private.h>

static void
syncdown(WINDOW *p, WINDOW *w)
{
	int y, py;

	if (p == (WINDOW *) 0)
		return;

	syncdown(p->_parent, p);

	for (py = w->_begy - p->_begy, y = 0; y < w->_maxy; ++y, ++py) {
		if (0 <= p->_last[py]) {
			w->_first[y] = p->_first[py] - w->_begx;
			w->_last[y] = p->_last[py] - w->_begx;
		}
	}
}

void
wsyncdown(WINDOW *w)
{
#ifdef M_CURSES_TRACE
	__m_trace("wsyncdown(%p)", w);
#endif

	syncdown(w->_parent, w);

	__m_return_void("wsyncdown");
}
