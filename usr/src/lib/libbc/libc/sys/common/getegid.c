/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)getegid.c	1.1	95/11/05 SMI"

int
getegid(void)
{
	int egid;

	if ((egid = _getegid()) > 0xffff)
		egid = 60001;	/* nobody */
	return (egid);
}
