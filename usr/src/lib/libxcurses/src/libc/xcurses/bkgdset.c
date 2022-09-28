/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)bkgdset.c 1.1	95/12/22 SMI"

/*
 * bkgdset.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/bkgdset.c 1.2 1995/06/12 20:24:16 ant Exp $";
#endif
#endif

#include <private.h>

int
(bkgdset)(chtype bg)
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("bkgdset(%lx)", bg);
#endif

	code = __m_chtype_cc(bg, &stdscr->_bg);

	return __m_return_code("bkgdset", code);
}

int
(wbkgdset)(WINDOW *w, chtype bg)
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("wbkgdset(%p, %lx)", w, bg);
#endif

	code = __m_chtype_cc(bg, &w->_bg);

	return __m_return_code("wbkgdset", code);
}

chtype
(getbkgd)(WINDOW *w)
{
	chtype bg;

#ifdef M_CURSES_TRACE
	__m_trace("getbkgd(%p)", w);
#endif

	bg = __m_cc_chtype(&w->_bg);

	return __m_return_chtype("getbkgd", bg);
}
