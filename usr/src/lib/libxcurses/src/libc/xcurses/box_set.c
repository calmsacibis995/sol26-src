/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)box_set.c 1.1	95/12/22 SMI"

/*
 * box_set.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/box_set.c 1.1 1995/05/26 19:11:37 ant Exp $";
#endif
#endif

#include <private.h>

#undef box_set

int
box_set(w, v, h)
WINDOW *w; 
const cchar_t *v, *h;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("box_set(%p, %p, %p)", w, v, h);
#endif

	code = wborder_set(
		w, v, v, h, h, 
		(const cchar_t *) 0, (const cchar_t *) 0, 
		(const cchar_t *) 0, (const cchar_t *) 0
	);

	return __m_return_code("box_set", code);
}
