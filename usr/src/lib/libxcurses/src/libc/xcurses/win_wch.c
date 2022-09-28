/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)win_wch.c 1.1	95/12/22 SMI"

/*
 * win_ch.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/win_wch.c 1.2 1995/07/06 19:53:43 ant Exp $";
#endif
#endif

#include <private.h>

int
win_wch(w, cc)
WINDOW *w;
cchar_t *cc;
{
#ifdef M_CURSES_TRACE
	__m_trace("win_wch(%p, %p)", w, cc);
#endif

	*cc = w->_line[w->_cury][w->_curx];
	cc->_f = 1;

	return __m_return_code("win_wch", OK);
}
