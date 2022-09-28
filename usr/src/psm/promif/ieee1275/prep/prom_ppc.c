/*
 * Copyright (c) 1993-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)prom_ppc.c	1.2	95/05/07 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * P1275 Client Interface Functions defined for PowerPC.
 * we have this interface to closely follow sparc implementation.
 * This file belongs in a platform dependent area.
 */

/*
 * This function returns NULL or a a verified client interface structure
 * pointer to the caller. we need to verify the cookie somehow..
 */

int (*cif_handler)(void *);

void *
p1275_ppc_cif_init(void *cookie)
{
	cif_handler = (int (*)(void *))cookie;
	return ((void *)cookie);
}


int
p1275_ppc_cif_handler(void *p)
{
	cell_t rv;

	if (cif_handler == 0)
		return (-1);

	rv = (*cif_handler)(p);
	return (p1275_cell2int(rv));
}
