/*
 * Copyright (c) 1991-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)prom_alloc.c	1.3	95/07/14 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

caddr_t
prom_alloc(caddr_t virthint, u_int size, u_int align)
{
	/*
	 * align should be 0 to get memory at the virthint, if we find that
	 * this is not acceptable, we can use prom_alloc_ppc().
	 */
	return (prom_malloc(virthint, size, align));
}

/*
 * This is the generic client interface to "claim" memory.
 * These two routines belong in the common directory.
 */
caddr_t
prom_malloc(caddr_t virt, u_int size, u_int align)
{
	cell_t ci[7];
	int rv;

	ci[0] = p1275_ptr2cell("claim");	/* Service name */
	ci[1] = (cell_t)3;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_ptr2cell(virt);		/* Arg1: virt */
	ci[4] = p1275_uint2cell(size);		/* Arg2: size */
	ci[5] = p1275_uint2cell(align);		/* Arg3: align */

	promif_preprom();
	rv = p1275_cif_handler(&ci);
	promif_postprom();

	if (rv == 0)
		return ((caddr_t)p1275_cell2ptr(ci[6])); /* Res1: base */
	return ((caddr_t)-1);
}


void
prom_free(caddr_t virt, u_int size)
{
	cell_t ci[5];

	ci[0] = p1275_ptr2cell("release");	/* Service name */
	ci[1] = (cell_t)2;			/* #argument cells */
	ci[2] = (cell_t)0;			/* #result cells */
	ci[3] = p1275_ptr2cell(virt);		/* Arg1: virt */
	ci[4] = p1275_uint2cell(size);		/* Arg2: size */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();
}

/*
 * This implementation of alloc uses /memory and /chosen's mmu. we have
 * greater control over allocation. We will use this if we need to.
 */

caddr_t
prom_alloc_ppc(caddr_t virthint, u_int size, u_int align)
{

	caddr_t virt = virthint;
	u_int addr;

	if (align == 0)
		align = (u_int)4;

	/*
	 * First, allocate or claim the virtual address space.
	 * In either case, after this code, "virt" is the chosen address.
	 */
	if (virthint == 0) {
		virt = prom_allocate_virt(align, size);
		if (virt == (caddr_t)-1)
			return ((caddr_t)0);
	} else {
		if (prom_claim_virt(size, virthint) == (caddr_t)-1)
			return ((caddr_t)0);
	}

	/*
	 * Next, allocate the physical address space, at the specified
	 * physical alignment (or 4 byte alignment, if none specified)
	 */

	if (prom_allocate_phys(size, align, &addr) == -1) {

		/*
		 * Request failed, free virtual address space and return.
		 */
		prom_free_virt(size, virt);
		return ((caddr_t)0);
	}

	/*
	 * Next, create a mapping from the physical to virtual address,
	 * using a default "mode".
	 */

	if (prom_map_phys(-1, size, virt, 0, addr) == -1)  {

		/*
		 * The call failed; release the physical and virtual
		 * addresses allocated or claimed, and return.
		 */

		prom_free_virt(size, virt);
		prom_free_phys(size, addr);
		return ((caddr_t)0);
	}
	return (virt);
}
