/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)overlay.c 1.1	95/12/22 SMI"

/*
 * overlay.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/overlay.c 1.3 1995/09/19 19:15:49 ant Exp $";
#endif
#endif

#include <private.h>

int
(overlay)(s, t)
const WINDOW *s;
WINDOW *t;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("overlay(%p, %p)", s, t);
#endif

	code = __m_copywin(s, t, 1);

	return __m_return_code("overlay", ERR);
}

int
(overwrite)(s, t)
const WINDOW *s;
WINDOW *t;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("overwrite(%p, %p)", s, t);
#endif

	code = __m_copywin(s, t, 0);

	return __m_return_code("overwrite", ERR);
}
