/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)killchar.c 1.1	95/12/22 SMI"

/*
 * killchar.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/killchar.c 1.1 1995/06/06 13:55:22 ant Exp $";
#endif
#endif

#include <private.h>

int
erasechar()
{
	int ch;

#ifdef M_CURSES_TRACE
	__m_trace("erasechar(void)");
#endif

	/* Refer to _shell instead of _prog, since _shell will 
	 * correctly reflect the user's prefered settings, whereas 
	 * _prog may not have been initialised if both input and 
	 * output have been redirected.
	 */
	ch = cur_term->_shell.c_cc[VERASE];

	return __m_return_int("erasechar", ch);
}

int
killchar()
{
	int ch;

#ifdef M_CURSES_TRACE
	__m_trace("killchar(void)");
#endif

	/* Refer to _shell instead of _prog, since _shell will 
	 * correctly reflect the user's prefered settings, whereas 
	 * _prog may not have been initialised if both input and 
	 * output have been redirected.
	 */
	ch = cur_term->_shell.c_cc[VKILL];

	return __m_return_int("killchar", ch);
}

