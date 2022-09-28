/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)isendwin.c 1.1	95/12/22 SMI"

/*
 * isendwin.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/isendwin.c 1.1 1995/05/12 18:39:34 ant Exp $";
#endif
#endif

#include <private.h>

/*f
 * Return TRUE if endwin() has been called without any subsequent 
 * calls to wrefresh()/doupdate(), else FALSE.
 *
 * This function is not a macro because the structure of SCREEN is
 * not public knowledge.
 */
bool
isendwin()
{
	int value;

#ifdef M_CURSES_TRACE
	__m_trace("isendwin(void)");
#endif

	value = __m_screen != (SCREEN *) 0 && (__m_screen->_flags & S_ENDWIN);

	return __m_return_int("isendwin", value);
}
