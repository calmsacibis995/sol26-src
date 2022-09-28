/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)winnstr.c 1.1	95/12/22 SMI"

/*
 * winnstr.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/winnstr.c 1.1 1995/06/14 15:26:06 ant Exp $";
#endif
#endif

#include <private.h>

int
winnstr(w, mbs, n)
WINDOW *w;
char *mbs;
int n;
{
	int y, x;

#ifdef M_CURSES_TRACE
	__m_trace("winnstr(%p, %p, %d)", w, mbs, n);
#endif

	y = w->_cury;
	x = w->_curx;

	if (n < 0)
		n = w->_maxx + 1;

	/* Write first character as a multibyte string. */
	(void) __m_cc_mbs(&w->_line[y][x], mbs, n);

	/* Write additional characters without colour and attributes. */
	for (;;) {
		x = __m_cc_next(w, y, x);
		if (w->_maxx <= x)
			break;
		if (__m_cc_mbs(&w->_line[y][x], (char *) 0, 0) < 0)
			break;
	}

        /* Return to initial shift state and terminate string. */
        (void) __m_cc_mbs((const cchar_t *) 0, (char *) 0, 0);
 
	return __m_return_code("winnstr", OK);
}
