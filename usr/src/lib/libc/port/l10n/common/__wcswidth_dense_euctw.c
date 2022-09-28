/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__wcswidth_dense_euctw.c 1.6	96/07/30  SMI"

/*
static char sccsid[] = "@(#)50	1.1  "
"src/bos/usr/lib/nls/loc/methods/zh_TW/__wcswidth_dense_euctw.c,"
" bos, bos410 5/25/92 16:04:34";
*/
/*
 * COMPONENT_NAME: (LIBMETH) LIBC Code-Point/Process-Code Conversion Functions
 *
 * FUNCTIONS: wcwidth_dense_euctw
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
#include <sys/localedef.h>
/*
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
 */

int
__wcswidth_dense_euctw(_LC_charmap_t *handle, const wchar_t *wcs, size_t n)
{
	int	len;
	int	i;

	/*
	 * if wcs is NULL, return 0
	 */
	if (wcs == (wchar_t *)NULL || *wcs == (wchar_t)NULL)
		return (0);

	len = 0;
	for (i = 0; wcs[i] != (wchar_t)NULL && i < n; i++) {

	/*
	 * Single Byte PC <= 7f (1 display widths)
	 */
	if (wcs[i] <= 0x7f)
		len += 1;

	/*
	 * Double byte: 0x0100 - 0x2383 (2 display widths)
	 */
	else if ((wcs[i] >= 0x0100) && (wcs[i] <= 0x2383))
		len += 2;

	/*
	 * Quardruple byte 0x2384 - 0x4607 (2 display widths)
	 */
	else if ((wcs[i] >= 0x2384) && (wcs[i] <= 0x4607))
		len += 2;


	/*
	 * Quardruple byte 0x4608 - 0x688b (2 display widths)
	 */
	else if ((wcs[i] >= 0x4608) && (wcs[i] <= 0x688b))
		len += 2;

	/*
	 * Quardruple byte 0x688c - 0x8b0f (2 display widths)
	 */
	else if ((wcs[i] >= 0x688c) && (wcs[i] <= 0x8b0f))
		len += 2;

	/*
	 * Quardruple byte 0x8b10 - 0xad93 (2 display widths)
	 */
	else if ((wcs[i] >= 0x8b10) && (wcs[i] <= 0xad93))
		len += 2;

	/*
	 * Quardruple byte 0xad94 - 0xd017 (2 display widths)
	 */
	else if ((wcs[i] >= 0xad94) && (wcs[i] <= 0xd017))
		len += 2;

	/*
	 * Quardryple byte 0xd018 - 0xf29b (2 display widths)
	 */
	else if ((wcs[i] >= 0xd018) && (wcs[i] <= 0xf29b))
		len += 2;

	/*
	 * otherwise it is an invaild process code
	 */
	else
		return (-1);
	}

	return (len);

}
