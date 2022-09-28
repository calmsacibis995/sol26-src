/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)endwin.c 1.1	95/12/22 SMI"

/*
 * endwin.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/endwin.c 1.5 1995/07/19 16:37:58 ant Exp $";
#endif
#endif

#include <private.h>
#include <signal.h>


/*f
 * Restore tty modes, moves the cursor to the lower left hand 
 * corner of the screen and resets the terminal into proper non-visual 
 * mode.  Calling doupdate()/wrefresh() will resume visual mode. 
 */
int 
endwin()
{
#ifdef M_CURSES_TRACE
	__m_trace("endwin(void)");
#endif
	if (!(__m_screen->_flags & S_ENDWIN)) {
		__m_mvcur(-1, -1, lines-1, 0, __m_outc);

		if (exit_ca_mode != (char *) 0)
			(void) tputs(exit_ca_mode, 1, __m_outc);

		if (keypad_local != (char *) 0)
			(void) tputs(keypad_local, 1, __m_outc);

		if (orig_colors != (char *) 0)
			(void) tputs(orig_colors, 1, __m_outc);
		
		/* Make sure the current attribute state is normal.*/
		if (ATTR_STATE != WA_NORMAL) {
			(void) vid_puts(WA_NORMAL, 0, (void *) 0, __m_outc);

			if (ceol_standout_glitch)
				curscr->_line[curscr->_maxx-1][0]._at 
					|= WA_COOKIE;
		}

		(void) signal(SIGTSTP, SIG_DFL);
		__m_screen->_flags = S_ENDWIN;
	}

	(void) fflush(__m_screen->_of);
	(void) reset_shell_mode();

	return __m_return_code("endwin", OK);
}

