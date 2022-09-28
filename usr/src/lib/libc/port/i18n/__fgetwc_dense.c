/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)__fgetwc_dense.c	1.6	96/07/02 SMI"

/*
 * Macro for testing a valid byte value for a multi-byte/single-byte
 * character. A 96-character character set is assumed.
 */
#define	MB_MIN_BYTE	0xa0
#define	MB_MAX_BYTE	0xff
#define	IS_VALID(c)	(((c) >= MB_MIN_BYTE) && ((c) <= MB_MAX_BYTE))
#define	IS_C1(c)	(((c) >= 0x80) && ((c) <= 0x9f))

#define	STRIP(c)	((c) - MB_MIN_BYTE)
#define	COMPOSE(c1, c2)	(((c1) << WCHAR_SHIFT) | (c2))
#define	ERR_RETURN	errno = EILSEQ; return (WEOF)

#include <stdio.h>
#include <sys/localedef.h>
#include <widec.h>
#include <euc.h>
#include <limits.h>
#include <errno.h>
#include "mtlib.h"

wint_t
__fgetwc_dense(_LC_charmap_t *hdl, FILE *iop)
{
	int		c;
	int		length;
	_LC_euc_info_t	*eucinfo;

	if ((c = GETC(iop)) == EOF)
		return (WEOF);

	/*
	 * Optimization for a single-byte codeset.
	 *
	 * if ASCII or MB_CUR_MAX == 1 (i.e., single-byte codeset),
	 * store it to *wchar.
	 */
	if (isascii(c) || (hdl->cm_mb_cur_max == 1))
		return ((wint_t) c);

	eucinfo = hdl->cm_eucinfo;

	if (c == SS2) {
		if (eucinfo->euc_bytelen2 == 1) {
			c = GETC(iop);
			if (! IS_VALID(c)) {
				UNGETC(c, iop);
				ERR_RETURN;
			}
			return ((wint_t) (STRIP(c) + eucinfo->cs2_base));
		} else if (eucinfo->euc_bytelen2 == 2) {
			int c2;

			c = GETC(iop);
			if (! IS_VALID(c)) {
				UNGETC(c, iop);
				ERR_RETURN;
			}
			c2 = GETC(iop);
			if (! IS_VALID(c2)) {
				UNGETC(c2, iop);
				ERR_RETURN;
			}
			return ((wint_t) (COMPOSE(STRIP(c), STRIP(c2)) +
				eucinfo->cs2_base));
		} else if (eucinfo->euc_bytelen2 == 3) {
			int	c2, c3;

			c = GETC(iop);
			if (! IS_VALID(c)) {
				UNGETC(c, iop);
				ERR_RETURN;
			}
			c2 = GETC(iop);
			if (! IS_VALID(c2)) {
				UNGETC(c2, iop);
				ERR_RETURN;
			}
			c = COMPOSE(STRIP(c), STRIP(c2));
			c3 = GETC(iop);
			if (! IS_VALID(c3)) {
				UNGETC(c3, iop);
				ERR_RETURN;
			}
			return ((wint_t) (COMPOSE(c, STRIP(c3))
				+ eucinfo->cs2_base));
		} else {
			int	c2;

			if ((length = eucinfo->euc_bytelen2) == 0)
				return ((wint_t) c);
			c = 0;
			while (length--) {
				c2 = GETC(iop);
				if (! IS_VALID(c2)) {
					UNGETC(c2, iop);
					ERR_RETURN;
				}
				c = COMPOSE(c, STRIP(c2));
			}
			return ((wchar_t) (c + eucinfo->cs2_base));
		}
	} else if (c == SS3) {
		if (eucinfo->euc_bytelen3 == 1) {
			c = GETC(iop);
			if (! IS_VALID(c)) {
				UNGETC(c, iop);
				ERR_RETURN;
			}
			return ((wint_t) (STRIP(c) + eucinfo->cs3_base));
		} else if (eucinfo->euc_bytelen3 == 2) {
			int c2;

			c = GETC(iop);
			if (! IS_VALID(c)) {
				UNGETC(c, iop);
				ERR_RETURN;
			}
			c2 = GETC(iop);
			if (! IS_VALID(c2)) {
				UNGETC(c2, iop);
				ERR_RETURN;
			}
			return ((wchar_t) (COMPOSE(STRIP(c), STRIP(c2))
				+ eucinfo->cs3_base));
		} else {
			int length, c2;

			if ((length = eucinfo->euc_bytelen3) == 0)
				return ((wchar_t) c);
			c = 0;
			while (length--) {
				c2 = GETC(iop);
				if (! IS_VALID(c2)) {
					UNGETC(c2, iop);
					ERR_RETURN;
				}
				c = COMPOSE(c, STRIP(c2));
			}
			return ((wchar_t) (c + eucinfo->cs3_base));
		}
	}

	/*
	 * This locale sensitive checking should not be
	 * performed. However, this code remains unchanged for
	 * the compatibility.
	 */
	if (IS_C1(c))
		return ((wint_t) c);

	if (eucinfo->euc_bytelen1 == 2) {
		int c2;

		c2 = GETC(iop);
		if (! IS_VALID(c2)) {
			UNGETC(c2, iop);
			ERR_RETURN;
		}
		return ((wint_t) (COMPOSE(STRIP(c), STRIP(c2))
			+ eucinfo->cs1_base));
	} else {
		int length, c2;

		length = eucinfo->euc_bytelen1;
		c = 0;
		while (length--) {
			c2 = GETC(iop);
			if (! IS_VALID(c2)) {
				UNGETC(c2, iop);
				ERR_RETURN;
			}
			c = COMPOSE(c, STRIP(c2));
		}
		return ((wint_t) (c + eucinfo->cs1_base));
	}
	/*NOTREACHED*/
}
