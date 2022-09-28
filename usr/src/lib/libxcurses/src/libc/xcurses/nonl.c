/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)nonl.c 1.1	95/12/22 SMI"

/*
 * nonl.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/nonl.c 1.1 1995/05/15 15:12:23 ant Exp $";
#endif
#endif

#include <private.h>

/*
 * Enable mappnig of cr -> nl on input and nl -> crlf on output. 
 */
int
nl()
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("nl(void)");
#endif

	cur_term->_prog.c_iflag |= ICRNL;
	cur_term->_prog.c_oflag |= OPOST;
#ifdef ONLCR
	cur_term->_prog.c_oflag |= ONLCR;
#endif

	if ((code = __m_tty_set(&cur_term->_prog)) == OK)
		cur_term->_flags |= __TERM_NL_IS_CRLF;

	return __m_return_code("nl", code);
}

/*
 * Disable mappnig of cr -> nl on input and nl -> crlf on output. 
 */
int
nonl()
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("nonl(void)");
#endif

	cur_term->_prog.c_iflag &= ~ICRNL;
#if ONLCR
	cur_term->_prog.c_oflag &= ~ONLCR;
#else
	cur_term->_prog.c_oflag &= ~OPOST;
#endif

	if ((code = __m_tty_set(&cur_term->_prog)) == OK)
		cur_term->_flags &= ~__TERM_NL_IS_CRLF;

	return __m_return_code("nonl", code);
}
