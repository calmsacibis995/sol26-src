/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)getgid.c	1.1	95/11/05 SMI"

int
getgid(void)
{
	int gid;

	if ((gid = _getgid()) > 0xffff)
		gid = 60001;	/* nobody */
	return (gid);
}
