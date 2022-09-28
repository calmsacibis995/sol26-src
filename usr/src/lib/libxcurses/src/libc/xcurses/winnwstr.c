/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)winnwstr.c 1.1	95/12/22 SMI"

/*
 * winnwstr.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/winnwstr.c 1.1 1995/06/14 15:26:07 ant Exp $";
#endif
#endif

#include <private.h>

int
winnwstr(w, wcs, n)
WINDOW *w;
wchar_t *wcs;
int n;
{
	int i, y, x;
	cchar_t *cp;

#ifdef M_CURSES_TRACE
	__m_trace("winnwstr(%p, %p, %d)", w, wcs, n);
#endif

	for (x = w->_curx; x < w->_maxx && 0 < n; n -= i) {
		cp = &w->_line[w->_cury][x];

		/* Will entire character fit into buffer? */
		if (n < cp->_n)
			break;

		/* Copy only the character portion. */
		for (i = 0; i < cp->_n; ++i)
			*wcs++ = cp->_wc[i];

		x = __m_cc_next(w, y, x);
	}

	/* Enough room to null terminate the buffer? */
	if (0 < n)
		*wcs = '\0';

	return __m_return_code("winnwstr", OK);
}
