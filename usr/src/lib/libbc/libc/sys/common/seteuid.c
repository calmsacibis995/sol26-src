/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)seteuid.c	1.1	95/11/05 SMI"

int
seteuid(int euid)
{
	return (setreuid(-1, euid));
}
