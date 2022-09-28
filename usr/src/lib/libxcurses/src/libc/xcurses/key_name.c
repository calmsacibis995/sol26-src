/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)key_name.c 1.2	96/06/21 SMI"

/*
 * key_name.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/key_name.c 1.1 1995/06/07 13:57:49 ant Exp $";
#endif
#endif

#include <private.h>
#include <string.h>

/*f
 *
 */
const char *
key_name(wchar_t wc)
{
	size_t len;
	cchar_t cc;
	wchar_t *ws;
	static char mbs[MB_LEN_MAX+1];

#ifdef M_CURSES_TRACE
	__m_trace("key_name(%ld)", wc);
#endif

	(void) __m_wc_cc(wc, &cc);

	ws = (wchar_t *) wunctrl(&cc);

	if ((len = wcstombs(mbs, ws, MB_LEN_MAX)) == (size_t) -1)
		return __m_return_pointer("key_name", (const char *) 0);

	mbs[len] = '\0';

#ifdef M_CURSES_TRACE
	__m_trace("key_name returned %p = \"%s\".", mbs, mbs);
#endif
	return mbs;
}
