/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)touched.c 1.1	95/12/22 SMI"

/*
 * touched.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/touched.c 1.2 1995/06/15 18:42:55 ant Exp $";
#endif
#endif

#include <private.h>

/*
 * Return true if line has been touched.  See wtouchln().
 *
 *	touched 	0 <= _first[y] <= _last[y] <= _maxx
 * 	untouched	_last[y] < 0 < _maxx <= _first[y].  
 */
bool
(is_linetouched)(w, y)
WINDOW *w;
int y;
{
#ifdef M_CURSES_TRACE
	__m_trace("is_linetouched(%p, %d)", w, y);
#endif

	return __m_return_int("is_linetouched", 0 <= w->_last[y]);
}

bool
(is_wintouched)(w)
WINDOW *w;
{
	int y, value;

#ifdef M_CURSES_TRACE
	__m_trace("is_linetouched(%p, %d)", w, y);
#endif

	for (y = 0; y < w->_maxy; ++y)
		if ((value = 0 <= w->_last[y]))
			break;
	
	return __m_return_int("is_linetouched", value);
}

