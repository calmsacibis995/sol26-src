/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 */

#pragma	ident "@(#)prom_getversion.c	1.2	96/03/23 SMI"
#define	PROM_VERSION_NUMBER 3   /* Fake version number (1275-like) */

int
prom_getversion(void)
{
	return (PROM_VERSION_NUMBER);
}
