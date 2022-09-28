/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__wcstombs_sb.c 1.6	96/10/14  SMI"

/*
 * static char sccsid[] = "@(#)49	1.2.1.1  src/bos/usr/ccs/lib/libc/"
 * "__wcstombs_sb.c, bos, bos410 5/25/92 13:44:07";
*/
/*
 * COMPONENT_NAME: (LIBCCCPPC) LIBC Code-Point/Process-Code Conversion
 * 			Functions
 *
 * FUNCTIONS: __wcstombs_sb
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

size_t
__wcstombs_sb(_LC_charmap_t *hdl, char *s, const wchar_t *pwcs, size_t n)
{
	size_t len = n;
	wchar_t *pwcs0 = (wchar_t *)pwcs;

	/*
	* if s is a null pointer, just count the number of characters
	* in pwcs
	*/
	if (s == (char *)NULL) {
		while (*pwcs != '\0')
			pwcs++;
		return (pwcs - pwcs0);
	}

	/*
	* only do n or less characters
	*/
	while (len-- > 0) {
		if ((*pwcs >= 0) && (*pwcs <= 255))
			*s = (char) *pwcs;
		else
			return (-1);

	/*
	 * if pwcs is null, return
	 */
		if (*pwcs == '\0')
			return (pwcs - pwcs0);

	/*
	 * increment s to the next character
	 */
		s++;
		pwcs++;
	}

	/*
	*  Ran out of room in s before null was hit on wcs, return n
	*/
	return (n);
}
