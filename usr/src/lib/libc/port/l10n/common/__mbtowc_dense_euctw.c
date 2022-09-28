/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__mbtowc_dense_euctw.c 1.5	96/07/02  SMI"

/*
 * static char sccsid[] = "@(#)46	1.1  src/bos/usr/lib/nls/loc/methods/"
 * "zh_TW/__mbtowc_dense_euctw.c, bos, bos410 5/25/92 16:03:47";
 */

/*
 * COMPONENT_NAME: (LIBMETH) LIBC Code-Point/Process-Code Conversion Functions
 *
 * FUNCTIONS: mbtowc_dense_euctw
 *
 * ORIGINS: 27
 *
 * This module contains IBM CONFIDENTIAL code. -- (IBM
 * Confidential Restricted when combined with the aggregated
 * modules for this product)
 * OBJECT CODE ONLY SOURCE MATERIALS
 * (C) COPYRIGHT International Business Machines Corp.  1992
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 */

#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/localedef.h>


/*
 *  Converts a multi-byte string to process code for the TW_EUC codeset
 *
 *  The algorithm for this conversion is:
 *  s[0] <= 0x7f:  PC = s[0]
 *  s[0] >= 0xa1:  PC = (((s[0] - 0xa1) * 94) | (s[1] - 0xa1) + 0x100);
 *  s[0] = 0x8e  s[1] = a2: PC = (((s[2] - 0xa1) * 94) + (s[3] - 0xa1) + 0x2384)
 *  s[0] = 0x8e  s[1] = a3: PC = (((s[2] - 0xa1) * 94) + (s[3] - 0xa1) + 0x4608)
 *  s[0] = 0x8e  s[1] = ac: PC = (((s[2] - 0xa1) * 94) + (s[3] - 0xa1) + 0x688c)
 *  s[0] = 0x8e  s[1] = ad: PC = (((s[2] - 0xa1) * 94) + (s[3] - 0xa1) + 0x8b10)
 *  s[0] = 0x8e  s[1] = ae: PC = (((s[2] - 0xa1) * 94) + (s[3] - 0xa1) + 0xad94)
 *  s[0] = 0x8e  s[1] = af: PC = (((s[2] - 0xa1) * 94) + (s[3] - 0xa1) + 0xd018)
 *
 *  |  process code   |   s[0]    |   s[1]    |   s[2]    |    s[3]   |
 *  +-----------------+-----------+-----------+-----------+-----------+
 *  | 0x0000 - 0x007f | 0x00-0x7f |    --     |    --     |     --    |
 *  | 0x0080 - 0x00ff |   --      |    --     |    --     |     --    |
 *  | 0x0100 - 0x2383 | 0xa1-0xfe | 0xa1-0xfe |    --     |     --    |
 *  | 0x2384 - 0x4607 | 0x8e      | 0xa2      | 0xa1-0xfe | 0xa1-0xfe |
 *  | 0x4608 - 0x688b | 0x8e      | 0xa3      | 0xa1-0xfe | 0xa1-0xfe |
 *  | 0x688c - 0x8b0f | 0x8e      | 0xac      | 0xa1-0xfe | 0xa1-0xfe |
 *  | 0x8b10 - 0xad93 | 0x8e      | 0xad      | 0xa1-0xfe | 0xa1-0xfe |
 *  | 0xad94 - 0xd017 | 0x8e      | 0xae      | 0xa1-0xfe | 0xa1-0xfe |
 *  | 0xd018 - 0xf29b | 0x8e      | 0xaf      | 0xa1-0xfe | 0xa1-0xfe |
 *  +-----------------+-----------+-----------+-----------+-----------+
 *
 *  This algorithm compresses all of code points to process codes less
 *  than 0xf29b.
*/

int
__mbtowc_dense_euctw(_LC_charmap_t *hdl, wchar_t *pwc, const char *s,
			size_t maxlen)
{
	wchar_t dummy;

	if (maxlen < 1)
	return ((size_t)-1);

	if (s == (char *)NULL)
	return (0);

	if (pwc == (wchar_t *)NULL)
	pwc = &dummy;

	if ((unsigned char)s[0] <= 0x7f) {
		*pwc = (wchar_t) s[0];
		if (s[0] != '\0')
			return (1);
		else
			return (0);
	} else if ((unsigned char)s[0] == 0x8e) {
		if (maxlen >= 4) {
		*pwc = (wchar_t)(((unsigned char)s[1] - 0xa1) * 94 * 94)
		+ (wchar_t)(((unsigned char)s[2] - 0xa1) * 94)
		+ (wchar_t) ((unsigned char)s[3] - 0xa1) + 0x100;
		return (4);
		}
	} else if ((unsigned char)s[0] >= 0xa1 &&
			(unsigned char)s[0] <= 0xfe) {
		if (maxlen >= 2) {
		    if ((unsigned char)s[1] >= 0xa1 &&
			(unsigned char)s[1] <= 0xfe) {
		    *pwc = (wchar_t)(((unsigned char)s[0] - 0xa1) * 94)
		    + (wchar_t) (((unsigned char)s[1] - 0xa1)) + 0x100;
		    return (2);
		    }
		}
	}

	errno = EILSEQ;
	return (-1);

}
