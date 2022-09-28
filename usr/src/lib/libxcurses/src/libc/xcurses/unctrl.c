/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)unctrl.c 1.2	96/05/30 SMI"

/*
 * unctrl.c
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
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/unctrl.c 1.2 1995/10/02 19:34:15 ant Exp $";
#endif
#endif

#include <private.h>
#include <limits.h>
#include <ctype.h>

static const char *carat[] = {
	"^?",
	"^@",
	"^A",
	"^B",
	"^C",
	"^D",
	"^E",
	"^F",
	"^G",
	"^H",
	"^I",
	"^J",
	"^K",
	"^L",
	"^M",
	"^N",
	"^O",
	"^P",
	"^Q",
	"^R",
	"^S",
	"^T",
	"^U",
	"^V",
	"^W",
	"^X",
	"^Y",
	"^Z",
	"^[",
	"^\\", 
	"^]",
	"^^",
	"^_"
};

const char *
unctrl(chtype ch)
{
	char *str;
	int c, msb;
	static char chr[5];

#ifdef M_CURSES_TRACE
	__m_trace("unctrl(%ld)", ch);
#endif

        /* Map wide character to a wide string. */
	c = ch & A_CHARTEXT;
	msb = 1 << (CHAR_BIT-1);

	if (iscntrl(c)) {
		/* ASCII DEL */
		if (c == 127)
			return __m_return_pointer("unctrl", carat[0]);

		/* ASCII control codes. */
		if (0 <= c && c < 32)
			return __m_return_pointer("unctrl", carat[c+1]);

		/* Something we don't know what to do with. */
		return __m_return_pointer("unctrl", (char *) 0);
	} else if (c & msb) {
		/* Meta key notation if high bit is set on character. */
		c &= ~msb;

		chr[0] = 'M';
		chr[1] = '-';

		if (iscntrl(c)) {
			str = (char *) unctrl(c);
			chr[2] = *str++;
			chr[3] = *str;
			chr[4] = '\0';
		} else {
			chr[2] = c;
			chr[3] = '\0';
		}
	} else { 
		/* Return byte as is. */
		chr[0] = c;
		chr[1] = '\0';
	}

	return __m_return_pointer("unctrl", chr);
}
