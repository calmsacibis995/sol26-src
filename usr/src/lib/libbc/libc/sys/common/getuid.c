/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)getuid.c	1.1	95/11/05 SMI"

int
getuid(void)
{
	int uid;

	if ((uid = _getuid()) > 0xffff)
		uid = 60001;	/* nobody */
	return (uid);
}
