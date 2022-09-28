/*
 * Copyright (c) 1990-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bp_map.c	1.23	96/05/15 SMI"

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
#include <sys/devaddr.h>
#include <vm/as.h>
#include <vm/seg_kmem.h>
#include <vm/hat_srmmu.h>
#include <vm/page.h>
#include <sys/iommu.h>
#include <sys/bt.h>

/*
 * Global Routines:
 *
 * int impl_read_hwmap()
 * void bp_mapin()
 * void bp_mapout()
 */

/*
 * Static Routines
 */
static int bp_alloc(struct map *map, register struct buf *bp, int size);
static void bp_map(register struct buf *bp, caddr_t kaddr);

/*
 * Common routine for bp_map and iommunex_dma_map which will read in the
 * pte's from the hardware mapping registers into the pte array given.
 * If asked, we also enforce some rules on non-obmem ptes. Note that
 * the mapping must be locked down.
 */
int
impl_read_hwmap(struct as *as, caddr_t addr, int npf,
    struct pte *pte, int cknonobmem)
{
	struct pte *hpte, tpte;
	int bustype = BT_DRAM;
	int chkbt = 1;
	int level = 0;
	u_int off, span;
	struct ptbl *ptbl;
	kmutex_t *mtx;

	hpte = NULL;

	while (npf != 0) {
		if (level != 3 || ((u_int)MMU_L2_OFF(addr) < MMU_L3_SIZE)) {
			hpte = srmmu_ptefind_nolock(as, addr, &level);
		} else {
			hpte++;
		}

		mmu_readpte(hpte, &tpte);
		if (!pte_valid(&tpte)) {
			/*
			 * Even locked ptes can turn up invalid
			 * if we call ptefind without the hat lock
			 * if another cpu is sync'ing the rm bits
			 * at the same time.  We avoid this race
			 * by retrying ptefind with the lock.
			 */
			hpte = srmmu_ptefind(as, addr, &level,
				&ptbl, &mtx, LK_PTBL_SHARED);

			mmu_readpte(hpte, &tpte);
			unlock_ptbl(ptbl, mtx);

			if (!pte_valid(&tpte)) {
				cmn_err(CE_CONT, "impl_read_hwmap no pte\n");
				bustype = -1;
				break;
			}
		}

		switch (level) {
		case 3:
			off = 0;
			span = 1;
			break;
		case 2:
			off = MMU_L2_OFF(addr);
			span = MIN(npf, mmu_btopr(MMU_L2_SIZE - off));
			break;
		case 1:
			off = MMU_L1_OFF(addr);
			span = MIN(npf, mmu_btopr(MMU_L1_SIZE - off));
			break;
		}
		off = mmu_btop(off);

		if (cknonobmem) {
			if (chkbt) {
				bustype = impl_bustype(tpte.PhysicalPageNumber);
				if (bustype == BT_UNKNOWN) {
					cmn_err(CE_CONT,
						"impl_read_hwmap BT_UNKNOWN\n");
					bustype = -1;
					break;
				}
				chkbt = 0;
			} else {
				if (impl_bustype(tpte.PhysicalPageNumber) !=
				    bustype) {
					/*
					 * we don't allow mixing bus types.
					 */
					cmn_err(CE_CONT,
					    "impl_read_hwmap: mixed bustype\n");
					bustype = -1;
					break;
				}
			}
		}

		/*
		 * We make the translation writable, even if the current
		 * mapping is read only.  This is necessary because the
		 * new pte is blindly used in other places where it needs
		 * to be writable.
		 */
		tpte.PhysicalPageNumber += off;
		tpte.AccessPermissions = MMU_STD_SRWX; /* XXX -  generous? */

		/*
		 * Copy the hw ptes to the sw array.
		 */
		npf -= span;
		addr += mmu_ptob(span);
		while (span--) {
			*pte = tpte;
			tpte.PhysicalPageNumber++;
			pte++;
		}
	}

	/*
	 * we return bustype or -1 (failure).
	 */
	return (bustype);
}

/*
 * PTECHUNKSIZE is just a convenient number of pages to deal with at a time.
 * No way does it reflect anything but that.
 */
#define	PTECHUNKSIZE	16

static void
bp_map(register struct buf *bp, caddr_t kaddr)
{
	struct pte ptecache[PTECHUNKSIZE];
	struct pte *pte = &ptecache[0];
	page_t *pp = NULL;
	int npf, cidx;
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
		cidx = PTECHUNKSIZE;
	}

	/*
	 * We want to use mapin to set up the mappings now since some
	 * users of kernelmap aren't nice enough to unmap things
	 * when they are done and mapin handles this as a special case.
	 * If kaddr is in the kernelmap space, we use kseg so the
	 * software ptes will get updated.
	 */
	ASSERT(kaddr < Syslimit);

	npf = btoc(bp->b_bcount + ((int)bp->b_un.b_addr & PAGEOFFSET));

	while (npf > 0) {
		if (pp) {
			pfnum = page_pptonum(pp);
			pp = pp->p_next;
		} else if (pplist == NULL) {
			if (cidx == PTECHUNKSIZE) {
				int np = MIN(npf, PTECHUNKSIZE);
				if (impl_read_hwmap(as, addr, np,
				    ptecache, 0) < 0)
					cmn_err(CE_PANIC,
					    "bp_map: failed");
				addr += mmu_ptob(np);
				cidx = 0;
			}
			pte = &ptecache[cidx++];
			pfnum = MAKE_PFNUM(pte);
		} else {
			pfnum = page_pptonum(*pplist);
			pplist++;
		}

		segkmem_mapin(&kvseg, kaddr, MMU_PAGESIZE,
		    PROT_READ | PROT_WRITE | HAT_NOSYNC, pfnum, HAT_LOAD);

		kaddr += MMU_PAGESIZE;
		npf--;
	}
}

/*
 * Allocate 'size' units from the given map so that
 * the vac alignment constraints for bp are maintained.
 *
 * Return 'addr' if successful, 0 if not.
 */
static int
bp_alloc(struct map *map, register struct buf *bp, int size)
{
	struct map *mp;
	u_long addr, mask;
	int align = -1;
	page_t *pp = NULL;
	extern int fd_page_vac_align();

	ASSERT(MUTEX_HELD(&maplock(map)));

	if (vac) {
		if ((bp->b_flags & B_PAGEIO) != 0) {
			/*
			 * Peek at the first page's alignment.
			 * We could work harder and check the alignment
			 * of all the pages.  If a conflict is found
			 * and the page is not kept (more than once
			 * if intrans), then try to do hat_pageunload()'s
			 * to allow the IO to be cached.  However,
			 * this is very unlikely and not worth the
			 * extra work (for now at least).
			 */
			pp = bp->b_pages;
			ohat_mlist_enter(pp);
			align = fd_page_vac_align(pp);
			ohat_mlist_exit(pp);

		} else if (bp->b_un.b_addr != NULL) {
			align = mmu_btop((int)bp->b_un.b_addr &
			    (shm_alignment - 1));
		}
	}

	if (align == -1)
		return (rmalloc_locked(map, (size_t)size));

	/*
	 * Look for a map segment containing a request that works.
	 * If none found, return failure.
	 * Since VAC has a much stronger alignment requirement,
	 * we'll use shm_alignment even ioc is on too.
	 */

	if (vac)
		mask = mmu_btop(shm_alignment) - 1;

	for (mp = mapstart(map); mp->m_size; mp++) {
		if (mp->m_size < size)
			continue;

		/*
		 * Find first addr >= mp->m_addr that
		 * fits the alignment constraints.
		 */
		addr = (mp->m_addr & ~mask) + align;
		if (addr < mp->m_addr)
			addr += mask + 1;

		/*
		 * See if it fit within the map.
		 */
		if (addr + size <= mp->m_addr + mp->m_size)
			break;
	}

	if (mp->m_size == 0)
		return (0);

	/* let rmget() do the rest of the work */
	return (rmget(map, (long)size, (u_long)addr));

}

/*
 * Called to convert bp for pageio/physio to a kernel addressable location.
 * We allocate virtual space from the kernelmap and then use bp_map to do
 * most of the real work.
 */
void
bp_mapin(struct buf *bp)
{
	int npf, o;
	long a;
	caddr_t kaddr;

	if ((bp->b_flags & (B_PAGEIO | B_PHYS)) == 0 ||
	    (bp->b_flags & B_REMAPPED) != 0)
		return;		/* no pageio/physio or already mapped in */

	ASSERT((bp->b_flags & (B_PAGEIO | B_PHYS)) != (B_PAGEIO | B_PHYS));

	o = (int)bp->b_un.b_addr & PAGEOFFSET;
	npf = btoc(bp->b_bcount + o);

	/*
	 * Allocate kernel virtual space for remapping.
	 */
	mutex_enter(&maplock(kernelmap));
	while ((a = bp_alloc(kernelmap, bp, npf)) == 0) {
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
 * bp_mapout will release all the resources associated with a bp_mapin call.
 * We call hat_unload to release the work done by bp_map which will insure
 * that the reference and modified bits from this mapping are not OR'ed in.
 */
void
bp_mapout(struct buf *bp)
{
	int npf;
	u_long a;
	caddr_t	addr;

	if (bp->b_flags & B_REMAPPED) {
		addr = (caddr_t)((int)bp->b_un.b_addr & PAGEMASK);
		bp->b_un.b_addr = (caddr_t)((int)bp->b_un.b_addr & PAGEOFFSET);
		npf = mmu_btopr(bp->b_bcount + (int)bp->b_un.b_addr);
		hat_unload(kas.a_hat, addr, (u_int)mmu_ptob(npf),
			(HAT_UNLOAD_NOSYNC | HAT_UNLOAD_UNLOCK));
		a = mmu_btop(addr - Sysbase);
		bzero((caddr_t)&Usrptmap[a], (u_int)(sizeof (Usrptmap[0])*npf));
		rmfree(kernelmap, (size_t)npf, a);
		bp->b_flags &= ~B_REMAPPED;
	}
}
