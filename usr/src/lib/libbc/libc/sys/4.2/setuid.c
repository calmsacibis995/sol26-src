/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)setuid.c	1.1	95/11/05 SMI"

int
setuid(int uid)
{
	return (setreuid(uid, uid));
}
