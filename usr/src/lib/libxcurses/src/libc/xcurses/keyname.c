/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)keyname.c 1.1	95/12/22 SMI"

/*
 * keyname.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/keyname.c 1.5 1995/10/02 19:40:13 ant Exp $";
#endif
#endif

#include <private.h>
#include <ctype.h>
#include <string.h>

/*f
 *
 */
const char *
keyname(ch)
int ch;
{
	short (*p)[2];
	const char *str;

#ifdef M_CURSES_TRACE
	__m_trace("keyname(%d)", ch);
#endif

	/* Lookup KEY_ code. */
	for (p = __m_keyindex; **p != -1; ++p) {
		if ((*p)[1] == ch) {
			str = __m_strfnames[**p];
			goto done;
		}
	}

	/* unctrl() handles printable, control, and meta keys. */
	str = unctrl(ch);
done:
#ifdef M_CURSES_TRACE
	__m_trace("keyname returned %p = \"%s\".", str, str);
#endif
	return str;
}
