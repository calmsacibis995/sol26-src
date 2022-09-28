/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)bkgd.c 1.1	95/12/22 SMI"

/*
 * bkgd.c
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

#ifdef M_RCSID
#ifndef lint
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/bkgd.c 1.1 1995/06/07 13:53:06 ant Exp $";
#endif
#endif

#include <private.h>

int
(bkgd)(chtype bg)
{
	int code;
	cchar_t cc;

#ifdef M_CURSES_TRACE
	__m_trace("bkgd(%lx)", bg);
#endif

	if ((code = __m_chtype_cc(bg, &cc)) == OK)
		wbkgrnd(stdscr, &cc);

	return __m_return_code("bkgd", code);
}

int
(wbkgd)(WINDOW *w, chtype bg)
{
	int code;
	cchar_t cc;

#ifdef M_CURSES_TRACE
	__m_trace("wbkgd(%p, %lx)", w, bg);
#endif

	if ((code = __m_chtype_cc(bg, &cc)) == OK)
		wbkgrnd(w, &cc);

	return __m_return_code("wbkgd", code);
}
