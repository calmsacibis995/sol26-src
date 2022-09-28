#ident	"@(#)octet.c	1.3	96/07/08 SMI"

/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>

/*
 * Converts an octet string into an ASCII string.
 */
int
octet_to_ascii(u_char *nump, int nlen, char *bufp, int *blen)
{
	register int	i;
	register char	*bp;
	register u_char	*np;
	static char	ascii_conv[] = {'0', '1', '2', '3', '4', '5', '6',
	    '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

	if (nump == NULL || bufp == NULL || blen == NULL)
		return (EINVAL);

	if ((nlen * 2) >= *blen) {
		*blen = 0;
		return (E2BIG);
	}

	for (i = 0, bp = bufp, np = nump; i < nlen; i++) {
		*bp++ = ascii_conv[(np[i] >> 4) & 0x0f];
		*bp++ = ascii_conv[np[i] & 0x0f];
	}
	*bp = '\0';
	*blen = i * 2;
	return (0);
}
