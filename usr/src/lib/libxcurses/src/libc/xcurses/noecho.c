/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)noecho.c 1.1	95/12/22 SMI"

/*
 * noecho.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/noecho.c 1.1 1995/05/15 15:12:25 ant Exp $";
#endif
#endif

#include <private.h>

int
echo()
{
#ifdef M_CURSES_TRACE
	__m_trace("echo(void)");
#endif

	(void) __m_set_echo(1);

	return __m_return_code("echo", OK);
}

int
noecho()
{
#ifdef M_CURSES_TRACE
	__m_trace("noecho(void)");
#endif

	(void) __m_set_echo(0);

	return __m_return_code("noecho", OK);
}
