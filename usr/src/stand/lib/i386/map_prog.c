/*
 * Copyright (c) 1992-1994 Sun Microsystems, Inc.  All Rights Reserved.
 */

#ident "@(#)map_prog.c	1.9	96/04/08 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/promif.h>
#include <sys/bootconf.h>
#include <sys/booti386.h>
#include <sys/bootinfo.h>
#include <sys/mmu.h>
#include <sys/saio.h>
#include <sys/machine.h>
#include <sys/salib.h>

#define	MAGIC_PHYS	(4*1024*1024)
/*
 * For x86 machines, magic_phys used to be at 1MB but this severely limited
 *	the amount of memory available to standalone programs so we've pushed
 *      it up to 4MB; just like sparc.
 *
 * Memory below the 1MB boundary remains a critical resource, however, as all
 *	"realmode" (i.e, 286-like) code is unable to address anything beyond
 *	1MB.  Furthermore, DOS "realmode" drivers supplied by 3rd parties
 *	(including SCSI host adapter drivers, Ethernet extensions, etc) have
 *      to be loaded in the "black hole" between 640KB and 1MB.  Hence, we
 *	preallocate all memory below the 1MB boundary and use special routines
 *	("rm_malloc/rm_free") to allocate it.
 */

caddr_t magic_phys =  (caddr_t)MAGIC_PHYS;
caddr_t top_bootmem = (caddr_t)MAGIC_PHYS;

extern caddr_t memlistpage;

extern struct sysenvmt sysenvmt, *sep;
extern struct bootinfo bootinfo, *bip;
extern struct bootenv bootenv, *btep;

extern unsigned int top_virtaddr;

extern void *memset(void *dest, int c, size_t cnt);
caddr_t resalloc(enum RESOURCES, unsigned, caddr_t, int);

/* These are the various memory lists in boot.c */
extern struct memlist 	*pfreelistp, /* physmem available */
		*vfreelistp, /* virtmem available */
		*pinstalledp;   /* physmem installed */

extern void 	insert_node(struct memlist **node, struct memlist *newnode);
extern struct memlist *search_list(struct memlist *list,
		struct memlist *request);
extern void   print_memlist(struct memlist *list);

paddr_t
map_mem(u_int user_vaddr, u_int size, int align)
{
	register uint virtaddr;
	register ptbl_t	*ptp;
	uint	pgs, vpgs;
	ptbl_t	*pdp;
	uint virtaddr_end, virtaddr_start;
	uint mode;
	extern uint	bpd_loc;
	struct memlist m, *mp;
	paddr_t find_mem();
	paddr_t	physaddr, physaddr_sav;
	int availseg;
#if (defined(BOOT_DEBUG))
	uint size_sav;
#endif

	if (btep->db_flag & BOOTTALK) {
	    printf("map_mem: addr 0x%x size 0x%x align 0x%x\n",
		user_vaddr, size, align);
	}

	if ((physaddr = find_mem(size, align, &availseg)) == (paddr_t)-1) {
		/*
		 *  Oops: out of memory!
		 */

		return (physaddr);
	}

	physaddr_sav = physaddr;
	virtaddr = (user_vaddr ? user_vaddr : top_virtaddr);

	pgs = vpgs = 0;
	mode = PG_P | PG_RW;
	pdp = (ptbl_t *)bpd_loc;
	virtaddr_start = virtaddr;
	virtaddr_end = (virtaddr+size+MMU_PAGESIZE-1) & MMU_STD_PAGEMASK;


#if (defined(BOOT_DEBUG))
	if (btep->db_flag & BOOTTALK) {
		printf("\nMap at paddr %x virt %x virtend %x size %x seg %d "
		"base %x ext %x mode %d\n",
		    physaddr, virtaddr, virtaddr_end, size, availseg,
		    bip->memavail[availseg].base,
		    bip->memavail[availseg].extent, mode);
	}
#endif
	/*
	 *  Step thru all pages in virtual memory and make sure they
	 *  get mapped properly.
	 */
	while (virtaddr < virtaddr_end) {

	    uint pdir = MMU_L1_INDEX(virtaddr);

	    if (!(pdp->page[pdir] & PG_P)) {
		/*
		 *   Allocate a new page table for the mapping.
		 */

		ptp = (ptbl_t *)resalloc(RES_BOOTSCRATCH, MMU_PAGESIZE, 0, 0);
		pdp->page[pdir] = ((uint)ptp | PG_P | PG_RW);
		(void) memset(ptp, '\0', MMU_PAGESIZE);

	    } else {
		/*
		 *  Page table  already exists, use page directory to locate
		 *  it.
		 */

		ptp = (ptbl_t *)(((uint)pdp->page[pdir]) & MMU_STD_PAGEMASK);
	    }

	    while ((pdir == MMU_L1_INDEX(virtaddr)) &&
		(virtaddr < virtaddr_end)) {
		/*
		 * if address was previously mapped but in different mode,
		 * new mode will be or'd in
		 */

		uint pndx = MMU_L2_INDEX(virtaddr);

		if (ptp->page[pndx] & PG_P) {
		    /* XXX - memory leak; re-map with new mode! */
		    ptp->page[pndx] |= mode;
		    vpgs++;
		} else {
		    /* Unmapped page, initialize the new pte */
		    ptp->page[pndx] = ((uint)physaddr&MMU_STD_PAGEMASK) | mode;
		    physaddr += MMU_PAGESIZE;
		    pgs++;
		}

		virtaddr += MMU_PAGESIZE; /* Next virtual addr */
	    }
	}

	/* Reduce available memory by the number of pages we just mapped */
	bip->memavail[availseg].extent -= (pgs * MMU_PAGESIZE);
	bip->memavail[availseg].base += (pgs * MMU_PAGESIZE);

	if (((virtaddr_end+MMU_PAGESIZE-1) & MMU_STD_PAGEMASK) > top_virtaddr) {
		/*
		 * We've extended the top-most segment.  Reset the
		 * "top_virtaddr" pointer appropriately.
		 */

		top_virtaddr = (virtaddr_end+MMU_PAGESIZE-1) & MMU_STD_PAGEMASK;
	}

	if ((m.size = (pgs * MMU_PAGESIZE)) > 0) {
#if (defined(BOOT_DEBUG))
		size_sav = m.size;
#endif
		/*
		 *  Update the available memory lists if necessary, physical
		 *  addrs first.
		 */

		physaddr_sav &= MMU_STD_PAGEMASK;
		m.address = physaddr_sav;
		mp = search_list(pfreelistp, &m);

#if (defined(BOOT_DEBUG))
		printf("Take out physavail range start 0x%x size 0x%x\n",
			physaddr_sav, size_sav);
		printf("Dump physical available list before insert\n");
		print_memlist(pfreelistp);
#endif
		if (mp != (struct memlist *)0) {
#if (defined(BOOT_DEBUG))
			printf("Found node with addr 0x%x, size 0x%x\n",
				mp->address, mp->size);
#endif
			insert_node(&mp, &m);
#if (defined(BOOT_DEBUG))
			printf("Dump physical available list after insert\n");
			print_memlist(pfreelistp);
#endif
		}
#if (defined(BOOT_DEBUG))
		if (mp == (struct memlist *)0) {
			printf("Node not found (memory leak?)\n");
		}
#endif

		/* update the virtual address available list */
		physaddr_sav = m.address = virtaddr_start & MMU_STD_PAGEMASK;
		m.size = (vpgs + pgs) * MMU_PAGESIZE;
#if (defined(BOOT_DEBUG))
		size_sav = m.size;
#endif
		mp = search_list(vfreelistp, &m);

#if (defined(BOOT_DEBUG))
		printf("Take out virtavail range start 0x%x size 0x%x\n",
			physaddr_sav, size_sav);
		printf("Dump virtual available list before insert\n");
		print_memlist(vfreelistp);
#endif
		if (mp != (struct memlist *)0) {
#if (defined(BOOT_DEBUG))
			printf("Found node with addr 0x%x, size 0x%x\n",
			mp->address, mp->size);
#endif
			insert_node(&mp, &m);
#if (defined(BOOT_DEBUG))
			printf("Dump virtual available list after insert\n");
			print_memlist(vfreelistp);
#endif
		} else {
			m.address += MMU_PAGESIZE;
			m.size -= MMU_PAGESIZE;
			mp = search_list(vfreelistp, &m);

			if (mp != (struct memlist *)0) {
#if (defined(BOOT_DEBUG))
				printf("Found node with addr 0x%x, size 0x%x\n",
					mp->address, mp->size);
#endif
				insert_node(&mp, &m);
#if (defined(BOOT_DEBUG))
				printf("Dump virtual available"
					"list after insert\n");
				print_memlist(vfreelistp);
#endif
			}
#if (defined(BOOT_DEBUG))
			if (mp == (struct memlist *)0) {
				if (btep->db_flag & BOOTTALK) {
					printf("map_mem: memory list fail\n");
				}
			}
#endif
		}
	}

#if (defined(BOOT_DEBUG))
	/*
	 *  Let user look at the debugging output!
	 */

	(void) goany();
#endif
	return ((paddr_t)virtaddr_start);
}
