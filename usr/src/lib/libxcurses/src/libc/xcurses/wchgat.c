/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)wchgat.c 1.1	95/12/22 SMI"

/*
 * wchgat.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/wchgat.c 1.3 1995/06/21 20:30:57 ant Exp $";
#endif
#endif

#include <private.h>

int
wchgat(WINDOW *w, int n, attr_t at, short co, const void *opts)
{
	int i, x;
	cchar_t *cp;

#ifdef M_CURSES_TRACE
	__m_trace("wchgat(%p, %d, %x, %d, %p)", w, n, at, co, opts);
#endif
	
	if (n < 0)
		n = w->_maxx;

	cp = &w->_line[w->_cury][w->_maxx];

	if (!cp->_f)
		return __m_return_code("wchgat", ERR);

	for (i = 0, x = w->_curx; x < w->_maxx; ++x, ++cp) {
		if (cp->_f && n <= i++)
			break;

		cp->_co = co;
		cp->_at = at; 
	}
		
	WSYNC(w);

	return __m_return_code("wchgat", WFLUSH(w));
}
