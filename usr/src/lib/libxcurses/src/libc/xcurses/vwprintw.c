/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)vwprintw.c 1.2	96/02/16 SMI"

/*
 * vwprintw.c
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

#if M_RCSID
#ifndef lint
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/vwprintw.c 1.2 1995/08/30 19:39:01 danv Exp $";
#endif
#endif

#include <private.h>
#include <limits.h>

#undef va_start
#undef va_end
#undef va_arg

#ifdef M_USE_VARARGS_H
#include <varargs.h>
#else
#include <stdarg.h>
#endif

int
vwprintw(w, fmt, ap)
WINDOW *w;
const char *fmt;
#if defined(sun)
va_list ap;
#else
void *ap;
#endif
{
	static char buffer[LINE_MAX];

#ifdef M_CURSES_TRACE
	__m_trace("vwprintw(%p, %p = \"%s\", %p)", w, fmt, fmt, ap);
#endif

	/* Assume that sizeof buffer is sufficently large to
	 * format a string without causing a seg. fault.
	 */
#if defined(sun)
	(void) vsprintf(buffer, fmt, ap);
#else
	(void) vsprintf(buffer, fmt, (va_list) ap);
#endif

	return __m_return_code("vwprintw", waddnstr(w, buffer, -1));
}
