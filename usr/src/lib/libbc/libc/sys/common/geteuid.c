/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)geteuid.c	1.1	95/11/05 SMI"

int
geteuid(void)
{
	int euid;

	if ((euid = _geteuid()) > 0xffff)
		euid = 60001;	/* nobody */
	return (euid);
}
