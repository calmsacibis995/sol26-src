/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__wcwidth_dense_euctw.c 1.6	96/07/30  SMI"

/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 *
 * Origianlly came from:
 *	static char sccsid[] = "@(#)52	1.1  "
"src/bos/usr/lib/nls/loc/methods/zh_TW/__wcwidth_euctw.c,"
" bos, bos410 5/25/92 16:04:54";
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

#include <stdlib.h>
#include <ctype.h>
#include <sys/localedef.h>

/*
 * Get the width of Wide Characters
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
__wcwidth_dense_euctw(_LC_charmap_t *handle, wchar_t wc)
{
	int	len;

	/*
	 * if wc is NULL, return 0
	 */
	if (wc == (wchar_t)NULL)
		return (0);

	if (! iswprint(wc))
		return (-1);

	/* Single byte: <= 0x7f */
	if (wc <= 0x7f)
		return (1);
	/* Double bytes: 0x100 ~ 0x2383 */
	else if (wc >= 0x100 && wc <= 0x2293f)
	/* Four bytes:   0x2384 ~ 0x2293f */
		return (2);

	return (-1);

}
