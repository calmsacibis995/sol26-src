/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)setuid.c	1.1	95/11/05 SMI"

int
setuid(int uid)
{
	if (geteuid() == 0)
		return (setreuid(uid, uid));
	else
		return (setreuid(-1, uid));
}
