/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)setegid.c	1.1	95/11/05 SMI"

int
setegid(int egid)
{
	return (setregid(-1, egid));
}
