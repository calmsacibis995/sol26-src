/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)clearok.c 1.1	95/12/22 SMI"

/*
 * clearok.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/clearok.c 1.3 1995/06/19 16:12:07 ant Exp $";
#endif
#endif

#include <private.h>

int
clearok(WINDOW *w, bool bf)
{
#ifdef M_CURSES_TRACE
	__m_trace("clearok(%p, %d)", w, bf);
#endif

	w->_flags &= ~W_CLEAR_WINDOW;
	if (bf)
		w->_flags |= W_CLEAR_WINDOW;

	return __m_return_code("clearok", OK);
}

void
immedok(WINDOW *w, bool bf)
{
#ifdef M_CURSES_TRACE
	__m_trace("immedok(%p, %d)", w, bf);
#endif

	w->_flags &= ~W_FLUSH;
	if (bf)
		w->_flags |= W_FLUSH;

	__m_return_void("immedok");
}

int
leaveok(WINDOW *w, bool bf)
{
#ifdef M_CURSES_TRACE
	__m_trace("leaveok(%p, %d)", w, bf);
#endif

	w->_flags &= ~W_LEAVE_CURSOR;
	if (bf)
		w->_flags |= W_LEAVE_CURSOR;

	return __m_return_code("leaveok", OK);
}

int
notimeout(WINDOW *w, bool bf)
{
#ifdef M_CURSES_TRACE
	__m_trace("notimeout(%p, %d)", w, bf);
#endif

	w->_flags &= ~W_USE_TIMEOUT;
	if (!bf)
		w->_flags |= W_USE_TIMEOUT;

	return __m_return_code("notimeout", OK);
}

int
scrollok(WINDOW *w, bool bf)
{
#ifdef M_CURSES_TRACE
	__m_trace("scrollok(%p, %d)", w, bf);
#endif

	w->_flags &= ~W_CAN_SCROLL;
	if (bf)
		w->_flags |= W_CAN_SCROLL;

	return __m_return_code("scrollok", OK);
}

