/*
 * Copyright (c) 1993-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)prom_prop_ppc.c	1.4	95/07/14 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

int
prom_getintprop(dnode_t nodeid, caddr_t name, int *value)
{
	int	len;

	len = prom_getprop(nodeid, name, (caddr_t)value);
	if (len == sizeof (int))
		*value = prom_decode_int(*value);
	return (len);
}

/*
 * Decode an "encode-phys" property. On PPC OpenFirmware, addresses
 * are always only 32 bit, so "encode-phys" and "encode-int" are really same.
 */
u_int
prom_decode_phys(u_int value)
{
	return (prom_decode_int(value));
}
