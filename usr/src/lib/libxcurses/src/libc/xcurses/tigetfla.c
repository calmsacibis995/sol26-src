/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)tigetfla.c 1.1	95/12/22 SMI"

/*
 * tigetfla.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/tigetfla.c 1.2 1995/07/14 20:48:45 ant Exp $";
#endif
#endif

#include <private.h>
#include <string.h>

int
tigetflag(cap)
const char *cap;
{
	char **p;
	int value = -1;

#ifdef M_CURSES_TRACE
	__m_trace("tigetflag(%p = \"%s\")", cap, cap);
#endif

	for (p = __m_boolnames; *p != (char *) 0; ++p) {
		if (strcmp(*p, cap) == 0) {
			value = cur_term->_bool[(int)(p - __m_boolnames)];
			break;
		}
	}

	return __m_return_int("tigetflag", value);
}
