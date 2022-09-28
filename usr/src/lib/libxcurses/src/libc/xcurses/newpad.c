/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)newpad.c 1.1	95/12/22 SMI"

/*
 * newpad.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/newpad.c 1.1 1995/06/16 20:35:07 ant Exp $";
#endif
#endif

#include <private.h>

WINDOW *
(newpad)(nlines, ncols)
int nlines, ncols;
{
	WINDOW *w;

#ifdef M_CURSES_TRACE
	__m_trace("newpad(%d, %d)", nlines, ncols);
#endif

	w = __m_newwin((WINDOW *) 0, nlines, ncols, -1, -1);

	return __m_return_pointer("newpad", w);
}

WINDOW *
(subpad)(parent, nlines, ncols, begy, begx)
WINDOW *parent;
int nlines, ncols, begy, begx;
{
	WINDOW *w;

#ifdef M_CURSES_TRACE
	__m_trace(
		"subpad(%p, %d, %d, %d, %d)", 
		parent, nlines, ncols, begy, begx
	);
#endif

	w = subwin(parent, nlines, ncols, begy, begx);

	return __m_return_pointer("subpad", w);
}

