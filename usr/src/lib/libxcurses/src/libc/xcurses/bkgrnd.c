/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)bkgrnd.c 1.1	95/12/22 SMI"

/*
 * bkgrnd.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/bkgrnd.c 1.1 1995/06/02 20:26:35 ant Exp $";
#endif
#endif

#include <private.h>

/*f
 * Combine the new background setting with every position in the window.
 * The background is any combination of attributes and a character.
 * Only the attribute part is used to set the background of non-blank
 * characters, while both character and attributes are used for blank
 * positions.
 */
int
(bkgrnd)(bg)
const cchar_t *bg;
{
	int code;

#ifdef M_CURSES_TRACE
	__m_trace("bkgrnd(%p)", bg);
#endif

	code = wbkgrnd(stdscr, bg);

	return __m_return_code("bkgrnd", code);
}
