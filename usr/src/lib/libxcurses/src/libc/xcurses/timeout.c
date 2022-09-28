/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)timeout.c 1.1	95/12/22 SMI"

/*
 * timeout.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/timeout.c 1.1 1995/06/19 16:12:10 ant Exp $";
#endif
#endif

#include <private.h>

int
(nodelay)(WINDOW *w, bool bf)
{
#ifdef M_CURSES_TRACE
	__m_trace("nodelay(%p, %d)", w, bf);
#endif

	wtimeout(w, bf ? 0 : -1);

	return __m_return_code("nodelay", OK);
}

void
(timeout)(int delay)
{
#ifdef M_CURSES_TRACE
	__m_trace("timeout(%d)", delay);
#endif

	wtimeout(stdscr, delay);

	__m_return_void("timeout");
}

