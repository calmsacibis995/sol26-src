/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)flushinp.c 1.1	95/12/22 SMI"

/*
 * flushinp.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/flushinp.c 1.1 1995/06/06 14:13:41 ant Exp $";
#endif
#endif

#include <private.h>

/*f
 * Throw away any typeahead that has been typed by the user 
 * and has not yet been read by the program.
 */
int
flushinp()
{
	int fd;

#ifdef M_CURSES_TRACE
	__m_trace("flushinp(void)");
#endif

	if (!ISEMPTY())
		RESET();

        if (cur_term->_flags & __TERM_ISATTY_IN)
                fd = cur_term->_ifd;
        else if (cur_term->_flags & __TERM_ISATTY_OUT)
                fd = cur_term->_ofd;
	else
		fd = -1;

	if (0 <= fd)
		(void) tcflush(fd, TCIFLUSH);

	return __m_return_code("flushinp", OK);
}
