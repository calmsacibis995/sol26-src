/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__wctomb_dense_pck.c	1.6	96/07/30  SMI"

/*
static char sccsid[] = "@(#)55	1.3.1.1  "
"src/bos/usr/ccs/lib/libc/__wctomb_dense_pck.c, bos, bos410 5/25/92 13:44:21";
*/
/*
 * COMPONENT_NAME: (LIBCCCPPC) LIBC Code-Point/Process-Code Conversion Functions
 *
 * FUNCTIONS: __wctomb_dense_pck
 *
 * ORIGINS: 27
 *
 * This module contains IBM CONFIDENTIAL code. -- (IBM
 * Confidential Restricted when combined with the aggregated
 * modules for this product)
 * OBJECT CODE ONLY SOURCE MATERIALS
 * (C) COPYRIGHT International Business Machines Corp. 1991 , 1992
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
 *
 *  The algorithm for this conversion is:
 *  PC <=  0x007f:                  s[0] = PC
 *  PC >= 0x00a1 and PC <= 0x00df:  s[0] = PC
 *  PC >= 0x0100 and PC <= 0x189e:  s[0] = ((PC - 0x0100) & 0x001f) + 0x81
 *                                  s[1] = ((PC - 0x0100) >> 5) + 0x40
 *  PC >= 0x189f and PC <= 0x303b:  s[0] = ((PC - 0x189f) & 0x001f) + 0xe0
 *                                  s[1] = ((PC - 0x189f >> 5) + 0x40
 *
 *  |  process code   |   s[0]    |   s[1]    |
 *  +-----------------+-----------+-----------+
 *  | 0x0000 - 0x007f | 0x00-0x7f |    --     |
 *  | 0x007e - 0x00a0 |    --     |    --     |
 *  | 0x00a1 - 0x00df | 0xa1-0xdf |    --     |
 *  | 0x00e0 - 0x00ff |    --     |    --     |
 *  | 0x0100 - 0x189e | 0x81-0x9f | 0x40-0xfc (excluding 0x7f)
 *  | 0x189f - 0x303b | 0xe0-0xfc | 0xa1-0xfe (excluding 0x7f)
 *  +-----------------+-----------+-----------+
 *
 */

int
__wctomb_dense_pck(_LC_charmap_t *handle, char *s, wchar_t pwc)
{
	/*
	 * if s is NULL or points to a NULL return 0
	 */
	if (s == (char *)NULL)
		return (0);

	/*
	 * Single Byte PC <= 7f or PC >= a1 and PC <= df
	 */

	if (pwc <= 0x7f || (pwc >= 0xa1 && pwc <= 0xdf)) {
		s[0] = (char) pwc;
		return (1);
	}

	/*
	 * Double Byte PC >= e0 and PC <= 189e
	 */
	else if ((pwc >= 0x0100) && (pwc <= 0x189e)) {
		s[0] = (char) (((pwc - 0x0100) & 0x001f) + 0x0081);
		s[1] = (char) (((pwc - 0x0100) >> 5) + 0x0040);
		return (2);
	}

	/*
	 * Double Byte PC >=189f and PC <=303b
	 */
	else if ((pwc >= 0x189f) && (pwc <= 0x303b)) {
		s[0] = (char) (((pwc - 0x0189f) & 0x001f) + 0x00e0);
		s[1] = (char) (((pwc - 0x0189f) >> 5) + 0x0040);
		return (2);
	}

	errno = EILSEQ;
	return (-1);
}
