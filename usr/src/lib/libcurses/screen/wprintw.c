/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)wprintw.c	1.5	92/07/14 SMI"	/* SVr4.0 1.10	*/

/*
 * printw and friends
 *
 */

# include	"curses_inc.h"

#ifdef __STDC__
#include	<stdarg.h>
#else
#include <varargs.h>
#endif

/*
 *	This routine implements a printf on the given window.
 */
/*VARARGS*/
#ifdef __STDC__
wprintw(WINDOW *win, ...)
#else
wprintw(va_alist)
va_dcl
#endif
{
	va_list ap;
#ifndef __STDC__
	register WINDOW	*win;
#endif
	register char * fmt;

#ifdef __STDC__
	va_start(ap, win);
#else
	va_start(ap);
	win = va_arg(ap, WINDOW *);
#endif
	fmt = va_arg(ap, char *);
	return vwprintw(win, fmt, ap);
}
