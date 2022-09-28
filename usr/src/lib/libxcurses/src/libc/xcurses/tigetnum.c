/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)tigetnum.c 1.1	95/12/22 SMI"

/*
 * tigetnum.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/tigetnum.c 1.3 1995/09/29 16:04:33 ant Exp $";
#endif
#endif

#include <private.h>
#include <string.h>

int
tigetnum(cap)
const char *cap;
{
	char **p;
	int value = -2;

#ifdef M_CURSES_TRACE
	__m_trace("tigetnum(%p = \"%s\")", cap, cap);
#endif

	for (p = __m_numnames; *p != (char *) 0; ++p) {
		if (strcmp(*p, cap) == 0) {
			value = cur_term->_num[(int)(p - __m_numnames)];
			break;
		}
	}

	return __m_return_int("tigetnum", value);
}
