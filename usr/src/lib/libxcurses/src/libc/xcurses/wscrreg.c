/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)wscrreg.c 1.1	95/12/22 SMI"

/*
 * wscrreg.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/wscrreg.c 1.1 1995/06/05 19:19:48 ant Exp $";
#endif
#endif

#include <private.h>

int
wsetscrreg(w, top, bottom)
WINDOW *w;
int top, bottom;
{
#ifdef M_CURSES_TRACE
	__m_trace("wsetscrreg(%p, %d, %d)", w, top, bottom);
#endif

	if (top < 0 || bottom < top || w->_maxy <= bottom)
		return __m_return_code("wsetscrreg", ERR);

	/* Set _top (inclusive) to _bottom (exclusive). */
	w->_top = top;
	w->_bottom = bottom + 1;

	return __m_return_code("wsetscrreg", OK);
}
