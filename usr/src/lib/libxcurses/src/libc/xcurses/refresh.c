/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)refresh.c 1.1	95/12/22 SMI"

/*
 * refresh.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/refresh.c 1.1 1995/05/11 21:16:22 ant Exp $";
#endif
#endif

#include <private.h>

#undef refresh

int
refresh()
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("refresh(void)");
#endif

	code = wrefresh(stdscr);

	return __m_return_code("wrefresh", code);
}
