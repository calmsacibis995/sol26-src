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
 * 	(c) 1986, 1987, 1988, 1989, 1990, 1993, 1996  Sun Microsystems, Inc
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 *
 */

#ident  "@(#)vm_anon.c 1.119     96/09/20 SMI"
/*	From:	SVr4.0	"kernel:vm/vm_anon.c	1.24"		*/

/*
 * VM - anonymous pages.
 *
 * This layer sits immediately above the vm_swap layer.  It manages
 * physical pages that have no permanent identity in the file system
 * name space, using the services of the vm_swap layer to allocate
 * backing storage for these pages.  Since these pages have no external
 * identity, they are discarded when the last reference is removed.
 *
 * An important function of this layer is to manage low-level sharing
 * of pages that are logically distinct but that happen to be
 * physically identical (e.g., the corresponding pages of the processes
 * resulting from a fork before one process or the other changes their
 * contents).  This pseudo-sharing is present only as an optimization
 * and is not to be confused with true sharing in which multiple
 * address spaces deliberately contain references to the same object;
 * such sharing is managed at a higher level.
 *
 * The key data structure here is the anon struct, which contains a
 * reference count for its associated physical page and a hint about
 * the identity of that page.  Anon structs typically live in arrays,
 * with an instance's position in its array determining where the
 * corresponding backing storage is allocated; however, the swap_xlate()
 * routine abstracts away this representation information so that the
 * rest of the anon layer need not know it.  (See the swap layer for
 * more details on anon struct layout.)
 *
 * In the future versions of the system, the association between an
 * anon struct and its position on backing store will change so that
 * we don't require backing store all anonymous pages in the system.
 * This is important for consideration for large memory systems.
 * We can also use this technique to delay binding physical locations
 * to anonymous pages until pageout/swapout time where we can make
 * smarter allocation decisions to improve anonymous klustering.
 *
 * Many of the routines defined here take a (struct anon **) argument,
 * which allows the code at this level to manage anon pages directly,
 * so that callers can regard anon structs as opaque objects and not be
 * concerned with assigning or inspecting their contents.
 *
 * Clients of this layer refer to anon pages indirectly.  That is, they
 * maintain arrays of pointers to anon structs rather than maintaining
 * anon structs themselves.  The (struct anon **) arguments mentioned
 * above are pointers to entries in these arrays.  It is these arrays
 * that capture the mapping between offsets within a given segment and
 * the corresponding anonymous backing storage address.
 */

#define	ANON_DEBUG

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mman.h>
#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/cpuvar.h>
#include <sys/swap.h>
#include <sys/cmn_err.h>
#include <sys/vtrace.h>
#include <sys/kmem.h>
#include <sys/sysmacros.h>
#include <sys/bitmap.h>
#include <sys/debug.h>
#include <sys/tnf_probe.h>

#include <vm/as.h>
#include <vm/hat.h>
#include <vm/anon.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/rm.h>

int anon_debug;

struct	k_anoninfo k_anoninfo;
ani_free_t	 ani_free_pool[ANI_MAX_POOL];

static int npagesteal;

/*
 * Global hash table for (vp, off) -> anon slot
 */
extern int swap_maxcontig;
int anon_hash_size;
struct anon **anon_hash;

static struct kmem_cache *anon_cache;
static struct kmem_cache *anonmap_cache;

/*ARGSUSED*/
static int
anonmap_cache_constructor(void *buf, void *cdrarg, int kmflags)
{
	struct anon_map *amp = buf;

	mutex_init(&amp->lock, "anon map lock", MUTEX_DEFAULT, NULL);
	mutex_init(&amp->serial_lock, "anon map serial lock", MUTEX_DEFAULT,
		NULL);
	return (0);
}

/*ARGSUSED1*/
static void
anonmap_cache_destructor(void *buf, void *cdrarg)
{
	struct anon_map *amp = buf;

	mutex_destroy(&amp->lock);
	mutex_destroy(&amp->serial_lock);
}

kmutex_t	anonhash_lock[AH_LOCK_SIZE];

void
anon_init()
{
	extern int physmem;
	char buf[100];
	int i;

	anon_hash_size = 1 << highbit(physmem / ANON_HASHAVELEN);

	for (i = 0; i < AH_LOCK_SIZE; i++) {
		(void) sprintf(buf, "ah_lock.%d", i);
		mutex_init(&anonhash_lock[i], buf, MUTEX_DEFAULT, NULL);
	}

	anon_hash = (struct anon **)
		kmem_zalloc(sizeof (struct anon *) * anon_hash_size, KM_SLEEP);
	anon_cache = kmem_cache_create("anon_cache", sizeof (struct anon),
		0, NULL, NULL, NULL, NULL, NULL, 0);
	anonmap_cache = kmem_cache_create("anonmap_cache",
		sizeof (struct anon_map), 0,
		anonmap_cache_constructor, anonmap_cache_destructor, NULL,
		NULL, NULL, 0);
	swap_maxcontig = 1024 * 1024 / PAGESIZE;	/* 1MB of pages */
}

/*
 * Global anon slot hash table manipulation.
 */

static void
anon_addhash(ap)
	struct anon *ap;
{
	u_int	index;

	ASSERT(MUTEX_HELD(&anonhash_lock[AH_LOCK(ap->an_vp, ap->an_off)]));
	index = ANON_HASH(ap->an_vp, ap->an_off);
	ap->an_hash = anon_hash[index];
	anon_hash[index] = ap;
}

static void
anon_rmhash(ap)
	struct anon *ap;
{
	struct anon **app;

	ASSERT(MUTEX_HELD(&anonhash_lock[AH_LOCK(ap->an_vp, ap->an_off)]));

	for (app = &anon_hash[ANON_HASH(ap->an_vp, ap->an_off)];
	    *app; app = &((*app)->an_hash)) {
		if (*app == ap) {
			*app = ap->an_hash;
			break;
		}
	}
}


/*
 * Called from clock handler to sync ani_free value.
 */

void
set_anoninfo()
{
	u_int	ix, total = 0;

	for (ix = 0; ix < ANI_MAX_POOL; ix++) {
		total += ani_free_pool[ix].ani_count;
	}
	k_anoninfo.ani_free = total;
}

/*
 * Reserve anon space.
 *
 * It's no longer simply a matter of incrementing ani_resv to
 * reserve swap space, we need to check memory-based as well
 * as disk-backed (physical) swap.  The following algorithm
 * is used:
 * 	Check the space on physical swap
 * 		i.e. amount needed < ani_max - ani_phys_resv
 * 	If we are swapping on swapfs check
 *		amount needed < (availrmem - swapfs_minfree)
 * Since the algorithm to check for the quantity of swap space is
 * almost the same as that for reserving it, we'll just use anon_resvmem
 * with a flag to decrement availrmem.
 *
 * Return non-zero on success.
 */
int
anon_resvmem(size, takemem)
	u_int size;
	u_int takemem;
{
	register int npages = btopr(size);
	register int mswap_pages;
	register int pswap_pages;
	extern int swapfs_minfree;

	mutex_enter(&anoninfo_lock);


	ASSERT(k_anoninfo.ani_max >= k_anoninfo.ani_phys_resv);

	/*
	 * pswap_pages is the number of pages we can take from
	 * physical (i.e. disk-backed) swap.
	 */
	pswap_pages = k_anoninfo.ani_max - k_anoninfo.ani_phys_resv;

	ANON_PRINT(A_RESV,
		("anon_resvmem: spages %d takemem %d pswap %d caller %x\n",
		npages, takemem, pswap_pages, (int)caller()));

	if (npages <= pswap_pages) {
		/*
		 * we have enough space on a physical swap
		 */
		if (takemem) {
			k_anoninfo.ani_phys_resv += npages;
		}
		mutex_exit(&anoninfo_lock);
		return (1);
	} else if (pswap_pages > 0) {
		/*
		 * we have some space on a physical swap
		 */
		if (takemem) {
			/*
			 * use up remainder of phys swap
			 */
			k_anoninfo.ani_phys_resv += pswap_pages;
			ASSERT(k_anoninfo.ani_phys_resv == k_anoninfo.ani_max);
		}
	}
	/*
	 * since (npages > pswap_pages) we need mem swap
	 * mswap_pages is the number of pages needed from availrmem
	 */
	mswap_pages = npages - pswap_pages;

	ANON_PRINT(A_RESV, ("anon_resvmem: need %d pages from memory\n",
	    mswap_pages));
	/*
	 * Since swapfs is available, we can use up to
	 * (availrmem - swapfs_minfree) bytes of physical memory
	 * as reservable swap space
	 */
	mutex_enter(&freemem_lock);
	if (availrmem < (swapfs_minfree + mswap_pages)) {
		/*
		 * Fail if not enough memory
		 */
		if (takemem) {
			k_anoninfo.ani_phys_resv -= pswap_pages;
		}
		mutex_exit(&freemem_lock);
		mutex_exit(&anoninfo_lock);
		ANON_PRINT(A_RESV,
		    ("anon_resvmem: not enough space from swapfs\n"));
		return (0);
	} else if (takemem) {
		/*
		 * Take the memory from the rest of the system.
		 */
		availrmem -= mswap_pages;
		k_anoninfo.ani_mem_resv += mswap_pages;
		mutex_exit(&freemem_lock);
		ANI_ADD(mswap_pages);
		ANON_PRINT((A_RESV | A_MRESV),
		    ("anon_resvmem: took %d pages of availrmem\n",
		    mswap_pages));
	} else {
		mutex_exit(&freemem_lock);
	}

	ASSERT(k_anoninfo.ani_max >= k_anoninfo.ani_phys_resv);
	mutex_exit(&anoninfo_lock);
	return (1);
}


/*
 * Give back an anon reservation.
 */
void
anon_unresv(size)
	u_int size;
{
	register u_int npages = btopr(size);
	register u_int mem_free_pages = 0;
	register u_int phys_free_slots;
	u_int mem_resv;


	mutex_enter(&anoninfo_lock);

	ASSERT(k_anoninfo.ani_mem_resv >= k_anoninfo.ani_locked_swap);
	/*
	 * If some of this reservation belonged to swapfs
	 * give it back to availrmem.
	 * ani_mem_resv is the amount of availrmem swapfs has allocated.
	 * but some of that memory could be locked by segspt so we can only
	 * return non locked ani_mem_resv back to availrmem
	 */
	if (k_anoninfo.ani_mem_resv > k_anoninfo.ani_locked_swap) {
		ANON_PRINT((A_RESV | A_MRESV),
		    ("anon_unresv: growing availrmem by %d pages\n",
		    MIN(k_anoninfo.ani_mem_resv, npages)));

		mem_free_pages = MIN((k_anoninfo.ani_mem_resv -
					k_anoninfo.ani_locked_swap), npages);
		mutex_enter(&freemem_lock);
		availrmem += mem_free_pages;
		mutex_exit(&freemem_lock);
		k_anoninfo.ani_mem_resv -= mem_free_pages;

		ANI_ADD(-mem_free_pages);
	}
	/*
	 * The remainder of the pages is returned to phys swap
	 */
	phys_free_slots = npages - mem_free_pages;

	if (phys_free_slots) {
	    k_anoninfo.ani_phys_resv -= phys_free_slots;
	}
	mem_resv = k_anoninfo.ani_mem_resv;

	ASSERT(k_anoninfo.ani_mem_resv >= k_anoninfo.ani_locked_swap);
	ASSERT(k_anoninfo.ani_max >= k_anoninfo.ani_phys_resv);

	mutex_exit(&anoninfo_lock);

	ANON_PRINT(A_RESV, ("anon_unresv: %d, tot %d, caller %x\n",
			npages, mem_resv, (int)caller()));
}

/*
 * Allocate an anon slot and return it with the lock held.
 */
struct anon *
anon_alloc(vp, off)
	struct vnode *vp;
	u_int off;
{
	register struct anon *ap;
	kmutex_t	*ahm;

	ap = kmem_cache_alloc(anon_cache, KM_SLEEP);
	if (vp == NULL) {
		swap_alloc(ap);
	} else {
		ap->an_vp = vp;
		ap->an_off = off;
	}
	ap->an_refcnt = 1;
	ap->an_pvp = NULL;
	ap->an_poff = (u_int)0;
	ahm = &anonhash_lock[AH_LOCK(ap->an_vp, ap->an_off)];
	mutex_enter(ahm);
	anon_addhash(ap);
	mutex_exit(ahm);
	ANI_ADD(-1);
	ANON_PRINT(A_ANON, ("anon_alloc: returning ap %x, vp %x\n",
		(int)ap, (int)(ap ? ap->an_vp : NULL)));
	return (ap);
}

/*
 * Decrement the reference count of an anon page.
 * If reference count goes to zero, free it and
 * its associated page (if any).
 */
void
anon_decref(ap)
	register struct anon *ap;
{
	register page_t *pp;
	struct vnode *vp;
	u_int off;
	kmutex_t *ahm;

	ahm = &anonhash_lock[AH_LOCK(ap->an_vp, ap->an_off)];
	mutex_enter(ahm);
	ASSERT(ap->an_refcnt != 0);
	if (ap->an_refcnt == 0)
		cmn_err(CE_PANIC, "anon_decref: slot count 0\n");
	if (--ap->an_refcnt == 0) {
		swap_xlate(ap, &vp, &off);
		mutex_exit(ahm);

		/*
		 * If there is a page for this anon slot we will need to
		 * call VN_DISPOSE to get rid of the vp association and
		 * put the page back on the free list as really free.
		 * Acquire the "exclusive" lock to ensure that any
		 * pending i/o always completes before the swap slot
		 * is freed.
		 */
		pp = page_lookup(vp, (u_offset_t)off, SE_EXCL);

		/*
		 * If there was a page, we've synchronized on it (getting
		 * the exclusive lock is as good as gettting the iolock)
		 * so now we can free the physical backing store. Also, this
		 * is where we would free the name of the anonymous page
		 * (swap_free(ap)), a no-op in the current implementation.
		 */
		mutex_enter(ahm);
		ASSERT(ap->an_refcnt == 0);
		anon_rmhash(ap);
		if (ap->an_pvp)
			swap_phys_free(ap->an_pvp, ap->an_poff, PAGESIZE);
		mutex_exit(ahm);

		if (pp != NULL) {
			/*LINTED: constant in conditional context */
			VN_DISPOSE(pp, B_INVAL, 0, kcred);
		}
		ANON_PRINT(A_ANON, ("anon_decref: free ap %x, vp %x\n",
		    (int)ap, (int)ap->an_vp));
		kmem_cache_free(anon_cache, ap);

		ANI_ADD(1);
	} else {
		mutex_exit(ahm);
	}
}

/*
 * Duplicate references to size bytes worth of anon pages.
 * Used when duplicating a segment that contains private anon pages.
 * This code assumes that procedure calling this one has already used
 * hat_chgprot() to disable write access to the range of addresses that
 * that *old actually refers to.
 */
void
anon_dup(old, new, size)
	register struct anon **old, **new;
	u_int size;
{
	register int i;
	kmutex_t *ahm;

	i = btopr(size);
	while (i-- > 0) {
		if ((*new = *old) != NULL) {
			ahm = &anonhash_lock[AH_LOCK((*old)->an_vp,
			    (*old)->an_off)];
			mutex_enter(ahm);
			(*new)->an_refcnt++;
			mutex_exit(ahm);
		}
		old++;
		new++;
	}
}

/*
 * Free a group of "size" anon pages, size in bytes,
 * and clear out the pointers to the anon entries.
 */
void
anon_free(app, size)
	register struct anon **app;
	u_int size;
{
	register u_int i;
	struct anon *ap;

	i = (u_int)btopr(size);

	/*
	 * Large Files: The following is the assertion to validate the
	 * above cast.
	 */

	ASSERT(btopr(size) <= UINT_MAX);

	while (i-- > 0) {
		if ((ap = *app) != NULL) {
			*app = NULL;
			anon_decref(ap);
		}
		app++;
	}
}

/*
 * Return the kept page(s) and protections back to the segment driver.
 */
int
anon_getpage(app, protp, pl, plsz, seg, addr, rw, cred)
	struct anon **app;
	u_int *protp;
	page_t *pl[];
	u_int plsz;
	struct seg *seg;
	caddr_t addr;
	enum seg_rw rw;
	struct cred *cred;
{
	register page_t *pp;
	register struct anon *ap = *app;
	struct vnode *vp;
	u_int off;
	int err;
	kmutex_t *ahm;

	swap_xlate(ap, &vp, &off);

	/*
	 * Lookup the page. If page is being paged in,
	 * wait for it to finish as we must return a list of
	 * pages since this routine acts like the VOP_GETPAGE
	 * routine does.
	 */
	if (pl != NULL && (pp = page_lookup(vp, (u_offset_t)off, SE_SHARED))) {
		ahm = &anonhash_lock[AH_LOCK(ap->an_vp, ap->an_off)];
		mutex_enter(ahm);
		if (ap->an_refcnt == 1)
			*protp = PROT_ALL;
		else
			*protp = PROT_ALL & ~PROT_WRITE;
		mutex_exit(ahm);
		pl[0] = pp;
		pl[1] = NULL;
		return (0);
	}

	/*
	 * Simply treat it as a vnode fault on the anon vp.
	 */

	TRACE_3(TR_FAC_VM, TR_ANON_GETPAGE,
		"anon_getpage:seg %x addr %x vp %x",
		seg, addr, vp);

	err = VOP_GETPAGE(vp, (offset_t)off, PAGESIZE, protp, pl, plsz,
	    seg, addr, rw, cred);

	if (err == 0 && pl != NULL) {
		ahm = &anonhash_lock[AH_LOCK(ap->an_vp, ap->an_off)];
		mutex_enter(ahm);
		if (ap->an_refcnt != 1)
			*protp &= ~PROT_WRITE;	/* make read-only */
		mutex_exit(ahm);
	}
	return (err);
}

/*
 * Turn a reference to an object or shared anon page
 * into a private page with a copy of the data from the
 * original page which is always locked by the caller.
 * This routine unloads the translation and unlocks the
 * original page, if it isn't being stolen, before returning
 * to the caller.
 *
 * NOTE:  The original anon slot is not freed by this routine
 *	  It must be freed by the caller while holding the
 *	  "anon_map" lock to prevent races which can occur if
 *	  a process has multiple lwps in its address space.
 */
page_t *
anon_private(app, seg, addr, opp, oppflags, cred)
	struct anon **app;
	struct seg *seg;
	caddr_t addr;
	page_t *opp;
	u_int oppflags;
	struct cred *cred;
{
	register struct anon *old = *app;
	register struct anon *new;
	register page_t *pp;
	struct vnode *vp;
	u_int off;
	page_t *anon_pl[1 + 1];
	int err;

	if (oppflags & STEAL_PAGE)
		ASSERT(se_excl_assert(&opp->p_selock));
	else
		ASSERT(se_assert(&opp->p_selock));

	CPU_STAT_ADD_K(cpu_vminfo.cow_fault, 1);

	/* Kernel probe */
	TNF_PROBE_1(anon_private, "vm pagefault", /* CSTYLED */,
		tnf_opaque,	address,	addr);

	*app = new = anon_alloc(NULL, (u_int)0);
	swap_xlate(new, &vp, &off);

	if (oppflags & STEAL_PAGE) {
		page_rename(opp, vp, (u_offset_t)off);
		pp = opp;
		TRACE_5(TR_FAC_VM, TR_ANON_PRIVATE,
			"anon_private:seg %x addr %x pp %x vp %x off %llx",
			seg, addr, pp, vp, off);
		hat_setmod(pp);
		/*
		 * If original page is ``locked'', transfer
		 * the lock from a "cowcnt" to a "lckcnt" since
		 * we know that it is not a private page.
		 */
		if (oppflags & LOCK_PAGE)
			page_pp_useclaim(pp, pp, 1);
		npagesteal++;
		return (pp);
	}

	/*
	 * Call the VOP_GETPAGE routine to create the page, thereby
	 * enabling the vnode driver to allocate any filesystem
	 * space (e.g., disk block allocation for UFS).  This also
	 * prevents more than one page from being added to the
	 * vnode at the same time.
	 */
	err = VOP_GETPAGE(vp, (offset_t)off, PAGESIZE, (u_int *)NULL,
	    anon_pl, PAGESIZE, seg, addr, S_CREATE, cred);
	if (err) {
		*app = old;
		anon_decref(new);
		page_unlock(opp);
		return ((page_t *)NULL);
	}
	pp = anon_pl[0];

	/*
	 * Now copy the contents from the original page,
	 * which is locked and loaded in the MMU by
	 * the caller to prevent yet another page fault.
	 */
	ppcopy(opp, pp);		/* XXX - should set mod bit in here */

	hat_setrefmod(pp);		/* mark as modified */

	/*
	 * If the original page was locked, we need
	 * to move the lock to the new page.
	 * If we did not have an anon page before,
	 * page_pp_useclaim should expect opp
	 * to have a cowcnt instead of lckcnt.
	 */
	if (oppflags & LOCK_PAGE)
		page_pp_useclaim(opp, pp, old == NULL);

	/*
	 * Unload the old translation.
	 */
	hat_unload(seg->s_as->a_hat, addr, PAGESIZE, HAT_UNLOAD);

	/*
	 * Ok, now release the lock on the original page,
	 * or else the process will sleep forever in
	 * anon_decref() waiting for the "exclusive" lock
	 * on the page.
	 */
	page_unlock(opp);

	/*
	 * we are done with page creation so downgrade the new
	 * page's selock to shared, this helps when multiple
	 * as_fault(...SOFTLOCK...) are done to the same
	 * page(aio)
	 */
	page_downgrade(pp);

	/*
	 * NOTE:  The original anon slot must be freed by the
	 * caller while holding the "anon_map" lock, if we
	 * copied away from an anonymous page.
	 */
	return (pp);
}

/*
 * Allocate a private zero-filled anon page.
 */
page_t *
anon_zero(seg, addr, app, cred)
	struct seg *seg;
	caddr_t addr;
	struct anon **app;
	struct cred *cred;
{
	register struct anon *ap;
	register page_t *pp;
	struct vnode *vp;
	u_int off;
	page_t *anon_pl[1 + 1];
	int err;

	/* Kernel probe */
	TNF_PROBE_1(anon_zero, "vm pagefault", /* CSTYLED */,
		tnf_opaque,	address,	addr);

	*app = ap = anon_alloc(NULL, (u_int)0);
	swap_xlate(ap, &vp, &off);

	/*
	 * Call the VOP_GETPAGE routine to create the page, thereby
	 * enabling the vnode driver to allocate any filesystem
	 * dependent structures (e.g., disk block allocation for UFS).
	 * This also prevents more than on page from being added to
	 * the vnode at the same time since it is locked.
	 */
	err = VOP_GETPAGE(vp, (offset_t)off, PAGESIZE, (u_int *)NULL,
	    anon_pl, PAGESIZE, seg, addr, S_CREATE, cred);
	if (err) {
		*app = NULL;
		anon_decref(ap);
		return ((page_t *)NULL);
	}
	pp = anon_pl[0];

	pagezero(pp, 0, PAGESIZE);	/* XXX - should set mod bit */
	page_downgrade(pp);
	CPU_STAT_ADD_K(cpu_vminfo.zfod, 1);
	hat_setrefmod(pp);	/* mark as modified so pageout writes back */
	return (pp);
}

/*
 * Allocate array of private zero-filled anon pages for empty slots
 * and kept pages for non empty slots within given range.
 *
 * NOTE: This rountine will try and use large pages
 *       if available and supported by underlying platform.
 */
int
anon_map_getpages(struct anon_map *amp, u_int start_index, size_t len,
			page_t *ppa[], struct seg *seg, caddr_t addr,
			enum seg_rw rw, struct cred *cred)
{

	struct anon	*ap, **app, **app_start;
	struct vnode	*ap_vp;
	page_t		*pp, *pplist, *anon_pl[1 + 1];
	int		p_index, index, err = 0;
	u_int		l_szc, szc, npgs, prot, pg_cnt;
	u_int ap_off;
	size_t		pgsz;


	/*
	 * XXX For now only handle S_CREATE.
	 */
	ASSERT(rw == S_CREATE);

	app_start = &amp->anon[start_index];
	index	= start_index;
	p_index	= 0;
	npgs	= btopr(len);

	/*
	 * If this platform supports mulitple page sizes
	 * then try and allocate directly from the free
	 * list for pages larger than PAGESIZE.
	 *
	 * NOTE:When we have page_create_ru we can stop
	 *	directly allocating from the freelist.
	 */
	l_szc  = page_num_pagesizes() - 1;
	mutex_enter(&amp->serial_lock);
	while (npgs) {

		/*
		 * if anon slot already exists
		 *   (means page has been created)
		 * so 1) look up the page
		 *    2) if the page is still in memory, get it.
		 *    3) if not, create a page and
		 *	  page in from physical swap device.
		 * These are done in anon_getpage().
		 */
		app = &amp->anon[index];
		if (*app) {
			err = anon_getpage(app, &prot, anon_pl, PAGESIZE,
					seg, addr, S_READ, cred);
			if (err) {
				mutex_exit(&amp->serial_lock);
				cmn_err(CE_PANIC,
					"anon_map_getpages: anon_getpage");
			}
			pp = anon_pl[0];
			ppa[p_index++] = pp;

			addr += PAGESIZE;
			index++;
			npgs--;
			continue;
		}

		/*
		 * Now try and allocate the largest page possible
		 * for the current address and range.
		 * Keep dropping down in page size until:
		 *
		 *	1) Properly aligned
		 *	2) Does not overlap existing anon pages
		 *	3) Fits in remaining range.
		 *	4) able to allocate one.
		 *
		 * NOTE: XXX When page_create_ru is completed this code
		 *	 will change.
		 */
		szc    = l_szc;
		pplist = NULL;
		pg_cnt = 0;
		while (szc) {
			pgsz	= page_get_pagesize(szc);
			pg_cnt	= pgsz / PAGESIZE;
			if (IS_P2ALIGNED(addr, pgsz) && pg_cnt <= npgs &&
				anon_pages(app_start, index, pg_cnt) ==
					(u_int)0) {

				/*
				 * XXX
				 * Since we are faking page_create()
				 * we also need to do the freemem and
				 * pcf accounting.
				 */
				(void) page_create_wait(pg_cnt, PG_WAIT);

				pplist = page_get_freelist(
						(struct vnode *)NULL,
							(u_offset_t)0,
						seg->s_as, addr, pgsz, 0);

				if (pplist == NULL) {
					page_create_putback(pg_cnt);
				}

				/*
				 * If a request for a page of size
				 * larger than PAGESIZE failed
				 * then don't try that size anymore.
				 */
				if (pplist == NULL) {
					l_szc = szc - 1;
				} else {
					break;
				}
			}
			szc--;
		}

		/*
		 * If just using PAGESIZE pages then don't
		 * directly allocate from the free list.
		 */
		if (pplist == NULL) {
			ASSERT(szc == 0);
			pp = anon_zero(seg, addr, &ap, cred);
			if (pp == NULL) {
				mutex_exit(&amp->serial_lock);
				cmn_err(CE_PANIC,
					"anon_map_getpages: anon_zero");
			}
			ppa[p_index++] = pp;

			mutex_enter(&amp->lock);
			ASSERT(*app == NULL);
			*app = ap;
			mutex_exit(&amp->lock);

			addr += PAGESIZE;
			index++;
			npgs--;
			continue;
		}

		/*
		 * pplist is a list of pg_cnt PAGESIZE pages.
		 * These pages are locked SE_EXCL since they
		 * came directly off the free list.
		 */
		while (pg_cnt--) {

			ap = anon_alloc(NULL, (u_int)0);
			swap_xlate(ap, &ap_vp, &ap_off);

			ASSERT(pplist != NULL);
			pp = pplist;
			page_sub(&pplist, pp);
			PP_CLRFREE(pp);
			PP_CLRAGED(pp);

			if (!page_hashin(pp, ap_vp, (u_offset_t)ap_off, NULL)) {
				mutex_exit(&amp->serial_lock);
				cmn_err(CE_PANIC,
					"anon_map_getpages page_hashin");
			}
			pagezero(pp, 0, PAGESIZE);
			CPU_STAT_ADD_K(cpu_vminfo.zfod, 1);
			page_downgrade(pp);

			err = VOP_GETPAGE(ap_vp, ap_off, PAGESIZE,
					(u_int *)NULL, anon_pl, PAGESIZE, seg,
					addr, S_WRITE, cred);
			if (err) {
				mutex_exit(&amp->serial_lock);
				cmn_err(CE_PANIC, "anon_map_getpages: S_WRITE");
			}

			/*
			 * Unlock page once since it came off the
			 * freelist locked. The call to VOP_GETPAGE
			 * will leave it locked SHARED. Also mark
			 * as modified so pageout writes back.
			 */
			page_unlock(pp);
			hat_setrefmod(pp);

			mutex_enter(&amp->lock);
			ASSERT(amp->anon[index] == NULL);
			amp->anon[index] = ap;
			mutex_exit(&amp->lock);
			ppa[p_index++] = pp;

			addr += PAGESIZE;
			index++;
			npgs--;
		}
	}
	mutex_exit(&amp->serial_lock);
	return (0);
}

/*
 * Allocate and initialize an anon_map structure for seg
 * associating the given swap reservation with the new anon_map.
 */
struct anon_map *
anonmap_alloc(size, swresv)
	u_int size;
	u_int swresv;
{
	struct anon_map *amp;		/* XXX - For locknest */

	amp = kmem_cache_alloc(anonmap_cache, KM_SLEEP);

	amp->refcnt = 1;
	amp->size = size;
	amp->anon = (struct anon **)
	    kmem_zalloc((u_int)btopr(size) * sizeof (struct anon *), KM_SLEEP);
	amp->swresv = swresv;
	return (amp);
}

void
anonmap_free(amp)
	struct anon_map *amp;
{
	ASSERT(amp->anon);
	ASSERT(amp->refcnt == 0);

	kmem_free((caddr_t)amp->anon,
	    btopr(amp->size) * sizeof (struct anon *));
	kmem_cache_free(anonmap_cache, amp);
}

/*
 * Returns true if the app array has some empty slots.
 * The offp and lenp paramters are in/out paramters.  On entry
 * these values represent the starting offset and length of the
 * mapping.  When true is returned, these values may be modified
 * to be the largest range which includes empty slots.
 */
int
non_anon(app, offp, lenp)
	register struct anon **app;
	u_offset_t *offp;
	u_int *lenp;
{
	register int i, el;
	int low, high;

	low = -1;
	for (i = 0, el = *lenp; i < el; i += PAGESIZE) {
		if (*app++ == NULL) {
			if (low == -1)
				low = i;
			high = i;
		}
	}
	if (low != -1) {
		/*
		 * Found at least one non-anon page.
		 * Set up the off and len return values.
		 */
		if (low != 0)
			*offp += low;
		*lenp = high - low + PAGESIZE;
		return (1);
	}
	return (0);
}


/*
 * Return a count of the number of existing anon pages in the anon array
 * app in the range (off, off+len). The array and slots must be guaranteed
 * stable by the caller.
 */
u_int
anon_pages(app, anon_index, nslots)
	struct anon **app;
	u_int anon_index;
	u_int nslots;
{
	struct anon **capp, **eapp;
	int cnt = 0;

	for (capp = app + anon_index, eapp = capp + nslots;
			capp < eapp; capp++) {
		if (*capp != NULL)
			cnt++;
	}
	return (ptob((u_int)cnt));
}

/*
 * Move reserved phys swap into memory swap (unreserve phys swap
 * and reserve mem swap by the same amount).
 * Used by segspt when it needs to lock resrved swap npages in memory
 */
int
anon_swap_adjust(u_int npages)
{
	u_int unlocked_mem_swap;

	mutex_enter(&anoninfo_lock);

	ASSERT(k_anoninfo.ani_mem_resv >= k_anoninfo.ani_locked_swap);
	ASSERT(k_anoninfo.ani_max >= k_anoninfo.ani_phys_resv);

	unlocked_mem_swap = k_anoninfo.ani_mem_resv
					- k_anoninfo.ani_locked_swap;
	if (npages > unlocked_mem_swap) {
		u_int adjusted_swap = (npages - unlocked_mem_swap);
		/*
		 * if there is not enough unlocked mem swap we take missing
		 * amount from phys swap and give it to mem swap
		 */
		mutex_enter(&freemem_lock);
		if (availrmem < adjusted_swap + swapfs_minfree) {
			mutex_exit(&freemem_lock);
			mutex_exit(&anoninfo_lock);
			return (ENOMEM);
		}
		availrmem -= adjusted_swap;
		mutex_exit(&freemem_lock);

		k_anoninfo.ani_mem_resv += adjusted_swap;
		k_anoninfo.ani_phys_resv -= adjusted_swap;

		ANI_ADD(adjusted_swap);
	}
	k_anoninfo.ani_locked_swap += npages;

	ASSERT(k_anoninfo.ani_mem_resv >= k_anoninfo.ani_locked_swap);
	ASSERT(k_anoninfo.ani_max >= k_anoninfo.ani_phys_resv);

	mutex_exit(&anoninfo_lock);

	return (0);
}

/*
 * 'unlocked' reserved mem swap so when it is unreserved it
 * can be moved back phys (disk) swap
 */
void
anon_swap_restore(u_int npages)
{
	mutex_enter(&anoninfo_lock);

	ASSERT(k_anoninfo.ani_locked_swap <= k_anoninfo.ani_mem_resv);

	k_anoninfo.ani_locked_swap -= npages;

	ASSERT(k_anoninfo.ani_locked_swap <= k_anoninfo.ani_mem_resv);

	mutex_exit(&anoninfo_lock);
}
