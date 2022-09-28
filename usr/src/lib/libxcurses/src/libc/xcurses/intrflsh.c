/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)intrflsh.c 1.1	95/12/22 SMI"

/*
 * intrflsh.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/intrflsh.c 1.1 1995/06/15 18:39:17 ant Exp $";
#endif
#endif

#include <private.h>

/*
 * When set, pressing an interrupt, suspend, or quiit key, the terminal's
 * output queue will be flushed.  Default inherited from the terminal
 * driver.  The window parameter is ignored. 
 */
int
(intrflush)(WINDOW *w, bool bf)
{
#ifdef M_CURSES_TRACE
	__m_trace("intrflush(%p, %d)", w, bf);
#endif

	cur_term->_prog.c_lflag &= ~NOFLSH;
	if (!bf)
		cur_term->_prog.c_lflag |= NOFLSH;
		
	return __m_return_code("intrflush", __m_tty_set(&cur_term->_prog));
}
