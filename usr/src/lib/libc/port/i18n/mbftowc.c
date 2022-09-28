/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)mbftowc.c 1.8	96/08/07  SMI"

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)mbftowc.c	1.8	96/08/07 SMI"

#include <ctype.h>
#include <stdlib.h>
#include <sys/localedef.h>
#include <widec.h>

#define	IS_C1(c) (((c) >= 0x80) && ((c) <= 0x9f))

/* returns number of bytes read by (*f)() */
int
__mbftowc_euc(_LC_charmap_t *hdl, char *s, wchar_t *wchar,
		int (*f)(), int *peekc)
{
	register int length;
	register wchar_t intcode;
	register c;
	char *olds = s;
	wchar_t mask;

	if ((c = (*f)()) < 0)
		return (0);
	*s++ = c;
	if (isascii(c)) {
		*wchar = c;
		return (1);
	}
	intcode = 0;
	if (c == SS2) {
		if (!(length = hdl->cm_eucinfo->euc_bytelen2))
			goto lab1;
		mask = WCHAR_CS2;
		goto lab2;
	} else if (c == SS3) {
		if (!(length = hdl->cm_eucinfo->euc_bytelen3))
			goto lab1;
		mask = WCHAR_CS3;
		goto lab2;
	}

lab1:
	/* checking C1 characters */
	if IS_C1(c) {
		*wchar = c;
		return (1);
	}
	mask = WCHAR_CS1;
	length = hdl->cm_eucinfo->euc_bytelen1 - 1;
	intcode = c & WCHAR_S_MASK;
lab2:
	if (length < 0)
		return (-1);

	while (length--) {
		*s++ = c = (*f)();
		if (isascii(c) || IS_C1(c)) { /* Illegal EUC sequence. */
			if (c >= 0)
				*peekc = c;
			--s;
			return (-(s - olds));
		}
		intcode = (intcode << WCHAR_SHIFT) | (c & WCHAR_S_MASK);
	}
	*wchar = intcode | mask;
	return (s - olds);
}

int
_mbftowc(char *s, wchar_t *wchar, int (*f)(), int *peekc)
{
	return (METHOD(__lc_charmap, mbftowc)
			(__lc_charmap, s, wchar, f, peekc));
}
