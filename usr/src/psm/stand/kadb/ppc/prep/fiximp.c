/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 */

#pragma ident "@(#)fiximp.c	1.1	95/01/27 SMI"

#define	DEFAULT_PAGESIZE		0x1000

int pagesize = DEFAULT_PAGESIZE;

void
fiximp(void)
{
	pagesize = DEFAULT_PAGESIZE;
}
