/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)killwch.c 1.1	95/12/22 SMI"

/*
 * killwchar.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/killwch.c 1.2 1995/06/07 12:44:13 ant Exp $";
#endif
#endif

#include <private.h>

int
(erasewchar)(wcp)
wchar_t *wcp;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("erasewchar(%p)", wcp);
#endif

	code = __m_tty_wc(VERASE, wcp);

	return __m_return_int("erasewchar", code);
}

int
(killwchar)(wcp)
wchar_t *wcp;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("killwchar(%p)", wcp);
#endif

	code = __m_tty_wc(VKILL, wcp);

	return __m_return_int("killwchar", code);
}

