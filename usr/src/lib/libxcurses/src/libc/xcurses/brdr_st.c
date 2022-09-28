/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)brdr_st.c 1.1	95/12/22 SMI"

/*
 * brdr_st.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/brdr_st.c 1.1 1995/05/29 18:52:15 ant Exp $";
#endif
#endif

#include <private.h>

#undef border_set

int
border_set(ls, rs, ts, bs, tl, tr, bl, br)
const cchar_t *ls, *rs, *ts, *bs, *tl, *tr, *bl, *br;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace(
		"border_set(%p, %p, %p, %p, %p, %p, %p, %p)", 
		ls, rs, ts, bs, tl, tr, bl, br
	);
#endif

	code = wborder_set(stdscr, ls, rs, ts, bs, tl, tr, bl, br);

	return __m_return_code("border_set", code);
}

