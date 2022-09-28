/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)vid_attr.c 1.1	95/12/22 SMI"

/*
 * vid_attr.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/vid_attr.c 1.3 1995/07/19 16:38:09 ant Exp $";
#endif
#endif

#include <private.h>

int
vid_attr(attr_t attr, short pair, void *opts)
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("vid_attr(%x, %d, %p)", attr, pair, opts);
#endif

	code = vid_puts(attr, pair, opts, __m_putchar);

	return __m_return_code("vid_attr", code);
}
