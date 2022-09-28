/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)waddchn.c 1.1	95/12/22 SMI"

/*
 * waddchn.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/waddchn.c 1.3 1995/06/21 20:30:54 ant Exp $";
#endif
#endif

#include <private.h>

int
waddchnstr(WINDOW *w, const chtype *chs, int n)
{
	cchar_t cc;
	int x, width;

#ifdef M_CURSES_TRACE
	__m_trace("waddchnstr(%p, %p, %d)", w, chs, n);
#endif

	if (n < 0 || w->_maxx < (n += w->_curx))
		n = w->_maxx;

	for (x = w->_curx; x < n && *chs != 0; x += width, ++chs) {
                (void) __m_chtype_cc(*chs, &cc);
		width = __m_cc_replace(w, w->_cury, x, &cc, 0);
        }

	WSYNC(w);

	return __m_return_code("waddchnstr", WFLUSH(w));
}
