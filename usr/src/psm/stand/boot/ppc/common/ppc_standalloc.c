/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ppc_standalloc.c	1.10	96/03/13 SMI"

#include <sys/types.h>
#include <sys/saio.h>
#include <sys/sysmacros.h>
#include <sys/promif.h>
#include <sys/bootconf.h>
#include <sys/salib.h>

#ifdef DEBUG
static int	resalloc_debug = 1;
#else DEBUG
static int	resalloc_debug = 0;
#endif DEBUG
#define	dprintf	if (resalloc_debug) printf

/*
 * standalloc.c is split in to two files. standalloc.c and standalloc_ppc.c.
 * reasone for doing this is: original implementation assumes that first 4mb
 * is mapped 1-1, and uses this area as a scratch memory space. we don't want
 * to do that on powerpc.
 */

caddr_t 	scratchmemp;
extern caddr_t	memlistpage;
extern int	pagesize;

/*
 * Boot can pass this structure to kernel through a BOP call, allowing
 * Kernel to free this memory.
 * This structure will contain memory scratch allocated for boot's usage.
 * If we can add prom's allocation for boot's text+data+bss, it will be Cool
 * solution.
 */
struct	boot_memlist {
	caddr_t	addr;
	u_int	size;
} boot_mem_list;

void
reset_alloc()
{
	extern char _end[];
	int get_sp(void);

	/* Cannot be called multiple times */
	if (memlistpage != (caddr_t)0)
		return;

	memlistpage = (caddr_t)roundup((u_int)&_end, pagesize);
	memlistpage = prom_alloc(memlistpage, pagesize, 0);
	scratchmemp = (caddr_t)(memlistpage + pagesize);
	boot_mem_list.addr = scratchmemp;
	boot_mem_list.size = 0;
	dprintf("memlistpage = %x\n", memlistpage);
	dprintf("scratchmemp = %x\n", scratchmemp);
	dprintf("sp at %x\n", get_sp());

	bzero(memlistpage, pagesize);
}

/*ARGSUSED*/
caddr_t
resalloc(enum RESOURCES type, unsigned bytes, caddr_t virthint, int align)
{
	caddr_t	vaddr;

	if (memlistpage == (caddr_t)0)
		reset_alloc();

	if (bytes == 0)
		return ((caddr_t)0);

	/* extend request to fill a page */
	bytes = roundup(bytes, pagesize);

	dprintf("resalloc:  bytes = %x\n", bytes);

	switch (type) {

	case RES_BOOTSCRATCH:
		virthint = scratchmemp;
		/*
		 * align has to be 0 to get memory at the virthint.
		 */
		vaddr = (caddr_t)prom_alloc(virthint, bytes, 0);
		scratchmemp = vaddr + bytes;
		boot_mem_list.size += bytes;

		if (vaddr == (caddr_t)-1) {
			printf("Error in promalloc for boot\n");
			return ((caddr_t)0);
		}
		return (vaddr);

	case RES_CHILDVIRT:
		/*
		 * align has to be 0 to get memory at the virthint.
		 */
		vaddr = (caddr_t)prom_alloc(virthint, bytes, 0);

		if (vaddr == (caddr_t)virthint)
			return (vaddr);
		dprintf("requested %x got %x\n", virthint, vaddr);
		return ((caddr_t)0);

	default:
		printf("Bad resource type\n");
		return ((caddr_t)0);
	}
	/*NOTREACHED*/
}
