/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)setregid.c	1.1	95/11/05 SMI"

#include <errno.h>

int
setregid(int gid, int egid)
{
	if (gid > 0xffff || egid > 0xffff) {
		errno = EINVAL;
		return (-1);
	}
	return (_setregid(gid, egid));
}
