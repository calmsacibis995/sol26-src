/*
 * Copyright (c) 1990-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bp_map.c	1.17	96/06/10 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/map.h>
#include <sys/mman.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/pte.h>
#include <sys/mmu.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/machsystm.h>
#include <sys/debug.h>
#include <vm/as.h>
#include <vm/seg_kmem.h>
#include <vm/mach_page.h>

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
 * Common routine for bp_map and rootnex_dma_map which will read in the
 * pte's from the hardware mapping registers into the pte array given and
 * return bustype. If asked, we also enforce some rules on non-obmem ptes.
 */

#define	sunm_addrtopte(pmg, a) \
	((pmg)->pmg_pte + (((u_int)(a) & PMGRPOFFSET)  >> PAGESHIFT))

int
impl_read_hwmap(struct as *as, caddr_t addr, int npf, struct pte *pte,
    int cknonobmem)
{
	struct pmgrp *pmgrp = (struct pmgrp *)NULL;
	int bustype = -1;

	while (npf-- > 0) {
		/*
		 * If we're not the kernel address space, point us
		 * at the software copy of the pte.
		 */

		if (as != &kas) {
			/*
			 * If we haven't gotten the first pmgrp, or if
			 * we're at the beginning of the next pmgrp's
			 * address range, point us at the first pmgrp
			 * that would contain our address.
			 */
			if ((pmgrp == (struct pmgrp *)NULL) ||
			    (((int)addr & PMGRPOFFSET) < MMU_PAGESIZE)) {
				struct sunm *sunm;
				caddr_t saddr;

				saddr = (caddr_t)((int)addr & PMGRPMASK);
				sunm = (struct sunm *)as->a_hat->hat_data;
				for (pmgrp = sunm->sunm_pmgrps;
					pmgrp && pmgrp->pmg_base != saddr;
						pmgrp = pmgrp->pmg_next)
							;
				if (pmgrp == (struct pmgrp *)NULL) {
					bustype = -1;
					break;
				}
			}
			*pte = *sunm_addrtopte(pmgrp, addr);
		} else {
			mmu_getpte(addr, pte);
		}

		if (!pte_valid(pte) && (as == &kas)) {
			/*
			 * Even locked ptes can turn up invalid
			 * if we call ptefind without the hat lock
			 * if another cpu is sync'ing the rm bits
			 * at the same time.  We avoid this race
			 * by retrying ptefind with the lock.
			 *
			 * This is the same check as in the sun4m/sun4d
			 * case, and brought over for completeness.  We
			 * only provide this in the kas case since otherwise,
			 * we would have grabbed the software copies of the
			 * pte's (they should have been protected by a higher
			 * level lock -- see hat_unload()).
			 */
			mutex_enter(&sunm_mutex);
			mmu_getpte(addr, pte);
			mutex_exit(&sunm_mutex);

			if (!pte_valid(pte)) {
				bustype = -1;
				break;
			}
		}
		if (bustype == -1) {
			bustype = pte->pg_type;
		} else if (cknonobmem) {
			if (bustype != pte->pg_type) {
				/*
				 * Our type shifted....
				 */
				bustype = -1;
				break;
			}
		}
		addr += MMU_PAGESIZE;
		pte++;
	}
	return (bustype);
}

/*
 * Map the data referred to by the buffer bp into the kernel
 * at kernel virtual address kaddr.
 */
#define	PTECHUNKSIZE	16

static void
bp_map(register struct buf *bp, caddr_t kaddr)
{
	struct pte ptecache[PTECHUNKSIZE];
	struct pte *ptep = &ptecache[0];
	struct page *pp = NULL;
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
	 * In 5.x DVMA is handled in rootnex.c
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
				    ptecache, 0) < 0) {
					cmn_err(CE_PANIC,
						"bp_map: failed");
					/*NOTREACHED*/
				}
				addr = (caddr_t)((u_long)addr +
					mmu_ptob(np));
				cidx = 0;
			}
			ptep = &ptecache[cidx++];
			pfnum = MAKE_PFNUM(ptep);
		} else {
			pfnum = page_pptonum(*pplist);
			pplist++;
		}

		segkmem_mapin(&kvseg, kaddr, MMU_PAGESIZE,
		    PROT_READ | PROT_WRITE | HAT_NOSYNC, pfnum, 0);

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
			struct page *pp = bp->b_pages;

			hat_mlist_enter(pp);
			if (((machpage_t *)pp)->p_mapping != NULL) {
				struct hment *hme;

				hme = (struct hment *)
				    ((machpage_t *)pp)->p_mapping;
				align = hme->hme_impl &
					vac_mask >> MMU_PAGESHIFT;
			}
			hat_mlist_exit(pp);
		} else if (bp->b_un.b_addr != NULL) {
			align = mmu_btop((int)bp->b_un.b_addr & vac_mask);
		}
	}

	if (align == -1)
		return (rmalloc_locked(map, (size_t)size));

	/*
	 * Look for a map segment containing a request that works.
	 * If none found, return failure.
	 */
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

	addr = rmget(map, (size_t)size, (ulong_t)addr);
	return (addr);
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
		hat_unload(kas.a_hat, addr, (u_int)mmu_ptob(npf), HAT_UNLOAD);
		a = mmu_btop(addr - Sysbase);
		bzero((caddr_t)&Usrptmap[a], (u_int)(sizeof (Usrptmap[0])*npf));
		rmfree(kernelmap, (size_t)npf, a);
		bp->b_flags &= ~B_REMAPPED;
	}
}
