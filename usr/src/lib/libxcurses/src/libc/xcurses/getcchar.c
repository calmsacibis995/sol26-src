/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)getcchar.c 1.1	95/12/22 SMI"

/*
 * getcchar.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/getcchar.c 1.1 1995/05/10 13:59:24 ant Exp $";
#endif
#endif

#include <private.h>

int
getcchar(const cchar_t *c, wchar_t *wcs, attr_t *at, short *co, void *opts)
{
	int i;

#ifdef M_CURSES_TRACE
	__m_trace("getcchar(%p, %p, %p, %p, %p)", c, wcs, at, co, opts);
#endif

	if (wcs == (wchar_t *) 0)
		return __m_return_int("getcchar", c->_n + 1);

	*at = c->_at;
	*co = (short) c->_co;
	
	for (i = 0; i < c->_n; ++i)
		*wcs++ = c->_wc[i];
	*wcs = M_MB_L('\0');
		
	return __m_return_code("getcchar", OK);
}
