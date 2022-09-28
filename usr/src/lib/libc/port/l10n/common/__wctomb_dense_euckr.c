/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__wctomb_dense_euckr.c 1.7	96/07/30  SMI"

/*
static char sccsid[] = "@(#)38	1.1  "
"src/bos/usr/lib/nls/loc/methods/ko_KR/__wctomb_dense_euckr.c, "
"bos, bos410 5/25/92 16:00:26";
*/
/*
 * COMPONENT_NAME:	LIBMETH
 *
 * FUNCTIONS: __wctomb_dense_euckr
 *
 * ORIGINS: 27
 *
 * This module contains IBM CONFIDENTIAL code. -- (IBM
 * Confidential Restricted when combined with the aggregated
 * modules for this product)
 * OBJECT CODE ONLY SOURCE MATERIALS
 * (C) COPYRIGHT International Business Machines Corp. 1992
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 */
/*
 *
 * FUNCTION:
 *
 *
 * PARAMETERS:
 *
 *
 * RETURN VALUE:
 *
 *
 */
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/localedef.h>
/*
 * Converts a process code to a string of characters for the KR_EUC codeset
 *
 *   The algorithm for this conversion is:
 *   PC <= 0x007f:                 s[0] = PC
 *   PC >= 0x0100 and PC <=0x2383: s[1] = ((PC - 0x100) % 94) + 0xa1
 *                                 s[0] = (PC - 0x100 -s[1] + 0xa1) / 94 + 0xa1
 *
 *   +-----------------+-----------+-----------+
 *   |  process code   |   s[0]    |   s[1]    |
 *   +-----------------+-----------+-----------+
 *   | 0x0000 - 0x007f | 0x00-0x7f |    --     |
 *   | 0x0080 - 0x00ff |   --      |    --     |
 *   | 0x0100 - 0x2383 | 0xa1-0xfe | 0xa1-0xfe |
 *   +-----------------+-----------+-----------+
 *
 */

int
__wctomb_dense_euckr(_LC_charmap_t *handle, char *s, wchar_t pwc)
{
	/*
	 * if s is NULL or points to a NULL return 0
	 */
	if (s == (char *)NULL)
		return (0);

	/*
	 * Single Byte PC <= 7f
	 */

	if (pwc <= 0x7f) {
		s[0] = (char) pwc;
		return (1);
	}

	/*
	 * Double byte 0x0100 - 0x2383
	 */
	else if ((pwc >= 0x0100) && (pwc <= 0x2383)) {
		pwc -= 0x100;
		s[0] = (char) ((pwc / 94) + 0xa1);
		s[1] = (char) ((pwc % 94) + 0xa1);
		return (2);
	}

	/*
	 * otherwise it is an invaild process code
	 */
	errno = EILSEQ;
	return (-1);
}
