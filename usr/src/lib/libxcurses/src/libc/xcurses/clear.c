/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)clear.c 1.1	95/12/22 SMI"

/*
 * clear.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/clear.c 1.2 1995/07/07 17:59:29 ant Exp $";
#endif
#endif

#include <private.h>

/*
 * Erase window and clear screen next update.
 */
int
(clear)()
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("clear(void)");
#endif

	code = wclear(stdscr);

	return __m_return_code("clear", code); 
}

/*
 * Erase window.
 */
int
(erase)()
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("erase(void)");
#endif

	code = werase(stdscr);

	return __m_return_code("erase", code);
}

