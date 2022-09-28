/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)__mbftowc_dense.c	1.6	96/07/02 SMI"

#include <stdlib.h>
#include <sys/localedef.h>
#include <errno.h>
#include <widec.h>

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
#define	ERR_RETURN	if (c >= 0) \
				*peekc = c; \
			--s; \
			return (-(s - olds))
#define	NEXT_BYTE(c)	*s++ = (c) = (*f)()

int
__mbftowc_dense(_LC_charmap_t * hdl, char *s, wchar_t *wchar,
			int (*f)(), int *peekc)
{
	int		c;
	int		length;
	const char	*olds = s;
	_LC_euc_info_t	*eucinfo;

	NEXT_BYTE(c);
	if (c < 0)
		return (0);

	/*
	 * Optimization for a single-byte codeset.
	 *
	 * if ASCII or MB_CUR_MAX == 1 (i.e., single-byte codeset),
	 * store it to *wchar.
	 */
	if (isascii(c) || (hdl->cm_mb_cur_max == 1)) {
		*wchar = (wchar_t) c;
		return (1);
	}

	eucinfo = hdl->cm_eucinfo;

	if (c == SS2) {
		if (eucinfo->euc_bytelen2 == 1) {
			NEXT_BYTE(c);
			if (! IS_VALID(c)) {
				ERR_RETURN;
			}
			*wchar = (wchar_t) (STRIP(c)
					    + eucinfo->cs2_base);
			return (2);
		} else if (eucinfo->euc_bytelen2 == 2) {
			int c2;

			NEXT_BYTE(c);
			if (! IS_VALID(c)) {
				ERR_RETURN;
			}
			NEXT_BYTE(c2);
			if (! IS_VALID(c2)) {
				ERR_RETURN;
			}
			*wchar = (wchar_t) (COMPOSE(STRIP(c), STRIP(c2))
					    + eucinfo->cs2_base);
			return (3);
		} else if (eucinfo->euc_bytelen2 == 3) {
			int	c2, c3;

			NEXT_BYTE(c);
			if (! IS_VALID(c)) {
				ERR_RETURN;
			}
			NEXT_BYTE(c2);
			if (! IS_VALID(c2)) {
				ERR_RETURN;
			}
			c = COMPOSE(STRIP(c), STRIP(c2));
			NEXT_BYTE(c3);
			if (! IS_VALID(c3)) {
				ERR_RETURN;
			}
			*wchar = (wchar_t) (COMPOSE(c, STRIP(c3))
						    + eucinfo->cs2_base);
			return (4);
		} else {
			int	c2;

			if ((length = eucinfo->euc_bytelen2) == 0) {
				*wchar = (wchar_t) c;
				return (1);
			}
			c = 0;
			while (length--) {
				NEXT_BYTE(c2);
				if (! IS_VALID(c2)) {
					ERR_RETURN;
				}
				c = COMPOSE(c, STRIP(c2));
			}
			*wchar = (wchar_t) (c + eucinfo->cs2_base);
			return (s - olds);
		}
	} else if (c == SS3) {
		if (eucinfo->euc_bytelen3 == 1) {
			NEXT_BYTE(c);
			if (! IS_VALID(c)) {
				ERR_RETURN;
			}
			*wchar = (wchar_t) (STRIP(c)
					    + eucinfo->cs3_base);
			return (2);
		} else if (eucinfo->euc_bytelen3 == 2) {
			int c2;

			NEXT_BYTE(c);
			if (! IS_VALID(c)) {
				ERR_RETURN;
			}
			NEXT_BYTE(c2);
			if (! IS_VALID(c2)) {
				ERR_RETURN;
			}
			*wchar = (wchar_t) (COMPOSE(STRIP(c), STRIP(c2))
					    + eucinfo->cs3_base);
			return (3);
		} else {
			int length, c2;

			if ((length = eucinfo->euc_bytelen3) == 0) {
				*wchar = (wchar_t) c;
				return (1);
			}
			c = 0;
			while (length--) {
				NEXT_BYTE(c2);
				if (! IS_VALID(c2)) {
					ERR_RETURN;
				}
				c = COMPOSE(c, STRIP(c2));
			}
			*wchar = (wchar_t) (c + eucinfo->cs3_base);
			return (s - olds);
		}
	}

	/*
	 * This locale sensitive checking should not be
	 * performed. However, this code remains unchanged for the
	 * compatibility.
	 */
	if (IS_C1(c)) {
		*wchar = c;
		return (1);
	}
	if (eucinfo->euc_bytelen1 == 2) {
		int c2;

		NEXT_BYTE(c2);
		if (! IS_VALID(c2)) {
			ERR_RETURN;
		}
		*wchar = (wchar_t) (COMPOSE(STRIP(c), STRIP(c2))
				    + eucinfo->cs1_base);
		return (2);
	} else {
		int length, c2;

		length = eucinfo->euc_bytelen1;
		c = 0;
		while (length--) {
			c2 = (unsigned char) *s++;
			if (! IS_VALID(c2)) {
				ERR_RETURN;
			}
			c = COMPOSE(c, STRIP(c2));
		}
		*wchar = (wchar_t) (c + eucinfo->cs1_base);
		return (s - olds);
	}
	/*NOTREACHED*/
}
