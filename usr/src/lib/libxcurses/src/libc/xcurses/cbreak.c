/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)cbreak.c 1.1	95/12/22 SMI"

/*
 * cbreak.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/cbreak.c 1.2 1995/06/19 16:11:50 ant Exp $";
#endif
#endif

#include <private.h>

int
cbreak()
{
#ifdef M_CURSES_TRACE
	__m_trace("cbreak(void)");
#endif

	cur_term->_flags &= ~__TERM_HALF_DELAY;

	cur_term->_prog.c_cc[VMIN] = 1;
	cur_term->_prog.c_cc[VTIME] = 0;
	cur_term->_prog.c_lflag &= ~ICANON;

	return __m_return_code("cbreak", __m_tty_set(&cur_term->_prog));
}

int
nocbreak()
{
#ifdef M_CURSES_TRACE
	__m_trace("nocbreak(void)");
#endif

	cur_term->_flags &= ~__TERM_HALF_DELAY;

	/* On some systems VMIN and VTIME map to VEOF and VEOL, which
	 * means we have to restore them to their original settings.
	 */
	cur_term->_prog.c_cc[VEOF] = cur_term->_shell.c_cc[VEOF];
	cur_term->_prog.c_cc[VEOL] = cur_term->_shell.c_cc[VEOL];
	cur_term->_prog.c_lflag |= ICANON;

	return __m_return_code("nocbreak", __m_tty_set(&cur_term->_prog));
}

/*
 * Set global timeout value, which overrides individual window timeout
 * values (I think believe X/Open specified this wrong).
 */
int
halfdelay(tenths)
int tenths;
{
#ifdef M_CURSES_TRACE
	__m_trace("halfdelay(%d)", tenths);
#endif

	cur_term->_flags |= __TERM_HALF_DELAY;

	cur_term->_prog.c_cc[VMIN] = 0;
	cur_term->_prog.c_cc[VTIME] = tenths;
	cur_term->_prog.c_lflag &= ~ICANON;

	return __m_return_code("halfdelay", __m_tty_set(&cur_term->_prog));
}

int
raw()
{
#ifdef M_CURSES_TRACE
	__m_trace("raw(void)");
#endif

	cur_term->_flags &= ~__TERM_HALF_DELAY;

	cur_term->_prog.c_cc[VMIN] = 1;
	cur_term->_prog.c_cc[VTIME] = 0;
	cur_term->_prog.c_lflag &= ~(ICANON | ISIG | IXON);

	return __m_return_code("raw", __m_tty_set(&cur_term->_prog));
}

int
noraw()
{
#ifdef M_CURSES_TRACE
	__m_trace("noraw(void)");
#endif

	cur_term->_flags &= ~__TERM_HALF_DELAY;

	/* On some systems VMIN and VTIME map to VEOF and VEOL, which
	 * means we have to restore them to their original settings.
	 */
	cur_term->_prog.c_cc[VEOF] = cur_term->_shell.c_cc[VEOF];
	cur_term->_prog.c_cc[VEOL] = cur_term->_shell.c_cc[VEOL];
	cur_term->_prog.c_lflag |= ICANON | ISIG | IXON;

	return __m_return_code("noraw", __m_tty_set(&cur_term->_prog));
}

