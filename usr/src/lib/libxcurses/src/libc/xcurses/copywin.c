/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)copywin.c 1.1	95/12/22 SMI"

/*
 * copywin.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/copywin.c 1.2 1995/09/19 19:15:33 ant Exp $";
#endif
#endif

#include <private.h>
#include <wctype.h>

#undef min
#define min(a,b)		((a) < (b) ? (a) : (b))

/*f
 * Version of copywin used internally by Curses to compute
 * the intersection of the two windows before calling copywin().
 */
int
__m_copywin(s, t, transparent)
const WINDOW *s;
WINDOW *t;
int transparent;
{
	int code, sminr, sminc, tminr, tminc, tmaxr, tmaxc;

#ifdef M_CURSES_TRACE
	__m_trace("__m_copywin(%p, %p, %d)", s, t, transparent);
#endif

	tmaxc = min(s->_begx + s->_maxx, t->_begx + t->_maxx) - 1 - t->_begx;
	tmaxr = min(s->_begy + s->_maxy, t->_begy + t->_maxy) - 1 - t->_begy;

	if (s->_begy < t->_begy) {
		sminr = t->_begy - s->_begy;
		tminr = 0;
	} else {
		sminr = 0;
		tminr = s->_begy - t->_begy;
	}
	if (s->_begx < t->_begx) {
		sminc = t->_begx - s->_begx;
		tminc = 0;
	} else {
		sminc = 0; 
		tminc = s->_begx- t->_begx;
	}
	code = copywin(
		s, t, sminr, sminc, tminr, tminc, tmaxr, tmaxc, transparent
	);

	return __m_return_code("__m_copywin", code);
}

/*f
 * Overlay specified part of source window over destination window
 * NOTE copying is destructive only if transparent is set to false.
 */
int
copywin(s, t, sminr, sminc, tminr, tminc, tmaxr, tmaxc, transparent)
const WINDOW *s;
WINDOW *t;
int sminr, sminc, tminr, tminc, tmaxr, tmaxc, transparent;
{
	int i, tc;
	cchar_t *st, *tt;

#ifdef M_CURSES_TRACE
	__m_trace(
		"copywin(%p, %p, %d, %d, %d, %d, %d, %d, %d)",
		s, t, sminr, sminc, tminr, tminc, tmaxr, tmaxc, transparent
	);
#endif

	for (; tminr <= tmaxr; ++tminr, ++sminr) {
		st = s->_line[sminr] + sminc;
		tt = t->_line[tminr] + tminc;

		/* Check target window for overlap of broad
		 * characters around the outer edge of the
		 * source window's location.
		 */
		__m_cc_erase(t, tminr, tminc, tminr, tminc);
		__m_cc_erase(t, tminr, tmaxc, tminr, tmaxc);

		/* Copy source region to target. */
		for (tc = tminc; tc <= tmaxc; ++tc, ++tt, ++st) {
			if (transparent) 
				if (iswspace(st->_wc[0]))
					continue;
			*tt = *st;
		}

#ifdef M_CURSES_SENSIBLE_WINDOWS
		/* Case 4 - 
		 * Expand incomplete glyph from source into target window.
		 */
		if (0 < tminc && !t->_line[tminr][tminc]._f)
			(void) __m_cc_expand(t, tminr, tminc, -1);
		if (tmaxc + 1 < t->_maxx && !__m_cc_islast(t, tminr, tmaxc))
			(void) __m_cc_expand(t, tminr, tmaxc, 1);
#endif /* M_CURSES_SENSIBLE_WINDOWS */
	}

	return __m_return_code("copywin", OK);
}
