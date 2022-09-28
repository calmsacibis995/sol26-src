/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)initscr.c 1.1	95/12/22 SMI"

/*
 * initscr.c		
 *
 * XCurses Library
 *
 * Copyright 1986, 1994 by Mortice Kern Systems Inc.  All rights reserved.
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/initscr.c 1.5 1995/09/28 16:47:03 ant Exp $";
#endif
#endif

#include <private.h>
#include <errno.h>
#include <stdlib.h>

static char nomem_msg[] = m_textstr(
	3139, "Failed to allocate required memory.\n", "E"
);
static char noterm_msg[] = m_textstr(
	202, "Unknown terminal \"%s\".\n", "E term"
);
static char dumb_msg[] = m_textstr(
	3140, "Terminal \"%s\" has insufficent capabilities for Curses.\n", 
	"E term"
);

/*f
 * Initialize XCurses for use with a single terminal.  stdin and stdout
 * are used.  If a program needs an indication of error conditions,
 * so that it can continue to run in a line-oriented mode, use newterm()
 * instead.
 */
WINDOW *
initscr()
{
	WINDOW *w;
	SCREEN *sp;
	int i, n, begy;
	char *term, *err;

#ifdef M_CURSES_TRACE
	__m_trace("initscr(void)");
#endif

	errno = 0;
 	sp = newterm((char *) 0, stdout, stdin);

	if (sp == (SCREEN *) 0) {
		err = errno == ENOMEM ? nomem_msg : noterm_msg;
		goto error_1;
	}

	(void) set_term(sp);

	/* We require some form of cursor positioning and the ability to
	 * clear the end of a line.  These abilities should be sufficient
	 * to provide minimum full screen support. 
	 */
	if (1 < lines
	&& cursor_address == (char *) 0 
	&& row_address == (char *) 0
	&& (cursor_up == (char *) 0 || cursor_down == (char *) 0)
	&& (parm_up_cursor == (char *) 0 || parm_down_cursor == (char *) 0)) {
		err = dumb_msg;
		goto error_3;
	}
	
	if ((1 < lines && cursor_address == (char *) 0)
	&& column_address == (char *) 0
	&& (cursor_left == (char *) 0 || cursor_right == (char *) 0)
	&& (parm_left_cursor == (char *) 0 || parm_right_cursor == (char *)0)) {
		err = dumb_msg;
		goto error_3;
	}

	if (clr_eol == (char *) 0) {
		err = dumb_msg;
		goto error_3;
	}

	return __m_return_pointer("initscr", stdscr);
error_3:
	(void) delwin(stdscr);
error_2:
	(void) endwin();
	(void) delscreen(sp);
error_1:
	/* newterm()/setupterm() attempts to load $TERM, else if
	 * $TERM is not defined, the vendor's default terminal type.
	 */
	if ((term = getenv("TERM")) == (char *) 0)
		term = M_TERM_NAME;

	(void) fprintf(stderr, m_strmsg(err), term);
error_0:
	exit(1);
}

