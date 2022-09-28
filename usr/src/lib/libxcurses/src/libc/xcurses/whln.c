/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)whln.c 1.1	95/12/22 SMI"

/*
 * whln.c		
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/whln.c 1.1 1995/06/07 14:18:03 ant Exp $";
#endif
#endif

#include <private.h>

int
whline(WINDOW *w, chtype h, int n)
{
	int code;
	cchar_t cc;

#ifdef M_CURSES_TRACE
	__m_trace("whline(%p, %ld, %d)",  w, h, n);
#endif

        (void) __m_chtype_cc(h, &cc);
        code = whline_set(w, &cc, n);

	return __m_return_code("whline", code);
}

int
wvline(WINDOW *w, chtype v, int n)
{
	int code;
	cchar_t cc;

#ifdef M_CURSES_TRACE
	__m_trace("whline(%p, %ld, %d)",  w, v, n);
#endif

        (void) __m_chtype_cc(v, &cc);
        code = wvline_set(w, &cc, n);

	return __m_return_code("wvline", code);
}

