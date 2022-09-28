/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)_mvwinstr.c	1.5	92/07/14 SMI"	/* SVr4.0 1.2	*/

#define		NOMACROS
#include	"curses_inc.h"

mvwinstr(win, y, x, s)
WINDOW *win;
int	y, x;
char	*s;
{
    return (wmove(win,y,x)==ERR?ERR:winstr(win,s));
}
