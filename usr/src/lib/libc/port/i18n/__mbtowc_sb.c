/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__mbtowc_sb.c 1.4	96/07/02  SMI"

/*
 * static char sccsid[] = "@(#)46	1.3.1.1  src/bos/usr/ccs/lib/libc/"
 * "__mbtowc_sb.c , bos, bos410 5/25/92 13:43:49";
*/
/*
 * COMPONENT_NAME: (LIBCCCPPC) LIBC Code-Point/Process-Code Conversion Functions
 *
 * FUNCTIONS: __mbtowc_sb
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
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/localedef.h>

int
__mbtowc_sb(_LC_charmap_t *hdl, wchar_t *pwc, const char *ts, size_t len)
{
	unsigned char *s = (unsigned char *)ts;

	/*
	 * If length == 0 return -1
	 */
	if (len < 1) {
		errno = EILSEQ;
		return ((size_t)-1);
	}

	/*
	 *if s is NULL return 0
	*/
	if (s == NULL)
		return (0);

	/*
	 * if pwc is not NULL pwc to s
	 * length is 1 unless NULL which has length 0
	 */
	if (pwc != (wchar_t *)NULL)
		*pwc = (wchar_t)*s;
	if (s[0] != '\0')
		return (1);
	else
		return (0);
}
