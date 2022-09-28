/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)setgid.c	1.1	95/11/05 SMI"

#include <errno.h>

int
setgid(int gid)
{
	if (geteuid() == 0)
		return (setregid(gid, gid));
	else
		return (setregid(-1, gid));
}
