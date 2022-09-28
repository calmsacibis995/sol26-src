/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)qiflush.c 1.1	95/12/22 SMI"

/*
 * qiflush.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/qiflush.c 1.1 1995/06/15 18:39:15 ant Exp $";
#endif
#endif

#include <private.h>

void
(qiflush)()
{
#ifdef M_CURSES_TRACE
	__m_trace("qiflush(void)");
#endif

	cur_term->_prog.c_lflag &= ~NOFLSH;
	(void) __m_tty_set(&cur_term->_prog);

	__m_return_void("qiflush");
}

void
(noqiflush)()
{
#ifdef M_CURSES_TRACE
	__m_trace("noqiflush(void)");
#endif

	cur_term->_prog.c_lflag |= NOFLSH;
	(void) __m_tty_set(&cur_term->_prog);

	__m_return_void("noqiflush");
}
