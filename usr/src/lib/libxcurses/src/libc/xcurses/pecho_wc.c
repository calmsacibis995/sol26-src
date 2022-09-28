/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)pecho_wc.c 1.1	95/12/22 SMI"

/*
 * pecho_wc.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/pecho_wc.c 1.1 1995/06/20 13:31:51 ant Exp $";
#endif
#endif

#include <private.h>

int
pecho_wchar(WINDOW *pad, const cchar_t *cc)
{
	int code, dy, dx;

#ifdef M_CURSES_TRACE
	__m_trace("pecho_wchar(%p, %p)", pad, cc);
#endif

	/* Compute height and width of inclusive region. */ 
	dy = pad->_smaxy - pad->_sminy;
	dx = pad->_smaxx - pad->_sminx;

	/* Is the logical cursor within the previously displayed region? */
	if (pad->_cury < pad->_refy || pad->_curx < pad->_refx
	|| pad->_refy + dy < pad->_cury || pad->_refx + dx < pad->_curx)
		return __m_return_code("pecho_wchar", ERR);

	/* Add the character to the pad. */
	if ((code = wadd_wch(pad, cc)) == OK) {
		/* Redisplay previous region. */
		code = prefresh(
			pad, pad->_refy, pad->_refx,
			pad->_sminy, pad->_sminx, pad->_smaxy, pad->_smaxx
		);
	}

	return __m_return_code("pecho_wchar", code);
}
