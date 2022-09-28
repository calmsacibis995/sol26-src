/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)tgetflag.c 1.1	95/12/22 SMI"

/*
 * tgetflag.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/tgetflag.c 1.2 1995/08/30 19:27:38 danv Exp $";
#endif
#endif

#include <private.h>
#include <string.h>

int
tgetflag(cap)
const char *cap;
{
	char **p;
	int value = -1;

#ifdef M_CURSES_TRACE
	__m_trace("tgetflag(%p = \"%.2s\")", cap, cap);
#endif

	for (p = __m_boolcodes; *p != (char *) 0; ++p) {
		if (strcmp(*p, cap) == 0) {
			value = cur_term->_bool[(int)(p - __m_boolcodes)];
			break;
		}
	}

	return __m_return_int("tgetflag", value);
}
