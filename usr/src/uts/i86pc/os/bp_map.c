/*
 * Copyright (c) 1990-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bp_map.c	1.12	96/06/07 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/map.h>
#include <sys/mman.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/pte.h>
#include <sys/mmu.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <vm/as.h>
#include <vm/seg_kmem.h>
#include <vm/hat_i86.h>
#include <vm/page.h>

/*
 * Global Routines:
 *
 * void bp_mapin()
 * void bp_mapout()
 */

/*
 * Static Routines
 */
static void bp_map(register struct buf *bp, caddr_t kaddr);


/*
 * Map the data referred to by the buffer bp into the kernel
 * at kernel virtual address kaddr.
 *
 * Only called from bp_mapin below.
 */
static void
bp_map(register struct buf *bp, caddr_t kaddr)
{
	register page_t *pp = NULL;
	int npf;
	struct as *as;
	unsigned int pfnum;
	caddr_t addr;
	page_t **pplist = NULL;

	if (bp->b_flags & B_PAGEIO) {
		pp = bp->b_pages;
	} else if (bp->b_flags & B_SHADOW) {
		pplist = bp->b_shadow;
	} else {
		if (bp->b_proc == NULL || (as = bp->b_proc->p_as) == NULL) {
			as = &kas;
		}
		addr = (caddr_t)bp->b_un.b_addr;
	}

	/*
	 * We use segkmem_mapin to set up the mappings now since some
	 * users of kernelmap aren't nice enough to unmap things
	 * when they are done and segkmem_mapin handles this as a special case.
	 */
	ASSERT(kaddr < Syslimit);

	npf = btoc(bp->b_bcount + ((int)bp->b_un.b_addr & PAGEOFFSET));

	while (npf > 0) {
		if (pp) {
			pfnum = page_pptonum(pp);
			pp = pp->p_next;
			ASSERT(npf == 1 || pp != NULL);
		} else if (pplist == NULL) {
			pfnum = hat_getpfnum(as->a_hat, addr);
			addr += MMU_PAGESIZE;
		} else {
			pfnum = page_pptonum(*pplist);
			pplist++;
		}

		segkmem_mapin(&kvseg, kaddr, MMU_PAGESIZE,
		    PROT_READ | PROT_WRITE|HAT_NOSYNC, pfnum, HAT_LOAD);

		kaddr += MMU_PAGESIZE;
		npf--;
	}
}

/*
 * Called to convert bp for pageio/physio to a kernel addressable location.
 * We allocate virtual space from the kernelmap and then use bp_map to do
 * most of the real work.
 */
void
bp_mapin(register struct buf *bp)
{
	int npf, o;
	long a;
	caddr_t kaddr;

	if ((bp->b_flags & (B_PAGEIO | B_PHYS)) == 0 ||
	    (bp->b_flags & B_REMAPPED) != 0)
		return;	/* no pageio/physio or already mapped in */

	ASSERT((bp->b_flags & (B_PAGEIO | B_PHYS)) != (B_PAGEIO | B_PHYS));

	o = (int)bp->b_un.b_addr & PAGEOFFSET;
	npf = btoc(bp->b_bcount + o);

	/*
	 * Allocate kernel virtual space for remapping.
	 */
	mutex_enter(&maplock(kernelmap));
	while ((a = (long)rmalloc_locked(kernelmap, npf)) == 0) {
		mapwant(kernelmap) = 1;
		cv_wait(&map_cv(kernelmap), &maplock(kernelmap));
	}
	mutex_exit(&maplock(kernelmap));
	kaddr = Sysbase + mmu_ptob(a);

	/* map the bp into the virtual space we just allocated */
	bp_map(bp, kaddr);

	bp->b_flags |= B_REMAPPED;
	bp->b_un.b_addr = kaddr + o;
}

/*
 * bp_mapout releases all the resources associated with a bp_mapin call.
 * We call hat_unload to release the work done by bp_map which will insure
 * that the reference and modified bits from this mapping are not OR'ed in.
 */
void
bp_mapout(register struct buf *bp)
{
	int npf;
	u_long a;
	caddr_t	addr;

	if (bp->b_flags & B_REMAPPED) {
		addr = (caddr_t)((int)bp->b_un.b_addr & PAGEMASK);
		bp->b_un.b_addr = (caddr_t)((int)bp->b_un.b_addr & PAGEOFFSET);
		npf = mmu_btopr(bp->b_bcount + (int)bp->b_un.b_addr);
		hat_unload(kas.a_hat, addr,
		    (u_int)mmu_ptob(npf), HAT_UNLOAD_UNLOCK);
		a = mmu_btop(addr - Sysbase);
		rmfree(kernelmap, (long)npf, a);
		bp->b_flags &= ~B_REMAPPED;
	}
}
