/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)vidattr.c 1.1	95/12/22 SMI"

/*
 * vidattr.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/vidattr.c 1.2 1995/07/19 16:38:27 ant Exp $";
#endif
#endif

#include <private.h>

int
vidattr(chtype ch)
{
	int code;
	cchar_t cc;

#ifdef M_CURSES_TRACE
	__m_trace("vidattr(%lx)", ch);
#endif

	(void) __m_chtype_cc(ch, &cc);
	code = vid_puts(cc._at, cc._co, (void *) 0, __m_putchar);

	return __m_return_code("vidattr", code);
}

int
vidputs(chtype ch, int (*putout)(int))
{
	int code;
	cchar_t cc;

#ifdef M_CURSES_TRACE
	__m_trace("vidputs(%lx, %p)", ch, putout);
#endif

	(void) __m_chtype_cc(ch, &cc);
	code = vid_puts(cc._at, cc._co, (void *) 0, putout);

	return __m_return_code("vidputs", code);
}
