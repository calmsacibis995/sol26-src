/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)wbrdr.c 1.1	95/12/22 SMI"

/*
 * wbrdr.c		
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/wbrdr.c 1.2 1995/07/07 18:53:03 ant Exp $";
#endif
#endif

#include <private.h>

/*f
 * Draw a border around the edges of the window. The parms correspond to
 * a character and attribute for the left, right, top, and bottom sides, 
 * top left, top right, bottom left, and bottom right corners. A zero in
 * any character parm means to take the default.
 */
int
wborder(WINDOW *w,
	chtype ls, chtype rs, chtype ts, chtype bs, 
	chtype tl, chtype tr, chtype bl, chtype br)
{
	int code;
	cchar_t wls, wrs, wts, wbs, wtl, wtr, wbl, wbr;

#ifdef M_CURSES_TRACE
	__m_trace(
		"wborder(%p, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld)",
		 w, ls, rs, ts, bs, tl, tr, bl, br
	);
#endif

	if (ls == 0)
		ls = ACS_VLINE;
	(void) __m_acs_cc(ls, &wls);

	if (rs == 0)
		rs = ACS_VLINE;
	(void) __m_acs_cc(rs, &wrs);

	if (ts == 0)
		ts = ACS_HLINE;
	(void) __m_acs_cc(ts, &wts);

	if (bs == 0)
		bs = ACS_HLINE;
	(void) __m_acs_cc(bs, &wbs);

	if (tl == 0)
		tl = ACS_ULCORNER;
	(void) __m_acs_cc(tl, &wtl);

	if (tr == 0)
		tr = ACS_URCORNER;
	(void) __m_acs_cc(tr, &wtr);

	if (bl == 0)
		bl = ACS_LLCORNER;
	(void) __m_acs_cc(bl, &wbl);

	if (br == 0)
		br = ACS_LRCORNER;
	(void) __m_acs_cc(br, &wbr);

	code = wborder_set(w, &wls, &wrs, &wts, &wbs, &wtl, &wtr, &wbl, &wbr);

	return __m_return_code("wborder", code);
}

