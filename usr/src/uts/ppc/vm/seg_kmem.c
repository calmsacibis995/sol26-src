/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986, 1987, 1988, 1989, 1990, 1993, 1995  Sun Microsystems, Inc
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 *
 */

#pragma ident	"@(#)seg_kmem.c	1.83	96/08/08 SMI"

/*
 * VM - kernel segment routines
 */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/vm.h>
#include <sys/user.h>
#include <sys/kmem.h>
#include <sys/mman.h>
#include <sys/vtrace.h>
#include <sys/map.h>
#include <sys/tuneable.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

#include <sys/mmu.h>
#include <sys/pte.h>

#include <vm/seg_kmem.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/anon.h>
#include <vm/rm.h>
#include <vm/page.h>
#include <vm/faultcode.h>
#include <vm/hat_ppcmmu.h>

#include <sys/promif.h>
#include <sys/obpdefs.h>
#include <sys/bootconf.h>
#include <sys/machsystm.h>

/*
 * Private seg op routines.
 */
#if defined(__STDC__)

static int	segkmem_fault(struct hat *, struct seg *, caddr_t, u_int,
		    enum fault_type, enum seg_rw);
static int	segkmem_faulta(struct seg *, caddr_t);
static int	segkmem_setprot(struct seg *, caddr_t, u_int, u_int);
static int	segkmem_checkprot(struct seg *, caddr_t, u_int, u_int);
static int	segkmem_getprot(struct seg *, caddr_t, u_int, u_int *);
static u_offset_t	segkmem_getoffset(struct seg *, caddr_t);
static int	segkmem_gettype(struct seg *, caddr_t);
static int	segkmem_getvp(struct seg *, caddr_t, struct vnode **);
static void	segkmem_dump(struct seg *);
static int	segkmem_pagelock(struct seg *, caddr_t, u_int,
		    struct page ***, enum lock_type, enum seg_rw);
static int	segkmem_badop();
static u_int	getprot(struct spte *);
static void 	boot_getpages(caddr_t, u_int, u_int);
static int	segkmem_getmemid(struct seg *, caddr_t, memid_t *);

/*
void 	boot_alloc(caddr_t, u_int, u_int);
*/

#else

static int	segkmem_fault(/* hat, seg, addr, len, type, rw */);
static int	segkmem_faulta(/* seg, addr */);
static int	segkmem_setprot(/* seg, addr, len, prot */);
static int	segkmem_checkprot(/* seg, addr, len, prot */);
static int	segkmem_getprot(/* seg, addr, len, protv */);
static off_t	segkmem_getoffset(/* seg, caddr_t */);
static int	segkmem_gettype(/* seg, addr */);
static int	segkmem_getvp(/* seg, addr, vpp */);
static void	segkmem_dump(/* struct seg * */);
static int	segkmem_pagelock(/* seg, addr, len, page, type, rw */);
static int	segkmem_badop();
static u_int	getprot(/* struct spte * */);
static void	boot_getpages(/* base, size, align */);
static int	segkmem_getmemid(/*seg , caddr_t, memid_t */);
/*
void	boot_alloc(/* base, size, align */);
*/

#endif /* __STDC__ */

struct as kas;

extern struct bootops *bootops;

/*
 * Machine specific public segments.
 */
struct seg kvseg;

int segkmem_ready = 0;

/*
 * All kmem alloc'ed kernel pages are associated
 * with a special kernel vnode.
 */
struct vnode kvp;

struct	seg_ops segkmem_ops = {
	segkmem_badop,			/* dup */
	segkmem_badop,			/* unmap */
	(void(*)())segkmem_badop,	/* free */
	segkmem_fault,
	segkmem_faulta,
	segkmem_setprot,
	segkmem_checkprot,
	segkmem_badop,			/* kluster */
	(u_int (*)())segkmem_badop,	/* swapout */
	segkmem_badop,			/* sync */
	segkmem_badop,			/* incore */
	segkmem_badop,			/* lockop */
	segkmem_getprot,
	segkmem_getoffset,
	segkmem_gettype,
	segkmem_getvp,
	segkmem_badop,			/* advise */
	segkmem_dump,
	segkmem_pagelock,
	segkmem_getmemid,
};

/*
 * The segkmem driver will (optional) use an array of spte's to back
 * up the mappings for compatibility reasons.  This driver treates
 * argsp as a pointer to the spte array to be used for the segment.
 */
int
segkmem_create(struct seg *seg, void *argsp)
{
	ASSERT(seg->s_as == &kas && RW_WRITE_HELD(&seg->s_as->a_lock));
	seg->s_ops = &segkmem_ops;
	seg->s_data = (void *)argsp; /* actually a struct spte array */
	kas.a_size += seg->s_size;
	return (0);
}

/*ARGSUSED*/
static faultcode_t
segkmem_fault(hat, seg, addr, len, type, rw)
	struct hat *hat;
	struct seg *seg;
	caddr_t addr;
	u_int len;
	enum fault_type type;
	enum seg_rw rw;
{
	register faultcode_t retval;

	ASSERT(seg->s_as && RW_READ_HELD(&seg->s_as->a_lock));

	/*
	 * For now the only `fault' supported by this driver are
	 * F_SOFTLOCK/F_SOFTUNLOCK for the S_READ/S_WRITE case (caused
	 * during physio by RFS servers).
	 *
	 * These types of faults are used to denote "lock down already
	 * loaded translations".
	 */
	switch (type) {
	case F_SOFTLOCK:
		if (rw == S_READ || rw == S_WRITE || rw == S_OTHER) {
			retval = 0;
		} else
			retval = FC_NOSUPPORT;
		break;
	case F_SOFTUNLOCK:
		if (rw == S_READ || rw == S_WRITE)
			retval = 0;
		else
			retval = FC_NOSUPPORT;
		break;
	default:
		retval = FC_NOSUPPORT;
		break;
	}
	return (retval);
}

/*ARGSUSED*/
static faultcode_t
segkmem_faulta(seg, addr)
	struct seg *seg;
	caddr_t addr;
{
	return (FC_NOSUPPORT);
}

/*
 * XXX - Routines which obtain information directly from the
 * MMU should acquire the hat layer lock.
 */

static int
segkmem_setprot(seg, addr, len, prot)
	struct seg *seg;
	caddr_t addr;
	u_int len, prot;
{
	register struct spte *seg_pte;
	register caddr_t eaddr;
	register int pprot;

	ASSERT(seg->s_as == &kas);
	ASSERT(len <= seg->s_size);
	ASSERT(addr <= addr + len);
	ASSERT(addr >= seg->s_base && addr + len <= seg->s_base + seg->s_size);
	ASSERT(seg->s_ops == &segkmem_ops);
	ASSERT(AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	if (addr < seg->s_base || (addr + len) > (seg->s_base + seg->s_size))
		cmn_err(CE_PANIC, "segkmem_setprot -- out of segment");

	if (seg->s_data) {
		seg_pte = (struct spte *)seg->s_data;
		seg_pte += mmu_btop(addr - seg->s_base);
	} else
		seg_pte = NULL;

	if (prot == 0)
		hat_unload(seg->s_as->a_hat, addr, len, HAT_UNLOAD);
	else
		hat_chgprot(seg->s_as->a_hat, addr, len, prot);

	if (seg_pte == NULL)
		return (0);

	pprot = ppcmmu_vtop_prot(addr, prot);

	hat_enter(kas.a_hat);
	for (eaddr = addr + len; addr < eaddr; addr += PAGESIZE) {
		if (prot == 0) {
			*(u_int *)seg_pte = MMU_INVALID_SPTE;
			continue;
		}
		if (spte_valid(seg_pte))
			seg_pte->spte_pp = pprot;
		seg_pte++;
	}
	hat_exit(kas.a_hat);
	return (0);
}

static u_int
getprot(register struct spte *ppte)
{
	register u_int pprot;

	if (!spte_valid(ppte)) {
		pprot = 0;
	} else {

		switch (ppte->spte_pp) {
		case MMU_STD_SRXURX:
			pprot = PROT_READ | PROT_EXEC | PROT_USER;
			break;
		case MMU_STD_SRWXURX:
			pprot = PROT_READ | PROT_WRITE | PROT_EXEC | PROT_USER;
			break;
		case MMU_STD_SRWXURWX:
			pprot = PROT_READ | PROT_WRITE | PROT_EXEC | PROT_USER;
			break;
		case MMU_STD_SRWX:
			pprot = PROT_READ | PROT_WRITE | PROT_EXEC;
			break;
		default:
			pprot = 0;
			break;
		}
	}
	return (pprot);
}

static int
segkmem_checkprot(seg, addr, len, prot)
	register struct seg *seg;
	register caddr_t addr;
	u_int len;
	register u_int prot;
{
	register struct spte *spte;
	register hwpte_t *ppte;
	register u_int pprot;
	caddr_t eaddr;

	ASSERT(seg->s_as == &kas);
	ASSERT(len <= seg->s_size);
	ASSERT(addr <= addr + len);
	ASSERT(addr >= seg->s_base && addr + len <= seg->s_base + seg->s_size);
	ASSERT(seg->s_ops == &segkmem_ops);
	ASSERT(AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/* DEBUG */
	if (addr < seg->s_base || (addr + len) > (seg->s_base + seg->s_size))
		cmn_err(CE_PANIC, "segkmem_checkprot");
	/* End DEBUG */


	if (seg->s_data) {
		spte = (struct spte *)seg->s_data;
		spte += mmu_btop(addr - seg->s_base);
	} else
		spte = NULL;

	hat_enter(kas.a_hat);
	for (eaddr = addr + len; addr < eaddr; addr += PAGESIZE) {
		if (spte == NULL) {
			ppte = ppcmmu_ptefind(seg->s_as, addr, PTEGP_NOLOCK);
			if (ppte == NULL || !hwpte_valid(ppte))
				pprot = 0;
			else
				pprot = ppcmmu_ptov_prot(ppte);
		} else {
			if (spte_valid(spte))
				pprot = spte->spte_pp;
			else
				pprot = 0;
			spte++;
		}
		if ((pprot & prot) != prot) {
			hat_exit(kas.a_hat);
			return (EACCES);
		}

	}
	hat_exit(kas.a_hat);
	return (0);
}

static int
segkmem_getprot(seg, addr, len, protv)
	register struct seg *seg;
	register caddr_t addr;
	register u_int len, *protv;
{
	u_int pgno = seg_page(seg, addr + len) - seg_page(seg, addr) + 1;
	register struct spte *ppte;
	register i;
	register hwpte_t *pte;

	ASSERT(seg->s_as == &kas);

	hat_enter(kas.a_hat);
	if (seg->s_data != NULL) {
		ppte = (struct spte *)seg->s_data;
		ppte += mmu_btop(addr - seg->s_base);
	} else
		ppte = (struct spte *)NULL;

	for (i = 0; i < pgno; i++) {
		if (ppte != NULL) {
			protv[i] = getprot(ppte);
			ppte++;
		} else {
			pte = ppcmmu_ptefind(&kas, addr, PTEGP_NOLOCK);
			addr += MMU_PAGESIZE;
			if (pte)
				protv[i] = ppcmmu_ptov_prot(pte);
			else
				protv[i] = 0;
		}
	}
	hat_exit(kas.a_hat);
	return (0);
}

/*ARGSUSED*/
static u_offset_t
segkmem_getoffset(seg, addr)
	struct seg *seg;
	caddr_t addr;
{
	return ((u_offset_t)0);
}

/*ARGSUSED*/
static int
segkmem_gettype(seg, addr)
	struct seg *seg;
	caddr_t addr;
{
	return (MAP_SHARED);
}

/*ARGSUSED*/
static int
segkmem_getvp(seg, addr, vpp)
	register struct seg *seg;
	caddr_t addr;
	struct vnode **vpp;
{
	*vpp = NULL;
	return (-1);
}

static int
segkmem_badop()
{

	cmn_err(CE_PANIC, "segkmem_badop");
	/*NOTREACHED*/
	return (0);	/* Eliminate warnings. */
}

/*
 * Special public segkmem routines.
 */

/*
 * Allocate physical pages for the given kernel virtual address.
 * Performs most of the work of the old memall/vmaccess combination.
 */
int
segkmem_alloc(seg, addr, len, canwait)
	struct seg *seg;
	caddr_t addr;
	u_int len;
	int canwait;
{
	page_t *pp;
	struct spte *spte;
	struct pte tpte;
	register int val = 0;		/* assume failure */
	struct as *as = seg->s_as;

	ASSERT(as == &kas);
	ASSERT(len <= seg->s_size);
	ASSERT(addr <= addr + len);
	ASSERT(addr >= seg->s_base && addr + len <= seg->s_base + seg->s_size);
	ASSERT(seg->s_ops == &segkmem_ops);
	ASSERT(((u_int)addr & PAGEOFFSET) == 0);

	pp = page_create_va(&kvp, (u_offset_t)(u_int)addr, len,
			PG_EXCL | ((canwait) ? PG_WAIT : 0), &kas, addr);
	if (pp != (page_t *)NULL) {
		page_t *ppcur;

		TRACE_5(TR_FAC_VM, TR_SEGKMEM_ALLOC,
			"segkmem_alloc:seg %x addr %x pp %x kvp %x off %x",
			seg, addr, pp, &kvp, (u_long) addr & (PAGESIZE - 1));
		if (seg->s_data) {
			spte = (struct spte *)seg->s_data;
		} else {
			spte = (struct spte *)NULL;
		}

		while (pp != (page_t *)NULL) {
			ppcur = pp;
			page_sub(&pp, ppcur);
			ASSERT(se_assert(&ppcur->p_selock) &&
			    page_iolock_assert(ppcur));
			page_io_unlock(ppcur);
			page_downgrade(ppcur);

			ppcmmu_memload(as->a_hat, as, (caddr_t)ppcur->p_offset,
				ppcur, PROT_ALL & ~PROT_USER, HAT_LOAD_LOCK);
			if (spte != NULL) {
				ppcmmu_mempte(as->a_hat, ppcur,
					PROT_ALL & ~PROT_USER,
					&tpte, (caddr_t)ppcur->p_offset);
				pte_to_spte(&tpte, spte +
					(mmu_btop((caddr_t)ppcur->p_offset -
					seg->s_base)));
			}
		}
		val = 1;		/* success */
	}
	return (val);
}

/*ARGSUSED*/
void
segkmem_free(seg, addr, len)
	register struct seg *seg;
	caddr_t addr;
	u_int len;
{
	page_t *pp;
	struct spte *spte;
	struct as *as = seg->s_as;

	ASSERT(as == &kas);
	ASSERT(len <= seg->s_size);
	ASSERT(addr <= addr + len);
	ASSERT(addr >= seg->s_base && addr + len <= seg->s_base + seg->s_size);
	ASSERT(seg->s_ops == &segkmem_ops);
	ASSERT(((u_int)addr & PAGEOFFSET) == 0);

	if (seg->s_data) {
		spte = (struct spte *)seg->s_data;
		spte += mmu_btop(addr - seg->s_base);
	} else
		spte = NULL;

	for (; (int)len > 0; len -= PAGESIZE, addr += PAGESIZE) {
		/*
		 * Use page_find() instead of page_lookup() to
		 * find the page since we know that it is hashed
		 * and has an "exclusive" lock.
		 */
		pp = page_find(&kvp, (u_offset_t)(u_int)addr);
		if (pp == NULL)
			cmn_err(CE_PANIC, "segkmem_free");

		if (! page_tryupgrade(pp)) {
			/*
			 * Other thread has it locked shared too. Most
			 * likely is some /dev/mem reader threads.
			 */
			page_unlock(pp);

			/* We should be the only one to free this page! */
			pp = page_lookup(&kvp, (off_t)addr, SE_EXCL);
			if (pp == NULL) {
				cmn_err(CE_PANIC, "segkmem_free: page freed");
			}
		}

		hat_unlock(as->a_hat, addr, PAGESIZE);
		hat_unload(as->a_hat, addr, PAGESIZE, HAT_UNLOAD);
		/*
		 * Destroy identity of the page and put it
		 * back on the free list.
		 */
		/*LINTED: constant in conditional context*/
		VN_DISPOSE(pp, B_INVAL, 0, kcred);
		if (spte) {
			*(u_int *)spte = MMU_INVALID_SPTE;
			spte++;
		}
	}
}

/*
 * segkmem_mapin() and segkmem_mapout() are for manipulating kernel
 * addresses only. Since some users of segkmem_mapin() forget to unmap,
 * this is done implicitly.
 * NOTE: addr and len must always be multiples of the mmu page size.
 * Also, this routine cannot be used to set invalid translations.
 */
void
segkmem_mapin(seg, addr, len, vprot, pcookie, flags)
	struct seg *seg;
	register caddr_t addr;
	register u_int len;
	u_int vprot;
	u_int pcookie;	/* Actually the physical page number */
	int flags;
{
	register struct spte *spte;
	struct pte apte;
	register hwpte_t *hwpte;
	register page_t *pp;

	ASSERT(seg->s_as == &kas);
	ASSERT(len <= seg->s_size);
	ASSERT(addr <= addr + len);
	ASSERT(addr >= seg->s_base && addr + len <= seg->s_base + seg->s_size);
	ASSERT(seg->s_ops == &segkmem_ops);
	ASSERT(((u_int)addr & MMU_PAGEOFFSET) == 0);

	if (seg->s_data) {
		spte = (struct spte *)seg->s_data;
		spte += mmu_btop(addr - seg->s_base);
	} else
		spte = (struct spte *)NULL;
#if defined(_NO_LONGLONG)
	*(u_long *)&apte = MMU_INVALID_PTE;
	*((u_long *)&apte+1) = MMU_INVALID_PTE;
#else
	*(u_longlong_t *)&apte = MMU_INVALID_PTE;
#endif
	apte.pte_ppn = pcookie;
	if (vprot == PROT_NONE)
		cmn_err(CE_PANIC, "segkmem_mapin -- invalid ptes");
	apte.pte_pp = ppcmmu_vtop_prot(addr, (vprot & HAT_PROT_MASK));
	apte.pte_valid = PTE_VALID;

	/*
	 * Always lock the mapin'd translations.
	 */
	flags |= HAT_LOAD_LOCK;

	for (; len != 0; addr += MMU_PAGESIZE, len -= MMU_PAGESIZE) {
		if (spte == NULL) {
			hat_enter(kas.a_hat);
			hwpte =  ppcmmu_ptefind(&kas, addr, PTEGP_NOLOCK);
			hat_exit(kas.a_hat);
			if (hwpte != NULL) {
				hat_unlock(kas.a_hat, addr, PAGESIZE);
				hat_unload(kas.a_hat, addr, MMU_PAGESIZE,
								HAT_UNLOAD);
			}
		} else {
			if (spte_valid(spte)) {
				hat_unlock(kas.a_hat, addr, PAGESIZE);
				hat_unload(kas.a_hat, addr, MMU_PAGESIZE,
								HAT_UNLOAD);
			}
		}

		/*
		 * Determine the page frame we're mapping to allow
		 * the translation layer to manage cache consistency.
		 * If we're replacing a valid mapping, then ask the
		 * translation layer to unload the mapping and update
		 * data structures (necessary to maintain the information
		 * for cache consistency).  We use page_numtopp_nowait here
		 * instead of page_numtopp to avoid a potential deadlock if
		 * the page is "exclusively locked".
		 */
		if (pf_is_memory(apte.pte_ppn) &&
		    !(flags & HAT_LOAD_NOCONSIST)) {
			apte.pte_wimg = WIMG(1, 0);
			pp = page_numtopp_nolock(apte.pte_ppn);
			ASSERT(pp == NULL || se_assert(&pp->p_selock) ||
			    panicstr);
		} else {
			pp = (page_t *)NULL;
			apte.pte_wimg = WIMG(0, !mmu601);
		}

		ppcmmu_devload(kas.a_hat, &kas, addr, (devpage_t *)pp,
			apte.pte_ppn, vprot, flags);


		if (spte != NULL) {
			pte_to_spte(&apte, spte);
			spte++;
		}
		apte.pte_ppn++;
	}
}

/*
 * Release mapping for the kernel. This frees pmegs, which are a critical
 * resource. The segment must be backed by software ptes. The pages
 * specified are only freed if they are valid. This allows callers to
 * clear out virtual space without knowing if it's mapped or not.
 * NOTE: addr and len must always be multiples of the page size.
 */
void
segkmem_mapout(seg, addr, len)
	struct seg *seg;
	register caddr_t addr;
	register u_int len;
{
	register struct spte *spte;

	ASSERT(len <= seg->s_size);
	ASSERT(addr <= addr + len);
	ASSERT(addr >= seg->s_base && addr + len <= seg->s_base + seg->s_size);
	ASSERT(seg->s_ops == &segkmem_ops);

	/*
	 * XXX - What happens if MMU_PAGESIZE is not the
	 * same as PAGESIZE?
	 */
	ASSERT(((u_int)addr & MMU_PAGEOFFSET) == 0);

	if (seg->s_as != &kas)
		cmn_err(CE_PANIC, "segkmem_mapout: bad as");

	if (seg->s_data != NULL) {
		spte = (struct spte *)seg->s_data;
		spte += mmu_btop(addr - seg->s_base);
	} else
		spte = (struct spte *)NULL;
	for (; len != 0; addr += MMU_PAGESIZE, len -= MMU_PAGESIZE) {
		if (spte) {
			if (!spte_valid(spte)) {
				spte++;
				continue;
			}
			*(u_int *)spte = MMU_INVALID_SPTE;
			spte++;
		} else {
			hat_enter(kas.a_hat);
			if (ppcmmu_ptefind(&kas, addr, PTEGP_NOLOCK) == NULL) {
				hat_exit(kas.a_hat);
				continue;
			}
			hat_exit(kas.a_hat);
		}
		hat_unlock(kas.a_hat, addr, PAGESIZE);
		hat_unload(kas.a_hat, addr, MMU_PAGESIZE, HAT_UNLOAD);
	}
}

/*
 * Dump the pages belonging to this segkmem segment.
 */
void
segkmem_dump(struct seg *seg)
{
	caddr_t  addr, eaddr;
	u_int   pfn;
	extern void dump_addpage(u_int);

	eaddr = seg->s_base + seg->s_size;
	for (addr = seg->s_base; addr < eaddr; addr += PAGESIZE) {
		pfn = va_to_pfn(addr);
		if (pfn != (u_int)-1 && pfn <= physmax && pf_is_memory(pfn))
			dump_addpage(pfn);
	}

}

/*ARGSUSED*/
static int
segkmem_pagelock(struct seg *seg, caddr_t addr, u_int len,
    struct page ***ppp, enum lock_type type, enum seg_rw rw)
{
	return (ENOTSUP);
}

/*
 * Allocate "npages" (MMU) pages worth of system virtual address space, and
 * allocate wired-down page frames to back them.
 * If "flag" is KM_NOSLEEP, block until address space and page frames are
 * available.
 *
 * XXX - should a distinction be drawn between "rmalloc" failing and
 * "segkmem_alloc" failing?
 */
void *
kmem_getpages(npages, flag)
	int npages, flag;
{
	register u_long a;
	caddr_t base;

	TRACE_3(TR_FAC_KMEM, TR_KMEM_GETPAGES_START,
		"kmem_getpages_start:caller %K npages %d flag %x",
		caller(), npages, flag);

	/*
	 * Make sure that the number of pages requested isn't bigger
	 * than segkmem itself.
	 */
	if (segkmem_ready && (npages > (int)mmu_btop(kvseg.s_size)))
		cmn_err(CE_PANIC, "kmem_getpages: request too big");

	/*
	 * Allocate kernel virtual address space.
	 */
	if (flag & KM_NOSLEEP) {
		if ((a = rmalloc(kernelmap, (long)npages)) == 0) {
			TRACE_1(TR_FAC_KMEM, TR_KMEM_GETPAGES_END,
				"kmem_getpages_end:kmem addr %x", NULL);
			return (NULL);
		}
	} else {
		a = rmalloc_wait(kernelmap, (long)npages);
	}

	base = kmxtob(a);

	/*
	 * Allocate physical pages to back the address allocated.
	 */
	if (segkmem_ready) {
		if (segkmem_alloc(&kvseg, (caddr_t)base,
		    (u_int)mmu_ptob(npages),
		    !(flag & KM_NOSLEEP)) == 0) {
			rmfree(kernelmap, (long)npages, a);
			TRACE_1(TR_FAC_KMEM, TR_KMEM_GETPAGES_END,
				"kmem_getpages_end:kmem addr %x", NULL);
			return (NULL);
		}
	} else
		boot_getpages(base, mmu_ptob(npages), BO_NO_ALIGN);

	TRACE_1(TR_FAC_KMEM, TR_KMEM_GETPAGES_END,
		"kmem_getpages_end:kmem addr %x", base);
	return ((void *)base);
}


/*
 * This is the memlist we'll keep around until segkmem is ready
 * and then we'll come through and free it.
 */
static struct memlist *kmem_garbage_list;

/*
 * Free "npages" (MMU) pages gotten with "kmem_getpages".
 */
void
kmem_freepages(addr, npages)
#ifdef lint
	caddr_t addr;
#else
	void *addr;
#endif
	int npages;
{
	TRACE_3(TR_FAC_KMEM, TR_KMEM_FREEPAGES_START,
		"kmem_freepages_start:caller %K addr %x npages %d",
		caller(), addr, npages);

	if (!segkmem_ready) {
		struct memlist *this;
		/*
		 * If someone attempts to free memory here we must delay
		 * the action until kvseg is ready.  So lets just put
		 * the request on a list and delete it at a later time.
		 */
		this = (struct memlist *)
		    kmem_zalloc(sizeof (struct memlist), KM_NOSLEEP);
		if (!this) {
			cmn_err(CE_PANIC,
			    "can't allocate kmem gc list element");
		}
		this->address = (u_int)addr;
		this->size = (u_int)ctob(npages);
		this->next = kmem_garbage_list;
		kmem_garbage_list = this;
	} else {

		/*
		 * Free the physical memory behind the pages.
		 */
		segkmem_free(&kvseg, (caddr_t)addr, (u_int)mmu_ptob(npages));

		/*
		 * Free the virtual addresses behind which they resided.
		 */
		rmfree(kernelmap, (long)npages, (u_long)btokmx((caddr_t)addr));
	}
	TRACE_0(TR_FAC_KMEM, TR_KMEM_FREEPAGES_END, "kmem_freepages_end");
}

/*
 * Collect and free all the unfreed garbage on the delayed free list
 * Called at the end of startup().
 */
void
kmem_gc(void)
{
	struct memlist	*this, *next;

	ASSERT(segkmem_ready);

	for (this = kmem_garbage_list; this; this = next) {
		kmem_freepages((void *)this->address,
			(int)mmu_btopr(this->size));
		next = this->next;
		kmem_free(this, sizeof (struct memlist));
	}

	kmem_garbage_list = (struct memlist *)0;
}

/*
 * Return the total amount of virtual space available for kmem_alloc
 */
u_long
kmem_maxvirt(void)
{
	struct map *bp;
	u_long	amount = 0;

	mutex_enter(&maplock(kernelmap));
	for (bp = mapstart(kernelmap); bp->m_size; bp++)
		amount += bp->m_size;
	mutex_exit(&maplock(kernelmap));
	return (amount);
}

/*
 * Memory allocator functions for allocating memory from ktextseg segment.
 * This allocator is used ONLY by kobj for allocating text/data sections
 * of kernel modules. Kobj uses regular kmem_alloc for other allocations
 * (including bss sections) when a module is loaded.
 *
 * lokmem_init(base, size)
 *	Initialize the free list and any global variables. The base is the
 *	base address of the chunk within the ktextseg segment that
 *	can be used for allocations. And the 'size' is the size of the chunk.
 *
 * lokmem_zalloc(size, flags)
 *	Allocate 'size' bytes from ktextseg segment. 'flags' indicate if
 *	the request can wait (i.e KM_SLEEP and KM_NOSLEEP).
 *	This function uses segkmem_alloc() to map page(s) from the
 *	segment if segkmem is ready. Otherwise it uses boot_getpages()
 *	to get physcial pages. It aligns the 'size' to 16byte boundary.
 *
 * lokmem_free(addr, size)
 *	Free 'size' bytes from ktextseg segment. It updates the freelist
 *	and calls segkmem_free() (only if segkmem is ready) to free up the
 *	physical pages and the associated mappings. The freelist is sorted on
 *	kernel virtual address.
 *
 * lokmem_gc()
 *	This is called to free up any unused pages in ktextseg segment that
 *	have valid mappings. It calls segkmem_free() to free up the
 *	mappings and the associated physcial pages for any of the ununsed
 *	pages in the freelist.
 */

typedef struct lokmem_buf {
	caddr_t		lokmem_buf_addr;    /* buffer address */
	size_t		lokmem_buf_size;    /* size in bytes */
	struct lokmem_buf *lokmem_buf_next; /* next buffer in the list */
	struct lokmem_buf *lokmem_buf_prev; /* previous buffer in the list */
} lokmem_buf_t;

/* mutex to protect lokmem_buf_usedlist and lokmem_buf_freelist */
kmutex_t lokmem_lock;

lokmem_buf_t *lokmem_buf_usedlist; /* list of used buffers */
lokmem_buf_t *lokmem_buf_freelist; /* list of free buffers */
size_t lokmem_max_size;
size_t lokmem_avail;
caddr_t lokmem_base; /* base address for the lokmem chunk in ktextseg */
u_int lokmem_wanted;
kcondvar_t lokmem_cv;

#define	LOKMEM_ALIGN	16		/* alignment for the buffers */

#ifdef LOKMEM_DEBUG
#define	LOKMEM_ALLOC_DEBUG	1
#define	LOKMEM_FREE_DEBUG	2
#define	LOKMEM_FREELIST_DUMP	4
#define	LOKMEM_USEDLIST_DUMP	8
int lokmem_debug = 0;
void lokmem_listdump();
#endif

/*
 * lokmem_bimap[] - Each bit specifies if the corresponding page has a mapping.
 */
u_int *lokmem_bitmap;	/* bitmap for active mappings */

#define	NBINT	(sizeof (u_int) * NBBY)	/* no. of bits per u_int */
#define	SET_LOKMEM_MAP(page)\
		(lokmem_bitmap[(page) / NBINT] |= (1 << ((page) % NBINT)))
#define	CLEAR_LOKMEM_MAP(page)\
		(lokmem_bitmap[(page) / NBINT] &= ~(1 << ((page) % NBINT)))
#define	IS_LOKMEM_MAPPED(page)\
		(lokmem_bitmap[(page) / NBINT] & (1 << ((page) % NBINT)))

void
lokmem_init(caddr_t base, int size)
{
	int i;

	mutex_init(&lokmem_lock, "lokmem_lock", MUTEX_DEFAULT, DEFAULT_WT);
	cv_init(&lokmem_cv, "lokmem_cv", CV_DEFAULT, NULL);

	/* align the base address to the page boundary */
	lokmem_base = (caddr_t)((u_int)(base + PAGEOFFSET) & PAGEMASK);

	/* adjust lokmem chunk size to multiple pages */
	lokmem_max_size = ((u_int)(base + size) & PAGEMASK) -
				(u_int)lokmem_base;

	/* initialize the free list */
	lokmem_buf_freelist =
		(lokmem_buf_t *)kmem_alloc(sizeof (struct lokmem_buf), 0);
	lokmem_buf_freelist->lokmem_buf_addr = lokmem_base;
	lokmem_buf_freelist->lokmem_buf_next = NULL;
	lokmem_buf_freelist->lokmem_buf_prev = NULL;
	lokmem_avail = lokmem_buf_freelist->lokmem_buf_size = lokmem_max_size;

	/* allocate the bit map for lokmem-page-inuse */
	i = (mmu_btop(lokmem_max_size) + (NBINT - 1)) / NBINT;
	if ((lokmem_bitmap = (u_int *)
		kmem_zalloc(i * NBPW, KM_NOSLEEP)) == NULL)
		cmn_err(CE_PANIC, "Cannot allocate memory for lokmem_bitmap");
}

void *
lokmem_zalloc(size_t size, int flags)
{
	register lokmem_buf_t *lp;
	register lokmem_buf_t *tp;
	u_int last_page;
	u_int page_addr;

	size = (size + LOKMEM_ALIGN - 1) & ~(LOKMEM_ALIGN - 1);
	ASSERT(size <= lokmem_max_size);

#ifdef LOKMEM_DEBUG
	if (lokmem_debug & LOKMEM_ALLOC_DEBUG)
		prom_printf("lokmem_zalloc: size %x wanted\n", size);
	if (lokmem_debug & LOKMEM_USEDLIST_DUMP) {
		prom_printf("lokmem_zalloc: USEDLIST DUMP (before)\n");
		lokmem_listdump(lokmem_buf_usedlist);
	}
#endif
	mutex_enter(&lokmem_lock);
	/*
	 * find the first free buffer in the freelist that satisfies this
	 * request.
	 */
try_again:
	lp = lokmem_buf_freelist;
	while (lp) {
		if (size <= lp->lokmem_buf_size)
			break;
		lp = lp->lokmem_buf_next;
	}
	if (lp == NULL) {
		if (flags & KM_SLEEP) {
			if (lokmem_wanted <= size)
				lokmem_wanted = size;
			cv_wait(&lokmem_cv, &lokmem_lock);
			goto try_again;
		}
		mutex_exit(&lokmem_lock);
		return (NULL);
	}
	/*
	 * Now we need to make sure that the buffer is mapped.
	 */
	page_addr = (u_int)lp->lokmem_buf_addr & PAGEMASK;
	last_page = ((u_int)lp->lokmem_buf_addr + size - 1) & PAGEMASK;

	for (; page_addr <= last_page; page_addr += MMU_PAGESIZE) {
		register int pagenum;

		pagenum = (page_addr - (u_int)lokmem_base) >> PAGESHIFT;
		if (IS_LOKMEM_MAPPED(pagenum))
			continue;	/* this page is already mapped */
		/* map this page */
		if (segkmem_ready) {
			if (segkmem_alloc(&ktextseg, (caddr_t)page_addr,
				(u_int)MMU_PAGESIZE,
				!(flags & KM_NOSLEEP)) == 0) {
				mutex_exit(&lokmem_lock);
				return (NULL);
			}
		} else {
			boot_getpages((caddr_t)page_addr, MMU_PAGESIZE,
				BO_NO_ALIGN);
		}
		SET_LOKMEM_MAP(pagenum); /* set inuse bit for this page */
	}
	if (lp->lokmem_buf_size > size) {
		/* available buffer is bigger than what we need, split it */
		tp = (lokmem_buf_t *)kmem_zalloc(sizeof (struct lokmem_buf),
			flags);
		if (tp == NULL) {
			mutex_exit(&lokmem_lock);
			return (NULL);
		}
		/* allocate the low end of the buffer */
		tp->lokmem_buf_addr = lp->lokmem_buf_addr;
		tp->lokmem_buf_size = size;
		lp->lokmem_buf_size -= size;
		lp->lokmem_buf_addr += size;
	} else {
		/* buffer size is same as what we need, just take it out */
		tp = lp;
		if (lp->lokmem_buf_prev)
			lp->lokmem_buf_prev->lokmem_buf_next =
				lp->lokmem_buf_next;
		if (lp->lokmem_buf_next)
			lp->lokmem_buf_next->lokmem_buf_prev =
				lp->lokmem_buf_prev;
		if (lp == lokmem_buf_freelist)
			lokmem_buf_freelist = lp->lokmem_buf_next;
		lp->lokmem_buf_prev = NULL;
		lp->lokmem_buf_next = NULL;
	}
	/* add the new buffer pointed by 'tp' to the usedlist */
	tp->lokmem_buf_next = lokmem_buf_usedlist;
	if (lokmem_buf_usedlist)
		lokmem_buf_usedlist->lokmem_buf_prev = tp;
	lokmem_buf_usedlist = tp;

	mutex_exit(&lokmem_lock);

	bzero(tp->lokmem_buf_addr, size);
	lokmem_avail -= size;

#ifdef LOKMEM_DEBUG
	if (lokmem_debug & LOKMEM_USEDLIST_DUMP) {
		prom_printf("lokmem_zalloc: USEDLIST DUMP (after)\n");
		lokmem_listdump(lokmem_buf_usedlist);
	}
#endif
	return ((void *)tp->lokmem_buf_addr);
}

void
lokmem_free(void *buf, size_t size)
{
	register lokmem_buf_t *lp;
	register lokmem_buf_t *tp;
	u_int first_page;
	u_int last_page;

	size = (size + LOKMEM_ALIGN - 1) & ~(LOKMEM_ALIGN - 1);
	ASSERT(size <= lokmem_max_size);

#ifdef LOKMEM_DEBUG
	if (lokmem_debug & LOKMEM_FREE_DEBUG)
		prom_printf("lokmem_free: buf %x\n", buf);
	if (lokmem_debug & LOKMEM_FREELIST_DUMP) {
		prom_printf("lokmem_free: FREELIST DUMP (before)\n");
		lokmem_listdump(lokmem_buf_freelist);
	}
#endif
	mutex_enter(&lokmem_lock);
	/*
	 * Find the buffer in the usedlist.
	 *
	 * NOTE: A simple linear search is done for now. This memory
	 * allocator is used only when modules are loaded or unloaded so the
	 * efficency of the allocator is not a big concern.
	 */
	lp = lokmem_buf_usedlist;
	while (lp) {
		if (lp->lokmem_buf_addr == (caddr_t)buf)
			break;
		lp = lp->lokmem_buf_next;
	}
	if (lp == NULL) {
		mutex_exit(&lokmem_lock);
		cmn_err(CE_PANIC, "lokmem_free: buffer 0x%x not found\n", buf);
	}

	ASSERT(lp->lokmem_buf_size == size);

	/* remove the buffer from the usedlist */
	if (lp->lokmem_buf_prev)
		lp->lokmem_buf_prev->lokmem_buf_next = lp->lokmem_buf_next;
	if (lp->lokmem_buf_next)
		lp->lokmem_buf_next->lokmem_buf_prev = lp->lokmem_buf_prev;
	if (lp == lokmem_buf_usedlist)
		lokmem_buf_usedlist = lp->lokmem_buf_next;

	/*
	 * Search the freelist for adding the new buffer to it. The
	 * scheme is to maintain the list in a sorted sequence from the
	 * low to high. Merge the adjacent contigueous buffers if any.
	 */
	if ((tp = lokmem_buf_freelist) == NULL) {
		/* freelist is NULL, make this buffer the head of the list */
		lokmem_buf_freelist = lp;
	} else {
		register lokmem_buf_t *prev, *next;

		while (tp->lokmem_buf_next) {
			if (tp->lokmem_buf_addr > (caddr_t)buf)
				break;
			tp = tp->lokmem_buf_next;
		}
		/* insert the buffer into the freelist */
		if (tp->lokmem_buf_addr > (caddr_t)buf) {
			/* insert the buffer before tp */
			lp->lokmem_buf_next = tp;
			lp->lokmem_buf_prev = tp->lokmem_buf_prev;
			if (tp->lokmem_buf_prev)
				tp->lokmem_buf_prev->lokmem_buf_next = lp;
			tp->lokmem_buf_prev = lp;
			if (tp == lokmem_buf_freelist)
				lokmem_buf_freelist = lp;
		} else {
			/* insert the buffer after tp */
			lp->lokmem_buf_next = tp->lokmem_buf_next;
			lp->lokmem_buf_prev = tp;
			if (tp->lokmem_buf_next)
				tp->lokmem_buf_next->lokmem_buf_prev = lp;
			tp->lokmem_buf_next = lp;
		}
		/*
		 * Try to coalice the adjacent buffers to lp.
		 */
		prev = lp->lokmem_buf_prev;
		next = lp->lokmem_buf_next;
		if (prev && ((prev->lokmem_buf_addr + prev->lokmem_buf_size) ==
			lp->lokmem_buf_addr)) {
			/* merge lp with previous buffer */
			lp->lokmem_buf_addr = prev->lokmem_buf_addr;
			lp->lokmem_buf_size += prev->lokmem_buf_size;
			lp->lokmem_buf_prev = prev->lokmem_buf_prev;
			if (prev->lokmem_buf_prev)
				prev->lokmem_buf_prev->lokmem_buf_next = lp;
			kmem_free((void *)prev, sizeof (struct lokmem_buf));
			if (prev == lokmem_buf_freelist)
				lokmem_buf_freelist = lp;
		}
		if (next && ((lp->lokmem_buf_addr + lp->lokmem_buf_size) ==
			next->lokmem_buf_addr)) {
			/* merge lp with next buffer */
			lp->lokmem_buf_size += next->lokmem_buf_size;
			lp->lokmem_buf_next = next->lokmem_buf_next;
			if (next->lokmem_buf_next)
				next->lokmem_buf_next->lokmem_buf_prev = lp;
			kmem_free((void *)next, sizeof (struct lokmem_buf));
		}
	}

	if (segkmem_ready) {
		/*
		 * free up any physcial pages associated with the pages in
		 * the buffer freed.
		 */
		first_page = (u_int)buf & ~MMU_PAGEOFFSET;
		/* make sure we can free the page that contains buffer start */
		if ((caddr_t)first_page < lp->lokmem_buf_addr)
			first_page += MMU_PAGESIZE;
		last_page = ((u_int)buf + size - 1) & ~MMU_PAGEOFFSET;
		/* make sure we can free the page that contains buffer end */
		if ((last_page + MMU_PAGESIZE) > ((u_int)lp->lokmem_buf_addr +
			lp->lokmem_buf_size))
			last_page -= MMU_PAGESIZE;
		if (last_page >= first_page) {
			u_int page_addr = first_page;
			int pn = mmu_btop(first_page - (u_int)lokmem_base);

			/* free up the physical pages for this buffer */
			for (; page_addr <= last_page; pn++,
				page_addr += MMU_PAGESIZE) {
				segkmem_free(&ktextseg, (caddr_t)page_addr,
					MMU_PAGESIZE);
				/* clear the inuse bit for this page */
				CLEAR_LOKMEM_MAP(pn);
			}
		}
	}
	lokmem_avail += size;
	mutex_exit(&lokmem_lock);
#ifdef LOKMEM_DEBUG
	if (lokmem_debug & LOKMEM_FREELIST_DUMP) {
		prom_printf("lokmem_free: FREELIST DUMP (after)\n");
		lokmem_listdump(lokmem_buf_freelist);
	}
	if (lokmem_debug & LOKMEM_USEDLIST_DUMP) {
		prom_printf("lokmem_free: USEDLIST DUMP (after)\n");
		lokmem_listdump(lokmem_buf_usedlist);
	}
#endif
}

/*
 * Called from startup() to take care of lokmem-free'd pages prior to
 * segkmem is ready. This routine takes care of freeing up the memory
 * associated with the buffers in the freelist.
 */
void
lokmem_gc()
{
	register lokmem_buf_t *lp;
	register u_int page_addr, last_page;

	ASSERT(segkmem_ready != 0);

	mutex_enter(&lokmem_lock);
	/*
	 * Free up physcial pages associated with the buffers in the
	 * freelist.
	 */
	lp = lokmem_buf_freelist;
	while (lp) {
		/*
		 * free up any physcial pages associated with the pages in
		 * the buffer.
		 */
		page_addr = ((u_int)lp->lokmem_buf_addr + MMU_PAGEOFFSET) &
			~MMU_PAGEOFFSET;
		last_page = (((u_int)lp->lokmem_buf_addr +
			lp->lokmem_buf_size) & ~MMU_PAGEOFFSET);
		lp = lp->lokmem_buf_next;
		if (last_page > page_addr) {
			register int npages = mmu_btop(last_page - page_addr);
			register u_int pagenum;

			pagenum = mmu_btop(page_addr - (u_int)lokmem_base);
			for (; npages; page_addr += MMU_PAGESIZE, npages--) {
				if (!IS_LOKMEM_MAPPED(pagenum)) {
					++pagenum;
					continue;
				}
				segkmem_free(&ktextseg, (caddr_t)page_addr,
					MMU_PAGESIZE);
				/* reset the inuse bit for this page */
				CLEAR_LOKMEM_MAP(pagenum);
				pagenum++;
			}
		}
	}
	mutex_exit(&lokmem_lock);
}

#ifdef LOKMEM_DEBUG
void
lokmem_listdump(lokmem_buf_t *lp)
{
	while (lp) {
		prom_printf("\tbuf_addr %x", lp->lokmem_buf_addr);
		prom_printf("\tbuf_size %x\n", lp->lokmem_buf_size);
		lp = lp->lokmem_buf_next;
	}
}
#endif

/*
 * Get pages from boot and hash them into the kernel's vp.
 * Used after the page structs have been allocated, but before
 * segkmem is ready.
 */

void
boot_getpages(base, size, align)
	caddr_t base;
	u_int size, align;
{
	struct page *pp;
	caddr_t addr, p;
	u_int n, pfn;

	addr = BOP_ALLOC(bootops, base, size, align);
	if (addr != base)
		cmn_err(CE_PANIC, "bop_alloc failed");
	for (p = addr, n = mmu_btop(size); n; p += MMU_PAGESIZE, n--) {
		pfn = va_to_pfn(p);
		ASSERT(pfn != (unsigned)(-1));
#ifdef LOKMEM_DEBUG
		if (lokmem_debug & LOKMEM_ALLOC_DEBUG)
			if ((u_int)base < (u_int)s_data)
				prom_printf("boot_getpages: addr %x pfn %x\n",
					base, pfn);
#endif
		pp = page_numtopp(pfn, SE_EXCL);
		ASSERT(pp != NULL);
		(void) page_hashin(pp, &kvp, (u_offset_t)(u_int)p, NULL);
		page_downgrade(pp);
	}
}

/*ARGSUSED*/
static int
segkmem_getmemid(struct seg *seg, caddr_t addr, memid_t *memidp)
{
	return (ENODEV);
}
