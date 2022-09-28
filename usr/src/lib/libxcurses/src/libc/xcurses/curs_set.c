/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)curs_set.c 1.1	95/12/22 SMI"

/*
 * curs_set.c
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

#ifdef M_RCSID
#ifndef lint
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/curs_set.c 1.1 1995/06/05 19:25:21 ant Exp $";
#endif
#endif

#include <private.h>

/*f
 * Turn cursor off/on.  Assume cursor is on to begin with.
 */
int 
curs_set(visibility)
int visibility;
{
	int old;

	/* Assume cursor is initially on. */
	static int cursor_state = 1;

#ifdef M_CURSES_TRACE
	__m_trace("curs_set(%d)", visibility);
#endif

	old = cursor_state;
	switch (visibility) {
	case 0:
		if (cursor_invisible != (char *) 0) {
			(void) tputs(cursor_invisible, 1, __m_outc);
			cursor_state = visibility;
		}
		break;
	case 1:
		if (cursor_normal != (char *) 0) {
			(void) tputs(cursor_normal, 1, __m_outc);
			cursor_state = visibility;
		}
		break;
	case 2:
		if (cursor_visible != (char *) 0) {
			(void) tputs(cursor_visible, 1, __m_outc);
			cursor_state = visibility;
		}
		break;
	}

	return __m_return_int("curs_set", old);
}
