/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)tgetstr.c 1.1	95/12/22 SMI"

/*
 * tgetstr.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/tgetstr.c 1.2 1995/08/30 19:31:37 danv Exp $";
#endif
#endif

#include <private.h>
#include <string.h>

/*
 * Termcap Emulation
 *
 * Similar to tigetstr() except cap is a termcap code and area is
 * the buffer area used to receive a copy of the string.
 */
char *
tgetstr(cap, area)
const char *cap;
char **area;
{
	char **p, *value = (char *) -1;

#ifdef M_CURSES_TRACE
	__m_trace("tgetstr(%p = \"%.2s\", %p)", cap, cap, area);
#endif

	for (p = __m_strcodes; *p != (char *) 0; ++p) {
		if (strcmp(*p, cap) == 0) {
			value = cur_term->_str[(int)(p - __m_strcodes)];
			if (*area != (char *) 0 && *area != (char *) 0)
				*area += strlen(strcpy(*area, value));
			break;
		}
	}

	return __m_return_pointer("tgetstr", value);
}
