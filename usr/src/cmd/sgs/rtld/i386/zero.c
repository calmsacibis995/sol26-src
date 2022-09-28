/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)zero.c	1.7	96/06/07 SMI"

#include	<sys/types.h>

/*ARGSUSED2*/
void
zero(caddr_t addr, int len, int hint)
{

	while (len-- > 0) {
		/* Align and go faster */
		if (((int)addr & ((sizeof (int) - 1))) == 0) {
			/* LINTED */
			int *w = (int *)addr;
			while (len > 0) {
				*w++ = 0;
				len -= sizeof (int);
			}
			return;
		}
		*addr++ = 0;
	}
}
