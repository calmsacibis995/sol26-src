/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)is_wintou.c	1.5	92/07/14 SMI"	/* SVr4.0 1.1	*/

#include	"curses_inc.h"

is_wintouched(win)
WINDOW	*win;
{
    return (win->_flags & _WINCHANGED);
}
