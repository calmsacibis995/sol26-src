/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)setreuid.c	1.1	95/11/05 SMI"

#include <errno.h>

int
setreuid(int uid, int euid)
{
	if (uid > 0xffff || euid > 0xffff) {
		errno = EINVAL;
		return (-1);
	}
	return (_setreuid(uid, euid));
}
