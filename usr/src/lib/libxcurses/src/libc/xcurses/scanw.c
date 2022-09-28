/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)scanw.c 1.1	95/12/22 SMI"

/*
 * scanw.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/scanw.c 1.2 1995/07/14 20:50:28 ant Exp $";
#endif
#endif

#include <private.h>
#include <limits.h>
#include <stdarg.h>

int
scanw(const char *fmt, ...)
{
	int code;
	va_list ap;

#ifdef M_CURSES_TRACE
	__m_trace("scanw(%p = \"%s\", ...)", fmt, fmt);
#endif

	va_start(ap, fmt);
	code = vw_scanw(stdscr, fmt, ap);
	va_end(ap);

	return __m_return_code("scanw", code);
}

int
mvscanw(int y, int x, const char *fmt, ...)
{
	int code;
	va_list ap;

#ifdef M_CURSES_TRACE
	__m_trace("mvscanw(%d, %d, %p = \"%s\", ...)", y, x, fmt, fmt);
#endif

	va_start(ap, fmt);
	if ((code = wmove(stdscr, y, x)) == OK)
		code = vw_scanw(stdscr, fmt, ap);
	va_end(ap);

	return __m_return_code("mvscanw", code);
}

int
mvwscanw(WINDOW *w, int y, int x, const char *fmt, ...)
{
	int code;
	va_list ap;

#ifdef M_CURSES_TRACE
	__m_trace("mvwscanw(%p, %d, %d, %p = \"%s\", ...)", w, y, x, fmt, fmt);
#endif

	va_start(ap, fmt);
	if ((code = wmove(w, y, x)) == OK)
		code = vw_scanw(w, fmt, ap);
	va_end(ap);

	return __m_return_code("mvwscanw", code);
}

int
wscanw(WINDOW *w, const char *fmt, ...)
{
	int code;
	va_list ap;

#ifdef M_CURSES_TRACE
	__m_trace("wscanw(%p, %p = \"%s\", ...)", w, fmt, fmt);
#endif

	va_start(ap, fmt);
	code = vw_scanw(w, fmt, ap);
	va_end(ap);

	return __m_return_code("wscanw", code);
}

