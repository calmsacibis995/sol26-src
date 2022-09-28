/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)winch.c 1.1	95/12/22 SMI"

/*
 * winch.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/winch.c 1.1 1995/06/12 20:23:46 ant Exp $";
#endif
#endif

#include <private.h>

chtype
winch(w)
WINDOW *w;
{
	chtype ch;

#ifdef M_CURSES_TRACE
	__m_trace("winch(%p)", w);
#endif

	ch = __m_cc_chtype(&w->_line[w->_cury][w->_curx]);

	return __m_return_chtype("winch", ch);
}
