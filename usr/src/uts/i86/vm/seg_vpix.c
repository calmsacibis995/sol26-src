/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/* from SVR4:seg_vpix.c	1.3.2.4 */
#pragma ident "@(#)seg_vpix.c	1.21	96/08/08 SMI"

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
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 * 		All rights reserved.
 *
 */

/*
 *	Copyright (c) 1990 Intel Corporation
 *	Portions Copyright (c) 1990 Ing.C.Olivetti & C., S.p.A.
 *	Portions Copyright (c) 1990 NCR Corporation
 *	Portions Copyright (c) 1990 Oki Electric Industry Co., Ltd.
 *	Portions Copyright (c) 1990 Unisys Corporation
 *	All rights reserved.
 *
 *		INTEL CORPORATION PROPRIETARY INFORMATION
 *
 *	This software is supplied to USL under the terms of a license
 *	agreement with Intel Corp. and may not be copied or disclosed
 *	except in accordance with the terms of that agreement.
 */

/*
 * LIM 4.0 support:
 * Copyright (c) 1989 Phoenix Technologies Ltd.
 * All Rights Reserved.
 */


/*
 * SEG_VPIX -- VM support for VP/ix V86 processes.
 *
 * A seg_vpix segment supports the notion of "equivalenced" pages.  This
 * refers to multiple virtual pages in the same segment mapping to the
 * same physical (or anonymous) page.  It also allows given pages to be
 * mapped to specific physical device memory addresses (like seg_dev).
 */

#include <sys/types.h>
#include <sys/v86.h>
#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/mman.h>
#include <sys/debug.h>
#include <sys/cred.h>
#include <sys/vmsystm.h>
#include <sys/tuneable.h>
#include <sys/bitmap.h>
#include <sys/swap.h>
#include <sys/kmem.h>
#include <sys/sysmacros.h>
#include <sys/vtrace.h>
#include <sys/cmn_err.h>
#include <sys/bitmap.h>
#include <vm/seg_vpix.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_vn.h>
#include <vm/pvn.h>
#include <vm/anon.h>
#include <vm/page.h>
#include <vm/vpage.h>

#ifdef _VPIX

/*
 * Private seg op routines.
 */
#if defined(__STDC__)

static int	segvpix_dup(struct seg *, struct seg *);
static int	segvpix_unmap(struct seg *, caddr_t, u_int);
static void	segvpix_free(struct seg *);
static faultcode_t segvpix_fault(struct hat *, struct seg *, caddr_t, u_int,
			enum fault_type, enum seg_rw);
static faultcode_t segvpix_faulta(struct seg *, caddr_t);
static int	segvpix_setprot(struct seg *, caddr_t, u_int, u_int);
static int	segvpix_checkprot(struct seg *, caddr_t, u_int, u_int);
static int	segvpix_kluster(struct seg *, caddr_t, int);
static u_int	segvpix_swapout(struct seg *);
static int	segvpix_sync(struct seg *, caddr_t, u_int, int, u_int);
static int	segvpix_incore(struct seg *, caddr_t, u_int, char *);
static int	segvpix_lockop(struct seg *, caddr_t, u_int, int, int,
			ulong *, size_t);
static int	segvpix_getprot(struct seg *, caddr_t, u_int, u_int *);
static u_offset_t	segvpix_getoffset(struct seg *, caddr_t);
static int	segvpix_gettype(struct seg *, caddr_t);
static int	segvpix_getvp(struct seg *, caddr_t, struct vnode **);
static int	segvpix_advise(struct seg *, caddr_t, u_int, int);
static void	segvpix_dump(struct seg *);
static int	segvpix_pagelock(struct seg *, caddr_t, u_int,
			struct page ***, enum lock_type, enum seg_rw);
static int	segvpix_getmemid(struct seg *, caddr_t, memid_t *);

#else

static	int segvpix_dup(/* seg, newsegp */);
static	int segvpix_unmap(/* seg, addr, len */);
static	void segvpix_free(/* seg */);
static	faultcode_t segvpix_fault(/* seg, addr, type, rw */);
static	faultcode_t segvpix_faulta(/* seg, addr */);
static	int segvpix_setprot(/* seg, addr, len, prot */);
static	int segvpix_checkprot(/* seg, addr, len, prot */);
static	int segvpix_getprot(/* seg, addr, len, prot */);
static	int segvpix_kluster(/* seg, addr, delta */);
static	u_int segvpix_swapout(/* seg */);
static	int segvpix_sync(/* seg, addr, len, attr, flags */);
static	int segvpix_incore(/* seg, addr, len, vec */);
static	int segvpix_lockop(/* seg, addr, len, attr, op, bitmap, pos */);
static	u_offset_t segvpix_getoffset(/* seg, addr */);
static	int segvpix_gettype(/* seg, addr */);
static	int segvpix_getvp(/* seg, addr, vpp */);
static int	segvpix_advise(/* seg, addr_t, len, behav */);
static void	segvpix_dump(/* seg */);
static int	segvpix_pagelock(/* seg, addr, len, type, rw */);
static int	segvpix_getmemid(/* seg, addr, memid_t*/);
#endif	/* __STDC_ */

struct	seg_ops segvpix_ops = {
	segvpix_dup,
	segvpix_unmap,
	segvpix_free,
	segvpix_fault,
	segvpix_faulta,
	segvpix_setprot,
	segvpix_checkprot,
	segvpix_kluster,
	segvpix_swapout,
	segvpix_sync,
	segvpix_incore,
	segvpix_lockop,
	segvpix_getprot,
	segvpix_getoffset,
	segvpix_gettype,
	segvpix_getvp,
	segvpix_advise,
	segvpix_dump,
	segvpix_pagelock,
	segvpix_getmemid,
};

/*
 * Plain vanilla args structure, provided as a shorthand for others to use.
 */
static
struct segvpix_crargs	vpix_crargs = {
	0,
};

caddr_t vpix_argsp = (caddr_t)&vpix_crargs;

int
segvpix_create(seg, argsp)
	struct seg *seg;
	caddr_t argsp;
{
	register struct segvpix_crargs *a = (struct segvpix_crargs *)argsp;
	register struct segvpix_data *svd;
	register vpix_page_t *vpg;
	register u_int n, n2;
	register struct cred *cred;
	vpix_page_t *ovpage;
	u_int osize;
	u_int swresv;

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * We make sure the segment is entirely within one page table.
	 * This allows us to depend on the locking of the XTSS to keep
	 * virtual screen memory mappings from getting flushed.
	 */
	if (MMU_L1_INDEX(seg->s_base) !=
			MMU_L1_INDEX(seg->s_base + seg->s_size - 1))
		return (EINVAL);

	/*
	 * The segment will need private pages; reserve them now.
	 */
	for (swresv = seg->s_size, n = a->n_hole; n-- > 0; ) {
		/* Validate the hole: it must fall within the segment. */
		if (a->hole[n].base < seg->s_base ||
		    a->hole[n].base + a->hole[n].size >
				seg->s_base + seg->s_size) {
			return (EINVAL);
		}
		/* Don't reserve swap space for holes. */
		swresv -= a->hole[n].size;
	}

	if (anon_resv(swresv) == 0)
		return (ENOMEM);

	crhold(cred = CRED());

	if (seg->s_base != 0) {
		struct seg	*pseg;

		/*
		 * If this segment immediately follows another seg_vpix
		 * segment, coalesce them into a single segment.
		 */
		if ((pseg = seg->s_prev) != seg &&
		    pseg->s_ops == &segvpix_ops &&
		    pseg->s_base + pseg->s_size == seg->s_base) {
			svd = (struct segvpix_data *)pseg->s_data;
			ovpage = svd->vpage;
			osize = btop(pseg->s_size);
			seg_free(seg);
			(seg = pseg)->s_size += swresv;
		} else {
			anon_unresv(swresv);
			return (EINVAL);
		}
	} else {
		/* Start a new vpix segment. */
		svd = kmem_alloc(sizeof (struct segvpix_data), KM_SLEEP);
		rw_init(&svd->lock, "segvpix rwlock", RW_DEFAULT, DEFAULT_WT);
		svd->cred = cred;
		svd->swresv = 0;
		seg->s_ops = &segvpix_ops;
		seg->s_data = (void *)svd;
		ovpage = NULL;
		osize = 0;

		/*
		 * Enable data collection for the page data file and get
		 * unique id from the hat layer. (See segvpix_modscan()
		 * also)
		 */
		if ((svd->hatid = hat_startstat(seg->s_as)) == -1) {
			crfree(svd->cred);
			kmem_free(svd, sizeof (struct segvpix_data));
			return (ENOMEM);
		}
	}

	svd->swresv += swresv;

	svd->vpage = (vpix_page_t *)kmem_zalloc((u_int)
		(seg_pages(seg) * sizeof (vpix_page_t)), KM_SLEEP);

	if (ovpage) {
		bcopy((caddr_t)ovpage, (caddr_t)svd->vpage,
			osize * sizeof (vpix_page_t));
		kmem_free((caddr_t)ovpage, osize * sizeof (vpix_page_t));
	}

	for (vpg = &svd->vpage[n = seg_pages(seg)]; n-- > osize; ) {
		--vpg;
		vpg->eq_map = vpg->eq_link = vpg->rp_eq_list = n;
	}

	/* Change holes to be unmapped. */
	for (n = a->n_hole; n-- > 0; ) {
		vpg = &svd->vpage[btop(a->hole[n].base)];
		for (n2 = btopr(a->hole[n].size); n2-- > 0; vpg++) {
			vpg->eq_map = vpg->eq_link = vpg->rp_eq_list = NULLEQ;
			vpg->rp_hole = 1;
		}
	}

	return (0);
}

static int
segvpix_dup(seg, newseg)
	struct seg *seg, *newseg;
{
	/*
	 * We don't need any segment level locks for "segvpix" data
	 * since the address space is "write" locked.
	 */
	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	ASSERT(newseg && newseg->s_as);
	newseg->s_as->a_size -= seg->s_size;
	seg_free(newseg);
	return (0);
}

static int
segvpix_unmap(seg, addr, len)
	register struct seg *seg;
	register caddr_t addr;
	u_int len;
{
	register struct segvpix_data *svd = (struct segvpix_data *)seg->s_data;

	/*
	 * We don't need any segment level locks for "segvpix" data
	 * since the address space is "write" locked.
	 */
	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * Check for bad sizes
	 */
	if (addr < seg->s_base || addr + len > seg->s_base + seg->s_size ||
	    (len & PAGEOFFSET) || ((u_int)addr & PAGEOFFSET))
		cmn_err(CE_PANIC, "segvpix_unmap");

	/*
	 * Only allow entire segment to be unmapped.
	 */
	if (addr != seg->s_base || len != seg->s_size)
		return (-1);

	/*
	 * Remove any page locks set through this mapping.
	 */
	(void) segvpix_lockop(seg, addr, len, 0, MC_UNLOCK,
				(ulong *)NULL, (size_t)NULL);

	/*
	 * Unload any hardware translations in the range to be taken out.
	 */

	hat_unload(seg->s_as->a_hat, addr, len, HAT_UNLOAD_UNMAP);

	seg_free(seg);

	return (0);
}

static void
segvpix_free(seg)
	struct seg *seg;
{
	register struct segvpix_data *svd = (struct segvpix_data *)seg->s_data;
	u_int npages = seg_pages(seg);

	/*
	 * We don't need any segment level locks for "segvpix" data
	 * since the address space is "write" locked.
	 */
	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * Remove any page locks set through this mapping.
	 */
	(void) segvpix_lockop(seg, seg->s_base, seg->s_size,
			0, MC_UNLOCK, (ulong *)NULL, (size_t)NULL);

	/*
	 * Release anonymous pages.
	 */
	if (svd->vpage) {
		vpix_page_t	*vpage;

		for (vpage = &svd->vpage[npages]; vpage-- != svd->vpage; ) {
			if (!vpage->rp_phys && vpage->rp_anon)
				anon_decref(vpage->rp_anon);
		}
	}

	/*
	 * Deallocate the per-page arrays if necessary.
	 */
	if (svd->vpage != NULL) {
		kmem_free((caddr_t)svd->vpage, npages * sizeof (vpix_page_t));
		svd->vpage = NULL;
	}

	/*
	 * Release swap reservation.
	 */
	if (svd->swresv) {
		anon_unresv(svd->swresv);
		svd->swresv = 0;
	}

	/*
	 * Release claim on credentials, and finally free the
	 * private data.
	 */
	crfree(svd->cred);
	seg->s_data = NULL;
	SEGVPIX_LOCK_DESTROY(seg->s_as, &svd->lock);
	kmem_free(svd, sizeof (struct segvpix_data));
}

/*
 * Do a F_SOFTUNLOCK call over the range requested.
 * The range must have already been F_SOFTLOCK'ed.
 */
static void
segvpix_softunlock(seg, addr, len, rw)
	struct seg *seg;
	caddr_t addr;
	u_int len;
	enum seg_rw rw;
{
	register struct segvpix_data *svd = (struct segvpix_data *)seg->s_data;
	register vpix_page_t *vpp;
	u_int vpg, evpg, rp, equiv;
	register vpix_page_t *rpp;
	vpix_page_t *vpage;
	page_t *pp;
	struct vnode *vp;
	u_offset_t offset;

	ASSERT(seg->s_as && AS_READ_HELD(seg->s_as, &seg->s_as->a_lock));

	SEGVPIX_LOCK_ENTER(seg->s_as, &svd->lock, RW_READER);

	vpp = svd->vpage;
	vpage = &vpp[vpg = seg_page(seg, addr)];
	evpg = seg_page(seg, addr + len);

	for (; vpg != evpg; vpg++, vpage++) {
		if ((rp = vpage->eq_map) == NULLEQ)
			continue;

		/* If a physical device page, just unlock the translation */
		if ((rpp = &vpp[rp])->rp_phys) {
			/*
			 * Unlock the translation for the page and equivalents
			 */
			equiv = vpg;
			do {
				hat_unlock(seg->s_as->a_hat,
				    (caddr_t)mmu_ptob(equiv), PAGESIZE);
			} while ((equiv = vpp[equiv].eq_link) != vpg);
			continue;
		}

		if (rpp->rp_anon == NULL)
			continue;

		swap_xlate(rpp->rp_anon, &vp, &offset);

		pp = page_find(vp, offset);

		if (pp == NULL)
			cmn_err(CE_PANIC, "segvpix_softunlock");


		if (rw == S_WRITE) {
			hat_setmod(pp);
			if (seg->s_as->a_vbits) {
				hat_setstat(seg->s_as, addr, PAGESIZE,
				    P_MOD | P_REF);
			}
		}
		if (rw != S_OTHER) {
			hat_setref(pp);
			if (seg->s_as->a_vbits) {
				hat_setstat(seg->s_as, addr, PAGESIZE, P_REF);
			}
		}

		/*
		 * Unlock the translation for the page and its equivalents
		 */
		equiv = vpg;
		do {
			hat_unlock(seg->s_as->a_hat, (caddr_t)mmu_ptob(equiv),
			    PAGESIZE);
		} while ((equiv = vpp[equiv].eq_link) != vpg);

		page_unlock(pp);

		mutex_enter(&freemem_lock);
		availrmem++;
		mutex_exit(&freemem_lock);
	}
	SEGVPIX_LOCK_EXIT(seg->s_as, &svd->lock);
}


/*
 * Handles all the dirty work of getting the right
 * anonymous pages and loading up the translations.
 * This routine is called only from segvpix_fault()
 * when looping over the range of addresses requested.
 *
 * The basic algorithm here is:
 * 	If this is an anon_zero case
 *		Call anon_zero to allocate page
 *	else
 *		Use anon_getpage to get the page
 *	endif
 *	Load up the translation to the page and its equivalents
 *	return
 */
static int
segvpix_faultpage(hat, seg, addr, vp, type, rw)
	struct hat *hat;
	struct seg *seg;
	caddr_t addr;
	u_int vp;
	enum fault_type type;
	enum seg_rw rw;
{
	register struct segvpix_data *svd = (struct segvpix_data *)seg->s_data;
	register vpix_page_t *vpp;
	register page_t *pp;
	u_int rp, equiv;
	vpix_page_t *vpage, *rpp;
	page_t *anon_pl[1 + 1];
	struct anon **app;
	u_int vpprot = PROT_READ|PROT_WRITE|PROT_USER;
	u_int hat_flags;
	int err;

	ASSERT(SEGVPIX_READ_HELD(seg->s_as, &svd->lock));

	vpage = (vpp = svd->vpage) + vp;
	if ((rp = vpage->eq_map) == NULLEQ)
		return (FC_NOMAP);

	hat_flags = (type == F_SOFTLOCK ? HAT_LOCK : HAT_LOAD);

	if ((rpp = &vpp[rp])->rp_phys) {
		/*
		 * This page is mapped to physical device memory.
		 * Load translation for the page and its equivalents.
		 */
		equiv = vp;
		do {
			hat_devload(hat, (caddr_t)mmu_ptob(equiv),
				PAGESIZE, rpp->rp_pfn, vpprot, hat_flags);
		} while ((equiv = vpp[equiv].eq_link) != vp);
		return (0);
	}

	if (type == F_SOFTLOCK) {
		mutex_enter(&freemem_lock);
		if (availrmem <= tune.t_minarmem) {
			mutex_exit(&freemem_lock);
			return (FC_MAKE_ERR(ENOMEM));   /* out of real memory */
		} else {
			availrmem--;
		}
		mutex_exit(&freemem_lock);
	}

	if (*(app = &rpp->rp_anon) == NULL) {
		/*
		 * Allocate a (normally) writable
		 * anonymous page of zeroes.
		 */
		pp = anon_zero(seg, addr, app, svd->cred);
		if (pp == NULL) {
			err = ENOMEM;
			goto out;	/* out of swap space */
		}
	} else {
		/*
		 * Obtain the page structure via anon_getpage().
		 */
		err = anon_getpage(app, &vpprot, anon_pl, PAGESIZE,
				    seg, addr, rw, svd->cred);
		if (err)
			goto out;
		pp = anon_pl[0];

		ASSERT(pp != NULL && se_shared_assert(&pp->p_selock));
	}

	hat_setref(pp);

	/*
	 * Load translation for the page and its equivalents
	 */
	equiv = vp;
	do {
		hat_memload(hat, (caddr_t)mmu_ptob(equiv),
		    pp, vpprot, hat_flags);
	} while ((equiv = vpp[equiv].eq_link) != vp);

	if (type != F_SOFTLOCK)
		page_unlock(pp);

	return (0);

out:
	if (type == F_SOFTLOCK) {
		mutex_enter(&freemem_lock);
		availrmem++;
		mutex_exit(&freemem_lock);
	}
	return (FC_MAKE_ERR(err));
}

/*
 * This routine is called via a machine specific fault handling routine.
 * It is also called by software routines wishing to lock or unlock
 * a range of addresses.
 *
 * Here is the basic algorithm:
 *	If unlocking
 *		Call segvpix_softunlock
 *		Return
 *	endif
 *	Checking and set up work
 *	Loop over all addresses requested
 *		Call segvpix_faultpage to load up translations
 *				and handle anonymous pages
 *	endloop
 */

static faultcode_t
segvpix_fault(hat, seg, addr, len, type, rw)
	struct hat *hat;
	struct seg *seg;
	caddr_t addr;
	u_int len;
	enum fault_type type;
	enum seg_rw rw;
{
	struct segvpix_data *svd = (struct segvpix_data *)seg->s_data;
	caddr_t a;
	u_int vpg;
	int err = 0;

	ASSERT(seg->s_as && AS_READ_HELD(seg->s_as, &seg->s_as->a_lock));

	/* ASSERT(type != F_PROT); */	/* since seg_vpix always read/write */

	/*
	 * First handle the easy stuff
	 */
	if (type == F_SOFTUNLOCK) {
		segvpix_softunlock(seg, addr, len, rw);
		return (0);
	}

	vpg = seg_page(seg, addr);

	SEGVPIX_LOCK_ENTER(seg->s_as, &svd->lock, RW_READER);

	/*
	 * Ok, now loop over the address range and handle faults
	 */
	for (a = addr; a < addr + len; a += PAGESIZE, ++vpg) {
		if (err = segvpix_faultpage(hat, seg, a, vpg, type, rw)) {
			SEGVPIX_LOCK_EXIT(seg->s_as, &svd->lock);
			return (err);
		}
	}
	SEGVPIX_LOCK_EXIT(seg->s_as, &svd->lock);
	return (0);
}

/*
 * This routine is used to start I/O on pages asynchronously.
 */
static faultcode_t
segvpix_faulta(seg, addr)
	struct seg *seg;
	caddr_t addr;
{
	register struct segvpix_data *svd = (struct segvpix_data *)seg->s_data;
	register u_int rp;
	register vpix_page_t *rpp;
	struct anon **app;
	int err;

	ASSERT(seg->s_as && AS_READ_HELD(seg->s_as, &seg->s_as->a_lock));

	if ((rp = svd->vpage[seg_page(seg, addr)].eq_map) == NULLEQ)
		return (0);
	if ((rpp = &svd->vpage[rp])->rp_phys)
		return (0);
	if (*(app = &rpp->rp_anon) != NULL) {
		SEGVPIX_LOCK_ENTER(seg->s_as, &svd->lock, RW_READER);
		err = anon_getpage(app, (u_int *)NULL,
				(struct page **)NULL, 0, seg, addr, S_READ,
				svd->cred);
		SEGVPIX_LOCK_EXIT(seg->s_as, &svd->lock);
		if (err)
			return (FC_MAKE_ERR(err));
	}

	return (0);
}

static int
segvpix_setprot(seg, addr, len, prot)
	register struct seg *seg;
	register caddr_t addr;
	register u_int len, prot;
{
	if ((prot & (PROT_READ|PROT_WRITE)) == (PROT_READ|PROT_WRITE))
		return (EACCES);	/* Only R/W protections allowed */
	return (0);
}

static int
segvpix_checkprot(seg, addr, len, prot)
	register struct seg *seg;
	register caddr_t addr;
	register u_int len, prot;
{
	return (((prot & (PROT_READ|PROT_WRITE|PROT_USER)) != prot) ?
								EACCES : 0);
}

static int
segvpix_getprot(seg, addr, len, protv)
	register struct seg *seg;
	register caddr_t addr;
	register u_int len, *protv;
{
	struct segvpix_data *svd = (struct segvpix_data *)seg->s_data;
	int pgno = seg_page(seg, addr+len) - seg_page(seg, addr) + 1;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	while (--pgno >= 0)
		*protv++ = PROT_READ|PROT_WRITE;
	return (0);
}

/*
 * Check to see if it makes sense to do kluster/read ahead to
 * addr + delta relative to the mapping at addr.  We assume here
 * that delta is a signed PAGESIZE'd multiple (which can be negative).
 *
 * For seg_vpix, we currently "approve" of the action if we are
 * still in the segment and it maps from the same vp/off.
 */
static int
segvpix_kluster(seg, addr, delta)
	register struct seg *seg;
	register caddr_t addr;
	int delta;
{
	register struct segvpix_data *svd = (struct segvpix_data *)seg->s_data;
	register struct anon *oap, *ap;
	register int pd;
	register u_int page;
	u_int rp1, rp2;
	vpix_page_t *rpp1, *rpp2;
	struct vnode *vp1, *vp2;
	u_offset_t off1, off2;

	ASSERT(seg->s_as && AS_READ_HELD(seg->s_as, &seg->s_as->a_lock));
	ASSERT(SEGVPIX_LOCK_HELD(seg->s_as, &svd->lock));

	if (addr + delta < seg->s_base ||
	    addr + delta >= (seg->s_base + seg->s_size))
		return (-1);		/* exceeded segment bounds */

	pd = delta / PAGESIZE;		/* divide to preserve sign bit */
	page = seg_page(seg, addr);

	if ((rp1 = svd->vpage[page].eq_map) == NULLEQ ||
	    (rp2 = svd->vpage[page + pd].eq_map) == NULLEQ)
		return (-1);

	rpp1 = &svd->vpage[rp1];
	rpp2 = &svd->vpage[rp2];

	oap = rpp1->rp_phys ? NULL : rpp1->rp_anon;
	ap = rpp2->rp_phys ? NULL : rpp2->rp_anon;

	if (oap == NULL || ap == NULL)
		return (-1);

	/*
	 * Now we know we have two anon pointers - check to
	 * see if they happen to be properly allocated.
	 */
	swap_xlate(ap, &vp1, &off1);
	swap_xlate(oap, &vp2, &off2);
	if (!VOP_CMP(vp1, vp2) || off1 - off2 != delta)
		return (-1);
	return (0);
}

/*
 * Swap the pages of seg out to secondary storage, returning the
 * number of bytes of storage freed.
 *
 * The basic idea is first to unload all translations and then to call
 * VOP_PUTPAGE for all newly-unmapped pages, to push them out to the
 * swap device.  Pages to which other segments have mappings will remain
 * mapped and won't be swapped.  Our caller (as_swapout) has already
 * performed the unloading step.
 *
 * The value returned is intended to correlate well with the process's
 * memory requirements.  However, there are some caveats:
 * 	We assume that the hat layer maintains a large enough translation
 *	cache to capture process reference patterns.
 */
static u_int
segvpix_swapout(seg)
	struct seg *seg;
{
	struct segvpix_data *svd = (struct segvpix_data *)seg->s_data;
	register u_int pgcnt = 0;
	u_int npages;
	register u_int page;
	register struct page *pp;
	register u_int rp;
	register vpix_page_t *rpp;
	struct vnode *vp;
	u_offset_t off;

	ASSERT(seg->s_as && AS_READ_HELD(seg->s_as, &seg->s_as->a_lock));

	SEGVPIX_LOCK_ENTER(seg->s_as, &svd->lock, RW_READER);
	/*
	 * Find pages unmapped by our caller and force them
	 * out to the virtual swap device.
	 */
	npages = btop(seg->s_size);
	for (page = 0; page < npages; page++) {
		/*
		 * Obtain <vp, off> pair for the page, then look it up.
		 */
		if ((rp = svd->vpage[page].eq_map) == NULLEQ)
			continue;
		if ((rpp = &svd->vpage[rp])->rp_phys || rpp->rp_anon == NULL)
			continue;
		swap_xlate(rpp->rp_anon, &vp, &off);

		pp = page_lookup_nowait(vp, off, SE_SHARED);
		if (pp == NULL)
			continue;

		/*
		 * Examine the page to see whether it can be tossed out,
		 * keeping track of how many we've found.
		 */
		if (!page_tryupgrade(pp)) {
			/*
			 * If the page has an i/o lock and no mappings,
			 * it's very likely that the page is being
			 * written out as a result of klustering.
			 * Assume this is so and take credit for it here.
			 */
			if (!page_io_trylock(pp)) {
				if (!hat_page_is_mapped(pp))
					pgcnt++;
			} else {
				page_io_unlock(pp);
			}
			page_unlock(pp);
			continue;
		}
		ASSERT(!page_iolock_assert(pp));

		/*
		 * Skip if page is logically unavailable for removal.
		 */

		if (pp->p_lckcnt > 0 || hat_page_is_mapped(pp)) {
			page_unlock(pp);
			continue;
		}

		/*
		 * No longer mapped -- we can toss it out.  How
		 * we do so depends on whether or not it's dirty.
		 */
		if (PP_ISMOD(pp) && pp->p_vnode) {
			/*
			 * We must clean the page before it can be
			 * freed.  Setting B_FREE will cause pvn_done
			 * to free the page when the i/o completes.
			 * XXX: This also causes it to be accounted
			 *	as a pageout instead of a swap: need
			 *	B_SWAPOUT bit to use instead of B_FREE.
			 *
			 * Hold the vnode before releasing the page lock
			 * to prevent it from being freed and re-used by
			 * some other thread.
			 */
			VN_HOLD(vp);
			page_unlock(pp);
			(void) VOP_PUTPAGE(vp, off, PAGESIZE,
			    B_ASYNC | B_FREE, svd->cred);
			VN_RELE(vp);
		} else {
			/*
			 * The page was clean, free it.
			 *
			 * XXX: Can we ever encounter modified pages
			 *	with no associated vnode here?
			 */
			VN_DISPOSE(pp, B_FREE, 0, svd->cred);
		}

		/*
		 * Credit now even if i/o is in progress.
		 */
		pgcnt++;
	}
	SEGVPIX_LOCK_EXIT(seg->s_as, &svd->lock);

	return (mmu_ptob(pgcnt));
}

/*
 * Synchronize primary storage cache with real object in virtual memory.
 */
static int
segvpix_sync(seg, addr, len, attr, flags)
	struct seg *seg;
	register caddr_t addr;
	u_int len;
	int attr;
	u_int flags;
{
	register struct segvpix_data *svd = (struct segvpix_data *)seg->s_data;
	register vpix_page_t *vpage;
	u_int rp;
	register vpix_page_t *rpp;
	register struct anon *ap;
	register page_t *pp;
	struct vnode *vp;
	u_offset_t off;
	caddr_t eaddr;
	int bflags;
	int err = 0;
	int segtype;
	int pageprot;


	ASSERT(seg->s_as && AS_READ_HELD(seg->s_as, &seg->s_as->a_lock));

	SEGVPIX_LOCK_ENTER(seg->s_as, &svd->lock, RW_READER);

	bflags = B_FORCE | ((flags & MS_ASYNC) ? B_ASYNC : 0) |
			    ((flags & MS_INVALIDATE) ? B_INVAL : 0);

	if (attr) {
		pageprot = attr & ~(SHARED|PRIVATE);
		segtype = attr & SHARED ? MAP_SHARED : MAP_PRIVATE;

		/*
		 * We are done if the segment types or protections
		 * don't match.
		 */
		if ((segtype != MAP_PRIVATE) ||
			(pageprot & (PROT_READ|PROT_WRITE)) !=
				(PROT_READ|PROT_WRITE)) {
			SEGVPIX_LOCK_EXIT(seg->s_as, &svd->lock);
			return (0);
		}
	}

	vpage = &svd->vpage[seg_page(seg, addr)];

	for (eaddr = addr + len; addr < eaddr; addr += PAGESIZE) {

		if ((rp = (vpage++)->eq_map) == NULLEQ)
			continue;
		if ((rpp = &svd->vpage[rp])->rp_phys)
			continue;
		if ((ap = rpp->rp_anon) == NULL)
			continue;

		swap_xlate(ap, &vp, &off);

		/*
		 * See if any of these pages are locked --  if so, then we
		 * will have to truncate an invalidate request at the first
		 * locked one. We don't need the page_struct_lock to test
		 * as this is only advisory; even if we acquire it someone
		 * might race in and lock the page after we unlock and before
		 * we do the PUTPAGE, then PUTPAGE simply does nothing.
		 */
		if (flags & MS_INVALIDATE) {
			if ((pp = page_lookup(vp, off, SE_SHARED)) != NULL) {
				if (pp->p_lckcnt) {
					page_unlock(pp);
					SEGVPIX_LOCK_EXIT(seg->s_as,
								&svd->lock);
					return (EBUSY);
				}
				page_unlock(pp);
			}
		}

		/*
		* XXX - Should ultimately try to kluster
		* calls to VOP_PUTPAGE for performance.
		*/
		VN_HOLD(vp);
		err = VOP_PUTPAGE(vp, off, PAGESIZE,
				bflags, svd->cred);
		VN_RELE(vp);
		if (err)
			break;
	}
	SEGVPIX_LOCK_EXIT(seg->s_as, &svd->lock);
	return (err);
}

/*
 * Determine if we have data corresponding to pages in the
 * primary storage virtual memory cache (i.e., "in core").
 * N.B. Assumes things are "in core" if page structs exist.
 */
static int
segvpix_incore(seg, addr, len, vec)
	struct seg *seg;
	caddr_t addr;
	u_int len;
	char *vec;
{
	register struct segvpix_data *svd = (struct segvpix_data *)seg->s_data;
	u_int rp;
	register vpix_page_t *rpp;
	register page_t *pp;
	struct vnode *vp;
	u_offset_t offset;
	u_int p = seg_page(seg, addr);
	u_int ep = seg_page(seg, addr + len);
	u_int v;

	ASSERT(seg->s_as && AS_READ_HELD(seg->s_as, &seg->s_as->a_lock));

	SEGVPIX_LOCK_ENTER(seg->s_as, &svd->lock, RW_READER);

	for (v = 0; p < ep; p++, addr += PAGESIZE, v += PAGESIZE) {
		if ((rp = svd->vpage[p].eq_map) != NULLEQ &&
		    !(rpp = &svd->vpage[rp])->rp_phys &&
		    rpp->rp_anon != NULL) {
			swap_xlate(rpp->rp_anon, &vp, &offset);
			/*
			 * Try to obtain a "shared" lock on the page
			 * without blocking.  If this fails, determine
			 * if the page is in memory.
			 */
			pp = page_lookup_nowait(vp, offset, SE_SHARED);
			if (pp == NULL)
				*vec++ = (page_exists(vp, offset) != NULL);
			else {

				/*
				 * segvpix_incore() sets other values (USL
				 * did not). XXX who cares?
				 */
				*vec++ = 1;
				page_unlock(pp);
			}
		} else
			*vec++ = 0;
	}
	SEGVPIX_LOCK_EXIT(seg->s_as, &svd->lock);
	return (v);
}


/*
 * Lock down (or unlock) pages mapped by this segment.
 */
static int
segvpix_lockop(seg, addr, len, attr, op, lockmap, pos)
	struct seg *seg;
	caddr_t addr;
	u_int len;
	int   attr;
	int op;
	ulong *lockmap;
	size_t pos;
{
	struct segvpix_data *svd = (struct segvpix_data *)seg->s_data;
	register vpix_page_t *vpp, *vpage, *epage, *rpp;
	struct page *pp;
	struct vnode *vp;
	u_offset_t off;
	int segtype;
	int pageprot;
	int err;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	SEGVPIX_LOCK_ENTER(seg->s_as, &svd->lock, RW_WRITER);
	if (attr) {
		pageprot = attr & ~(SHARED|PRIVATE);
		segtype = attr & SHARED ? MAP_SHARED : MAP_PRIVATE;

		/*
		 * We are done if the segment types or protections
		 * don't match.
		 */
		if ((segtype != MAP_PRIVATE) ||
			(pageprot & (PROT_READ|PROT_WRITE)) !=
					(PROT_READ|PROT_WRITE)) {
			SEGVPIX_LOCK_EXIT(seg->s_as, &svd->lock);
			return (0);
		}
	}

	/*
	 * Loop over all pages in the range.  Process if we're locking and
	 * page has not already been locked in this mapping; or if we're
	 * unlocking and the page has been locked.
	 */
	vpp = svd->vpage;
	vpage = &vpp[seg_page(seg, addr)];
	epage = &vpp[seg_page(seg, addr + len)];

	for (; vpage < epage; vpage++, pos++, addr += PAGESIZE) {
		if (vpage->eq_map == NULLEQ)
			continue;
		if ((rpp = &vpp[vpage->eq_map])->rp_phys)
			continue;
		if ((op == MC_LOCK && !rpp->rp_lock) ||
		    (op == MC_UNLOCK && rpp->rp_lock)) {
			/*
			 * If we're locking, softfault the page in memory.
			 */
			if (op == MC_LOCK) {
				SEGVPIX_LOCK_EXIT(seg->s_as, &svd->lock);
				if (segvpix_fault(seg->s_as->a_hat, seg, addr,
				    PAGESIZE, F_INVAL, S_OTHER) != 0)
					return (EIO);
				SEGVPIX_LOCK_ENTER(seg->s_as, &svd->lock,
					RW_WRITER);
			}

			ASSERT(rpp->rp_anon);	/* SOFTLOCK will force these */

			/*
			 * Get name for page, accounting for
			 * existence of private copy.
			 */
			swap_xlate(rpp->rp_anon, &vp, &off);

			/*
			 * Get page frame.  It's ok if the page is
			 * not available when we're unlocking, as this
			 * may simply mean that a page we locked got
			 * truncated out of existence after we locked it.
			 *
			 * Invoke VOP_GETPAGE() to obtain the page
			 * since we may need to read it from disk if its
			 * been paged out.
			 */
			if (op != MC_LOCK)
				pp = page_lookup(vp, off, SE_SHARED);
			else {
				page_t *pl[1 + 1];

				if (VOP_GETPAGE(vp, off, PAGESIZE,
				    (u_int *)NULL, pl, PAGESIZE, seg, addr,
				    S_OTHER, svd->cred))
					cmn_err(CE_PANIC,
						"segvpix_lockop: no page");
				pp = pl[0];
				ASSERT(pp != NULL);
			}

			/*
			 * Perform page-level operation appropriate to
			 * operation.  If locking, undo the SOFTLOCK
			 * performed to bring the page into memory
			 * after setting the lock.  If unlocking,
			 * and no page was found, account for the claim
			 * separately.
			 */
			if (op == MC_LOCK) {
				err = page_pp_lock(pp, 0, 0);
				if (err != 0) {
					rpp->rp_lock = 1;
					if (lockmap != (ulong *)NULL) {
						BT_SET(lockmap, pos);
					}
				}
				page_unlock(pp);
				if (err == 0) {
					SEGVPIX_LOCK_EXIT(seg->s_as,
								&svd->lock);
					return (EAGAIN);
				}
			} else {
				if (pp) {
					page_pp_unlock(pp, 0, 0);
					page_unlock(pp);
				}
				rpp->rp_lock = 0;
			}

		}
	}
	SEGVPIX_LOCK_EXIT(seg->s_as, &svd->lock);
	return (0);
}

static int
segvpix_advise(seg, addr, len, behav)
	struct seg *seg;
	caddr_t addr;
	u_int len;
	int behav;
{
	return (0);
}

/* ARGSUSED */
static u_offset_t
segvpix_getoffset(seg, addr)
	register struct seg *seg;
	caddr_t addr;
{
	return ((u_offset_t)0);
}

/* ARGSUSED */
static int
segvpix_gettype(seg, addr)
	register struct seg *seg;
	caddr_t addr;
{
	return (MAP_PRIVATE);
}

/* ARGSUSED */
static int
segvpix_getvp(seg, addr, vpp)
	register struct seg *seg;
	caddr_t addr;
	struct vnode **vpp;
{
	return (-1);
}


/*
 * Map a range of virtual pages in the segment to specific physical
 * (device memory) pages.
 */
int
segvpix_physmap(seg, vpage, ppage, npages)
	register struct seg	*seg;
	u_int		vpage;
	u_int		ppage;
	u_int		npages;
{
	struct segvpix_data *svd = (struct segvpix_data *)seg->s_data;
	register vpix_page_t *vpp;
	register vpix_page_t *rpp;
	register u_int	vpg, equiv;
	u_int		pfn;

	ASSERT(seg->s_as && AS_READ_HELD(seg->s_as, &seg->s_as->a_lock));

	SEGVPIX_LOCK_ENTER(seg->s_as, &svd->lock, RW_WRITER);

	vpp = svd->vpage;

	for (rpp = &vpp[vpage]; npages-- > 0; rpp++, ppage++) {

		/* If this rpage is a "hole", fail */
		if (rpp->rp_hole)
			return (ENXIO);

		/* If this rpage had an anon before, discard it */
		if (!rpp->rp_phys && rpp->rp_anon != NULL)
			anon_decref(rpp->rp_anon);

		/* Indicate this rpage is now physmapped */
		rpp->rp_phys = 1;
		rpp->rp_pfn = ppage;

		/* Get a pointer to one of the vpages for this rp page */
		if ((vpg = rpp->rp_eq_list) == NULLEQ)
			continue;

		/* Unload any previous translations */
		equiv = vpg;
		do {
			hat_unload(seg->s_as->a_hat, (caddr_t)mmu_ptob(equiv),
			    PAGESIZE, HAT_UNLOAD);
			/* Load the new translation. [optimization] */
			hat_devload(seg->s_as->a_hat, (caddr_t)mmu_ptob(equiv),
			    PAGESIZE, ppage, PROT_ALL, 0);
		} while ((equiv = vpp[equiv].eq_link) != vpg);
	}
	SEGVPIX_LOCK_EXIT(seg->s_as, &svd->lock);
	return (0);
}


/*
 * Revert a range of virtual pages from a physical mapping to anonymous
 * memory.  This undoes the effect of segvpix_physmap().
 */
int
segvpix_unphys(seg, vpage, npages)
	register struct seg	*seg;
	u_int		vpage;
	u_int		npages;
{
	struct segvpix_data *svd = (struct segvpix_data *)seg->s_data;
	register vpix_page_t *vpp;
	register vpix_page_t *rpp;
	register u_int	vpg, equiv;

	ASSERT(seg->s_as && AS_READ_HELD(seg->s_as, &seg->s_as->a_lock));

	SEGVPIX_LOCK_ENTER(seg->s_as, &svd->lock, RW_WRITER);

	vpp = svd->vpage;

	for (rpp = &vpp[vpage]; npages-- > 0; rpp++) {

		/* Indicate this rpage is no longer physmapped */
		if (!rpp->rp_phys)
			continue;
		rpp->rp_phys = 0;
		rpp->rp_pfn = 0;

		/* Get a pointer to one of the vpages for this rp page */
		if ((vpg = rpp->rp_eq_list) == NULLEQ)
			continue;

		/* Unload any previous translations */
		equiv = vpg;
		do {
			hat_unload(seg->s_as->a_hat, (caddr_t)mmu_ptob(equiv),
				PAGESIZE, HAT_UNLOAD);
		} while ((equiv = vpp[equiv].eq_link) != vpg);
	}

	SEGVPIX_LOCK_EXIT(seg->s_as, &svd->lock);
	return (0);
}


/*
 * Set up an equivalence between two virtual pages in the segment.
 * First, vpage_from is unlinked from any equivalence set it might belong
 * to.  Then it is attached to the (possibly unit) set containing
 * rpage_to.  Note that this means that if vpage_from and rpage_to are
 * the same, this will have the effect of unlinking that page from any
 * equivalence set.
 */
int
segvpix_page_equiv(seg, vpage_from, rpage_to)
	register struct seg	*seg;
	u_int		vpage_from;
	u_int		rpage_to;
{
	struct segvpix_data *svd = (struct segvpix_data *)seg->s_data;
	register vpix_page_t	*vpp;
	register u_int		rp, equiv, eq2;
	register vpix_page_t	*rpp, *vpg;

	ASSERT(seg->s_as && AS_READ_HELD(seg->s_as, &seg->s_as->a_lock));

	SEGVPIX_LOCK_ENTER(seg->s_as, &svd->lock, RW_WRITER);

	vpg = &(vpp = svd->vpage)[vpage_from];

	if ((rp = vpg->eq_map) == rpage_to) {
		/* Already mapped to the right page. */
		SEGVPIX_LOCK_EXIT(seg->s_as, &svd->lock);
		return (0);
	}

	/*
	 * Unlink vpage_from from its old mapping.
	 */
	if (rp != NULLEQ) {
		rpp = &vpp[rp];

		/* Unlink from the previous equivalence set */
		ASSERT(vpg->eq_link != NULLEQ);
		for (equiv = vpg->eq_link; vpp[equiv].eq_link != vpage_from; )
			equiv = vpp[equiv].eq_link;
		if (equiv == vpage_from)
			rpp->rp_eq_list = NULLEQ;
		else
			vpp[equiv].eq_link = vpg->eq_link;

		/* Unload any previous translation */
		hat_unload(seg->s_as->a_hat, (caddr_t)mmu_ptob(vpage_from),
				PAGESIZE, HAT_UNLOAD);
	}

	/*
	 * Make sure we don't map to a "hole".
	 */
	rpp = &vpp[rp = vpg->eq_map = rpage_to];
	if (rpp->rp_hole) {
		vpg->eq_map = NULLEQ;
		if (vpage_from != rpage_to) {
			SEGVPIX_LOCK_EXIT(seg->s_as, &svd->lock);
			return (ENXIO);
		}
		SEGVPIX_LOCK_EXIT(seg->s_as, &svd->lock);
		return (0);
	}

	/*
	 * Link to rpage_to.
	 */
	if ((equiv = rpp->rp_eq_list) == NULLEQ)
		rpp->rp_eq_list = vpg->eq_link = vpage_from;
	else {
		vpg->eq_link = vpp[equiv].eq_link;
		vpp[equiv].eq_link = vpage_from;
	}

	SEGVPIX_LOCK_EXIT(seg->s_as, &svd->lock);
	return (0);
}


/*
 * This is like segvpix_page_equiv(), but works on a range of pages.
 */
int
segvpix_range_equiv(seg, vpage_from, rpage_to, npages)
	register struct seg	*seg;
	u_int		vpage_from;
	u_int		rpage_to;
	u_int		npages;
{
	register u_int	npg = npages;
	int		err;

	ASSERT(seg->s_as && AS_READ_HELD(seg->s_as, &seg->s_as->a_lock));

	while (npg-- > 0) {
		err = segvpix_page_equiv(seg, vpage_from++, rpage_to++);
		if (err) {
			npages -= npg;
			segvpix_range_equiv(seg, vpage_from - npages,
						    vpage_from - npages,
						    npages);
			return (err);
		}
	}

	return (0);
}


/*
 * Scan a range of memory for modified bits.
 * (Returns 0 if none of the pages are modified. -- see v86.c)
 */
int
segvpix_modscan(seg, vpage, npages)
	register struct seg	*seg;
	u_int		vpage;
	u_int		npages;
{
	struct segvpix_data *svd = (struct segvpix_data *)seg->s_data;
	int		retval = 0;
	u_char		refmod = 0;
	caddr_t		addr = (caddr_t)mmu_ptob(vpage);
	int		bit = 1;

	ASSERT(seg->s_as && AS_READ_HELD(seg->s_as, &seg->s_as->a_lock));

	for (; npages-- > 0; addr += PAGESIZE, bit <<= 1) {
		hat_getstat(seg->s_as, addr, PAGESIZE, svd->hatid,
			(caddr_t)&refmod, 1);
		/*
		 * hat_getstat() packs the rm bits for upto 4 pages in 1 byte
		 * in the order from left to right (i.e page 1 in bits 6-7,
		 * page 2 in bits 5-4, etc.)
		 */
		if (refmod & 0x40) { /* page is modified */
			retval |= bit;
		}
	}

	return (retval);
}

/*
 * Dump the pages belonging to this segvpix segment.
 */
static void
segvpix_dump(seg)
	struct seg *seg;
{
	struct segvpix_data *svd;
	page_t *pp;
	struct vnode *vp;
	u_offset_t off;
	u_int pfn;
	u_int page, npages;
	vpix_page_t *vpage;
	extern void dump_addpage(u_int);

	npages = seg_pages(seg);
	svd = (struct segvpix_data *)seg->s_data;
	vpage = &svd->vpage[0];

	if (vpage) {
		for (page = 0; page < npages; vpage++, page++) {
			int we_own_it = 0;

			if (!vpage->rp_phys && vpage->rp_anon)
				swap_xlate_nopanic(vpage->rp_anon, &vp, &off);

			/*
			 * If pp == NULL, the page either does not exist
			 * or is exclusively locked.  So determine if it
			 * exists before searching for it.
			 */

			if ((pp = page_lookup_nowait(vp, off, SE_SHARED)))
				we_own_it = 1;
			else if (page_exists(vp, off))
				pp = page_find(vp, off);

			if (pp) {
				pfn = page_pptonum(pp);
				dump_addpage(pfn);
				if (we_own_it)
					page_unlock(pp);
			}
		}
	}
}

/* ARGSUSED */
static int
segvpix_pagelock(struct seg *seg, caddr_t addr, u_int len,
    struct page ***ppp, enum lock_type type, enum seg_rw rw)
{
	return (ENOTSUP);
}

static int
segvpix_getmemid(struct seg *seg, caddr_t addr, memid_t *memidp)
{
	return (ENODEV);
}
#endif	/* _VPIX */
