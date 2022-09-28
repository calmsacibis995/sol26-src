/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__fgetwc_dense_pck.c 1.3	96/07/10  SMI"

#include <stdio.h>
#include <sys/localedef.h>
#include <errno.h>
#include "mtlib.h"

wint_t
__fgetwc_dense_pck(_LC_charmap_t *hdl, FILE *iop)
{
	int	c;
	wint_t  wchar;
	int	ch;

	if ((c = GETC(iop)) == EOF)
		return (WEOF);

	if (c <= 0x7f) {
		wchar = c;
		return (wchar);
	} else if ((unsigned char)c >= 0x81 && (unsigned char)c <= 0x9f) {
		ch = GETC(iop);
		if ((unsigned char)ch >= 0x40 && (unsigned char)ch <= 0xfc &&
			(unsigned char)ch != 0x7f) {
			wchar = ((wchar_t)(((unsigned char)ch - 0x40) << 5)
				| (wchar_t)((unsigned char)c - 0x81)) + 0x100;
			return (wchar);
		} else {
			UNGETC(ch, iop);
			errno = EILSEQ;
			return (WEOF);
		}
	} else if ((unsigned char)c >= 0xe0 && (unsigned char)c <= 0xfc) {
		ch = GETC(iop);
		if ((unsigned char)ch >= 0x40 && (unsigned char)ch <= 0xfc &&
			(unsigned char)ch != 0x7f) {
			wchar = ((wchar_t)(((unsigned char)ch - 0x40) << 5)
				| (wchar_t)((unsigned char)c - 0xe0)) + 0x189f;
			return (wchar);
		} else {
			UNGETC(ch, iop);
			errno = EILSEQ;
			return (WEOF);
		}
	} else if ((unsigned char)c >= 0xa1 && (unsigned char)c <= 0xdf) {
			wchar = c;
			return (wchar);
		}

	/* must be invalid char */
	errno = EILSEQ;
	return (WEOF);
}
