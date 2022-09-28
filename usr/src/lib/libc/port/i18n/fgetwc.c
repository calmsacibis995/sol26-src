/*
 * Copyright (c) 1995, 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)fgetwc.c 1.9	96/07/02  SMI"

#include <stdio.h>
#include <sys/localedef.h>
#include <widec.h>
#include <euc.h>
#include <limits.h>
#include <errno.h>
#include "mtlib.h"

#define	IS_C1(c) (((c) >= 0x80) && ((c) <= 0x9f))

#ifdef __STDC__
#pragma weak fgetwc = _fgetwc
#pragma weak getwc = _getwc
#endif

#if defined(PIC)
wint_t
__fgetwc_euc(_LC_charmap_t * hdl, FILE *iop)
#else	/* !PIC */
#ifdef _REENTRANT
wint_t
_fgetwc_unlocked(FILE *iop)
#else	/* !_REENTRANT */
wint_t
_fgetwc(FILE *iop)
#endif	/* _REENTRANT */
#endif	/* PIC */
{
	int	c, length;
	wint_t	intcode, mask;

	if ((c = GETC(iop)) == EOF)
		return (WEOF);

	if (isascii(c))		/* ASCII code */
		return ((wint_t)c);


	intcode = 0;
	mask = 0;
	if (c == SS2) {
#if defined(PIC)
		if ((length = hdl->cm_eucinfo->euc_bytelen2) == 0)
#else
		if ((length = eucw2) == 0)
#endif
			goto lab1;
		mask = WCHAR_CS2;
		goto lab2;
	} else if (c == SS3) {
#if defined(PIC)
		if ((length = hdl->cm_eucinfo->euc_bytelen3) == 0)
#else
		if ((length = eucw3) == 0)
#endif
			goto lab1;
		mask = WCHAR_CS3;
		goto lab2;
	}

lab1:
	if (IS_C1(c))
		return ((wint_t)c);
#if defined(PIC)
	length = hdl->cm_eucinfo->euc_bytelen1 - 1;
#else
	length = eucw1 - 1;
#endif
	mask = WCHAR_CS1;
	intcode = c & WCHAR_S_MASK;
lab2:
	if (length < 0)		/* codeset 1 is not defined? */
		return ((wint_t)c);
	while (length--) {
		c = GETC(iop);
		if (c == EOF || isascii(c) ||
		    (IS_C1(c))) {
			UNGETC(c, iop);
			errno = EILSEQ;
			return (WEOF); /* Illegal EUC sequence. */
		}
		intcode = (intcode << WCHAR_SHIFT) | (c & WCHAR_S_MASK);
	}
	return ((wint_t)(intcode|mask));
}

#if defined(PIC)
#ifdef _REENTRANT
wint_t
_fgetwc_unlocked(FILE *iop)
#else	/* !_REENTRANT */
wint_t
_fgetwc(FILE *iop)
#endif	/* _REENTRANT */
{
	return (METHOD(__lc_charmap, fgetwc)(__lc_charmap, iop));
}
#endif	/* PIC */

#ifdef _REENTRANT
wint_t
_fgetwc(FILE *iop)
{
	wint_t result;

	flockfile(iop);
	result = _fgetwc_unlocked(iop);
	funlockfile(iop);
	return (result);
}
#endif /* _REENTRANT */

wint_t
_getwc(FILE *iop)
{
	return (fgetwc(iop));
}
