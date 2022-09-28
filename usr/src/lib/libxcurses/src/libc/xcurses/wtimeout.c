/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)wtimeout.c 1.1	95/12/22 SMI"

/*
 * wtimeout.c
 *
 * XCurses Library
 *
 * Copyright 1990, 1994 by Mortice Kern Systems Inc.  All rights reserved.
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/wtimeout.c 1.1 1995/06/19 16:12:14 ant Exp $";
#endif
#endif

#include <private.h>

/*f
 * Set blocking or non-blocking read for a specified window.
 * The delay is in milliseconds.
 */
void
wtimeout(w, delay)
WINDOW *w;
int delay;
{
#ifdef M_CURSES_TRACE
	__m_trace("wtimeout(%p, %d)", w, delay);
#endif

	if (delay < 0) {
		/* Blocking mode */
		w->_vmin = 1;
		w->_vtime = 0;
	} else {
		/* Non-Block (0 == delay) and delayed (0 < delay) */
		w->_vmin = 0;

		/* VTIME is in 1/10 of second */
		w->_vtime = (delay+50)/100;	
	}

	__m_return_void("wtimeout");
}

