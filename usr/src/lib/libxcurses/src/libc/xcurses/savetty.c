/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)savetty.c 1.1	95/12/22 SMI"

/*
 * template.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/savetty.c 1.1 1995/06/20 14:45:06 ant Exp $";
#endif
#endif

#include <private.h>

int
savetty()
{
#ifdef M_CURSES_TRACE
	__m_trace("savetty(void)");
#endif

	return __m_return_code("savetty", __m_tty_get(&cur_term->_save));
}

int
resetty()
{
#ifdef M_CURSES_TRACE
	__m_trace("resetty(void)");
#endif

	return __m_return_code("resetty", __m_tty_set(&cur_term->_save));
}
