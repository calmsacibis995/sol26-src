/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)box.c 1.1	95/12/22 SMI"

/*
 * box.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/box.c 1.1 1995/05/26 19:11:36 ant Exp $";
#endif
#endif

#include <private.h>

#undef box

int
box(WINDOW *w, chtype v, chtype h)
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("box(%p, %ld, %ld)", w, v, h);
#endif

	code = wborder(w, v, v, h, h, 0, 0, 0, 0);

	return __m_return_code("box", code);
}
