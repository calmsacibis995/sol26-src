/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__wcwidth_sb.c 1.4	96/07/02  SMI"

/*
 * static char sccsid[] = "@(#)62	1.2.1.2  src/bos/usr/ccs/lib/libc/"
 * "__wcwidth_latin.c, bos, bos410 1/12/93 11:12:11";
 */
/*
 * COMPONENT_NAME: (LIBCCPPC) LIBC Code-Point/Process-Code Conversion Functions
 *
 * FUNCTIONS: __wcwidth_sb
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
#include <sys/localedef.h>

/*
 *  returns the number of characters for a SINGLE-BYTE codeset
*/
int
__wcwidth_sb(_LC_charmap_t *hdl, wchar_t wc)
{
	/*
	 * if wc is null, return 0
	*/
	if (wc == (wchar_t) '\0')
		return (0);

	if (!iswprint(wc))
		return (-1);

	/*
	 * single-display width
	*/
	return (1);

}
