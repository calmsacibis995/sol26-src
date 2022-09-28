/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__wctomb_dense_euctw.c 1.6	96/07/30  SMI"

/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 *
 * Originally came from:
 *	static char sccsid[] = "@(#)51	1.1  "
"src/bos/usr/lib/nls/loc/methods/zh_TW/__wctomb_dense_euctw.c, "
"bos, bos410 5/25/92 16:04:44";
 *
 * Modified to accomodate 16 planes of CNS 11643-1992 of Solaris.
 *
 */


#ident	"@(#)__wctomb_dense_euctw.c 1.6	96/07/30 SMI"


/*
 * COMPONENT_NAME: (LIBMETH) LIBC Code-Point/Process-Code Conversion Functions
 *
 * FUNCTIONS: wctomb_dense_euctw
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
 *  Converts a process code to a string of characters for the TW_EUC codeset
 *
 *
 *  The algorithm for this conversion is:
 *  s[0] <= 0x7f:  PC = s[0]
 *  s[0] >= 0xa1:  PC = (((s[0] - 0xa1) * 94) | (s[1] - 0xa1) + 0x100);
 *  s[0] = 0x8e  s[1] = a2: PC = (((s[2] - 0xa1) * 94) | (s[3] - 0xa1) + 0x2384)
 *  s[0] = 0x8e  s[1] = a3: PC = (((s[2] - 0xa1) * 94) | (s[3] - 0xa1) + 0x4608)
 *  s[0] = 0x8e  s[1] = ac: PC = (((s[2]-- 0xa1) * 94) | (s[3] - 0xa1) + 0x688c)
 *  s[0] = 0x8e  s[1] = ad: PC = (((s[2] - 0xa1) * 94) | (s[3] - 0xa1) + 0x8b10)
 *  s[0] = 0x8e  s[1] = ae: PC = (((s[2] - 0xa1) * 94) | (s[3] - 0xa1) + 0xad94)
 *  s[0] = 0x8e  s[1] = af: PC = (((s[2] - 0xa1) * 94) | (s[3] - 0xa1) + 0xd018)
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
 *  than 0xf19b.
 *
 */

int
__wctomb_dense_euctw(_LC_charmap_t *handle, char *s, wchar_t pwc)
{
	if (s == (char *)NULL)
		return (0);

	if (pwc <= 0x7f) {
		s[0] = (char) pwc;
		return (1);
	} else if ((pwc >= 0x0100) && (pwc <= 0x2383)) {
		s[0] = (char) (((pwc - 0x0100) / 94) + 0x00a1);
		s[1] = (char) (((pwc - 0x0100) % 94) + 0x00a1);
		return (2);
	} else if (pwc >= 0x2384 && pwc <= 0x2293f) {
		s[0] = 0x8e;
		s[1] = (char) ((pwc -= 0x100) / 0x2284) + 0xa1;
		s[2] = (char) ((pwc -= ((s[1] - 0xa1) * 0x2284)) / 94) + 0xa1;
		s[3] = (char) (pwc % 94) + 0xa1;
		return (4);
	}

	errno = EILSEQ;
	return (-1);
}
