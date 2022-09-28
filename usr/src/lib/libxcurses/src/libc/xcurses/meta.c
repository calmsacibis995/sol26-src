/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)meta.c 1.1	95/12/22 SMI"

/*
 * meta.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/meta.c 1.1 1995/05/18 18:20:54 ant Exp $";
#endif
#endif

#include <private.h>

/*f
 * If true, then input will be 8 bits, else 7.
 * NOTE the window parameter is ignored.
 */
int
meta(WINDOW *w, bool bf)
{
#ifdef M_CURSES_TRACE
	__m_trace("meta(%p, %d)", w, bf);
#endif
	cur_term->_prog.c_cflag &= ~CSIZE;
	cur_term->_prog.c_cflag |= bf ? CS8 : CS7; 

	if (__m_tty_set(&cur_term->_prog) == ERR)
		return __m_return_code("meta", ERR); 

	__m_screen->_flags &= ~S_USE_META;

	if (bf) {
		if (meta_on != (char *) 0)
			(void) tputs(meta_on, 1, __m_outc);
		__m_screen->_flags |= S_USE_META;
	} else if (meta_off != (char *) 0) {
		(void) tputs(meta_off, 1, __m_outc);
	}

	return __m_return_code("meta", OK); 
}

