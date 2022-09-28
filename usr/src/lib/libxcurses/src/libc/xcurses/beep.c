/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)beep.c 1.1	95/12/22 SMI"

/*
 * beep.c
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
 *
 */

#ifdef M_RCSID
#ifndef lint
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/beep.c 1.2 1995/07/14 20:51:19 ant Exp $";
#endif
#endif

#include <private.h>

/*f
 * Sound the current terminal's audible bell if it has one. If 
 * not, flash the screen if possible.
 */
int
beep()
{
#ifdef M_CURSES_TRACE
	__m_trace("beep(void)");
#endif

	if (bell != (char *) 0)
		(void) tputs(bell, 1, __m_outc);
	else if (flash_screen != (char *) 0)
		(void) tputs(flash_screen, 1, __m_outc);

	(void) fflush(__m_screen->_of);

	return __m_return_code("beep", OK);
}

/*f
 * flash() - Flash the current terminal's screen if possible. If not,
 * sound the audible bell if one exists.
 */
int
flash()
{
#ifdef M_CURSES_TRACE
	__m_trace("flash(void)");
#endif

	if (flash_screen != (char *) 0)
		(void) tputs(flash_screen, 1, __m_outc);
	else if (bell != (char *) 0)
		(void) tputs(bell, 1, __m_outc);

	(void) fflush(__m_screen->_of);

	return __m_return_code("flash", OK);
}
