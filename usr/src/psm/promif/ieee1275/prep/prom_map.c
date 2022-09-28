/*
 * Copyright (c) 1993-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)prom_map.c	1.4	95/07/14 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * Mapping routines suitable for implementations using 1-cell physical
 * address formats.  Use of these routines makes the caller
 * platform-dependent.
 *
 * Note: "space" contains the high order 32 bits of a physical address.
 * Existing prep PPC plaforms do not use this (passing a 0 value).
 */
caddr_t
prom_map(caddr_t virthint, u_int space, u_int phys, u_int size)
{
	caddr_t virt;

	if (space != (u_int)0)
		prom_panic("prom_map: space contains a non-NULL value\n");
	/*
	 * If no virthint, allocate it; otherwise claim it,
	 * the physical address is assumed to be a device or
	 * already claimed, or not appearing in a resource list.
	 */
	if (virthint == (caddr_t)0)  {
		if ((virt = prom_allocate_virt((u_int)PROMIF_MIN_ALIGN,
		    size)) == 0)
			return ((caddr_t)0);
	} else {
		virt = virthint;
		if (prom_claim_virt(size, virt) != virt)
			return ((caddr_t)0);
	}

	/*
	 * we can specify mode bits WIMG bits for ppc instead of default
	 * prom settings
	 */
	if (prom_map_phys(-1, size, virt, space, phys) != 0) {
		/*
		 * The map operation failed, free the virtual
		 * addresses we allocated or claimed.
		 */
		(void) prom_free_virt(size, virt);
		return ((caddr_t)0);
	}
	return (virt);
}

void
prom_unmap(caddr_t virt, u_int size)
{
	(void) prom_unmap_virt(size, virt);
	prom_free_virt(size, virt);
}
