/*
 * Copyright (c) 1990-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bp_map.c	1.30	96/06/27 SMI"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/map.h>
#include <sys/mman.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/machparam.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <vm/as.h>
#include <vm/seg_kmem.h>
#include <vm/page.h>
#include <vm/mach_page.h>
#include <vm/hat_sfmmu.h>
#include <sys/cpu_module.h>

extern	u_int	shm_alignment;	/* VAC address consistency modulus */

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
 * Map the data referred to by the buffer bp into the kernel
 * at kernel virtual address kaddr.
 */
static void
bp_map(struct buf *bp, caddr_t kaddr)
{
	page_t *pp = NULL;
	int npf;
	struct as *as;
	unsigned int pfnum;
	caddr_t addr;
	int needaslock = 0;
	page_t **pplist = NULL;

	if (bp->b_flags & B_PAGEIO) {
		pp = bp->b_pages;
	} else if (bp->b_flags & B_SHADOW) {
		pplist = bp->b_shadow;
	} else {
		if (bp->b_proc == NULL || (as = bp->b_proc->p_as) == NULL) {
			as = &kas;
		}
		needaslock = 1;
		addr = (caddr_t)bp->b_un.b_addr;
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

	if (needaslock) {
		/*
		 * We need to grab the as lock because hat_getpfnum expects
		 * it to be held.
		 */
		ASSERT(as);
		AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
	}
	while (npf > 0) {
		if (pp) {
			pfnum = ((machpage_t *)pp)->p_pagenum;
			pp = pp->p_next;
		} else if (pplist == NULL) {
			pfnum = hat_getpfnum(as->a_hat, addr);
			addr += mmu_ptob(1);
		} else {
			pfnum = ((machpage_t *)*pplist)->p_pagenum;
			pplist++;
		}

		segkmem_mapin(&kvseg, kaddr, MMU_PAGESIZE,
		    PROT_READ | PROT_WRITE | HAT_NOSYNC, pfnum, HAT_LOAD);

		kaddr += MMU_PAGESIZE;
		npf--;
	}
	if (needaslock) {
		AS_LOCK_EXIT(as, &as->a_lock);
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
	register struct map *mp;
	register u_long addr, mask;
	int vcolor = -1;
	page_t *pp = NULL;
	extern int sfmmu_get_addrvcolor(caddr_t);

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
			vcolor = sfmmu_get_ppvcolor(PP2MACHPP(pp));
		} else if (bp->b_un.b_addr != NULL) {
			vcolor = sfmmu_get_addrvcolor(bp->b_un.b_addr);
		}
	}

	if (vcolor == -1)
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
		addr = (mp->m_addr & ~mask) + vcolor;
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
bp_mapin(bp)
	register struct buf *bp;
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
bp_mapout(bp)
	register struct buf *bp;
{
	int npf;
	u_long a;
	caddr_t	addr;

	if (bp->b_flags & B_REMAPPED) {
		addr = (caddr_t)((int)bp->b_un.b_addr & PAGEMASK);
		bp->b_un.b_addr = (caddr_t)((int)bp->b_un.b_addr & PAGEOFFSET);
		npf = mmu_btopr(bp->b_bcount + (int)bp->b_un.b_addr);

		flush_instr_mem(addr, mmu_ptob(npf));

		hat_unload(kas.a_hat, addr, (u_int)mmu_ptob(npf),
			(HAT_UNLOAD_NOSYNC | HAT_UNLOAD_UNLOCK));
		a = mmu_btop(addr - Sysbase);
		rmfree(kernelmap, (size_t)npf, a);
		bp->b_flags &= ~B_REMAPPED;
	}
}
