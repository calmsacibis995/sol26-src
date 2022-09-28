/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)brdr.c 1.1	95/12/22 SMI"

/*
 * brdr.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/brdr.c 1.1 1995/05/29 18:52:12 ant Exp $";
#endif
#endif

#include <private.h>

#undef border

int
border(chtype ls, chtype rs, chtype ts, chtype bs, 
	chtype tl, chtype tr, chtype bl, chtype br) 
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace(
		"border(%ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld)", 
		ls, rs, ts, bs, tl, tr, bl, br
	);
#endif

	code = wborder(stdscr, ls, rs, ts, bs, tl, tr, bl, br);

	return __m_return_code("border", code);
}

