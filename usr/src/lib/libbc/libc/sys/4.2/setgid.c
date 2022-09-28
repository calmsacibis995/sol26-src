/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)setgid.c	1.1	95/11/05 SMI"

int
setgid(int gid)
{
	return (setregid(gid, gid));
}
