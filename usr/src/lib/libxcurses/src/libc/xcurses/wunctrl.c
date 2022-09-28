/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)wunctrl.c 1.1	95/12/22 SMI"

/*
 * wunctrl.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/wunctrl.c 1.1 1995/05/16 15:15:37 ant Exp $";
#endif
#endif

#include <private.h>
#include <wctype.h>

static const wchar_t *carat[] = {
	M_MB_L("^?"),
	M_MB_L("^@"),
	M_MB_L("^A"),
	M_MB_L("^B"),
	M_MB_L("^C"),
	M_MB_L("^D"),
	M_MB_L("^E"),
	M_MB_L("^F"),
	M_MB_L("^G"),
	M_MB_L("^H"),
	M_MB_L("^I"),
	M_MB_L("^J"),
	M_MB_L("^K"),
	M_MB_L("^L"),
	M_MB_L("^M"),
	M_MB_L("^N"),
	M_MB_L("^O"),
	M_MB_L("^P"),
	M_MB_L("^Q"),
	M_MB_L("^R"),
	M_MB_L("^S"),
	M_MB_L("^T"),
	M_MB_L("^U"),
	M_MB_L("^V"),
	M_MB_L("^W"),
	M_MB_L("^X"),
	M_MB_L("^Y"),
	M_MB_L("^Z"),
	M_MB_L("^["),
	M_MB_L("^\\"), 
	M_MB_L("^]"),
	M_MB_L("^^"),
	M_MB_L("^_")
};

const wchar_t *
wunctrl(cc)
const cchar_t *cc;
{
	int i;
	wint_t wc;
	static wchar_t wcs[M_CCHAR_MAX+1];

#ifdef M_CURSES_TRACE
	__m_trace("wunctrl(%p)", cc);
#endif
	if (cc->_n <= 0)
		return __m_return_pointer("wunctrl", (wchar_t *) 0);

        /* Map wide character to a wide string. */
	wc = cc->_wc[0];
	if (iswcntrl(wc)) {
		if (wc == 127)
			return __m_return_pointer("wunctrl", carat[0]);
		if (0 <= wc && wc <= 32)
			return __m_return_pointer("wunctrl", carat[wc+1]);
		return __m_return_pointer("wunctrl", (wchar_t *) 0);
	}

	for (i = 0; i < cc->_n; ++i)
		wcs[i] = cc->_wc[i];
	wcs[i] = M_MB_L('\0');

	return __m_return_pointer("wunctrl", wcs);
}
