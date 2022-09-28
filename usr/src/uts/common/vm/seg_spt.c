/*
 * Copyright (c) 1993,1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident   "@(#)seg_spt.c 1.26     96/10/23 SMI"

#include <sys/param.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <sys/kmem.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <vm/hat.h>
#include <vm/seg.h>
#include <vm/as.h>
#include <vm/anon.h>
#include <vm/page.h>
#include <sys/buf.h>
#include <sys/swap.h>
#include <vm/seg_spt.h>
#include <sys/debug.h>
#include <sys/vtrace.h>

#include <sys/tnf_probe.h>

#define	SEGSPTADDR	(caddr_t)0x0

/*
 * # pages used for spt
 */

extern kmutex_t	spt_lock;
static u_int	spt_used;

extern int	maxmem;

static int segspt_create(struct seg *seg, caddr_t argsp);
static int segspt_unmap(struct seg *seg, caddr_t raddr, u_int ssize);
static void segspt_free(struct seg *seg);
static int segspt_lockop(struct seg *seg, caddr_t addr, u_int len, int attr,
    int op, ulong *lockmap, size_t pos);
static int segspt_badops();
static void segspt_free_pages(struct seg *seg, caddr_t addr, u_int len);
static int segspt_kluster(struct seg *seg, caddr_t addr, int delta);

struct seg_ops segspt_ops = {

	segspt_badops,
	segspt_unmap,
	segspt_free,
	segspt_badops,
	segspt_badops,
	segspt_badops,
	segspt_badops,
	segspt_kluster,
	(u_int (*)()) NULL,	/* swapout */
	segspt_badops,
	segspt_badops,
	segspt_lockop,
	segspt_badops,
	(u_offset_t (*)()) segspt_badops,
	segspt_badops,
	segspt_badops,
	segspt_badops,		/* advise */
	(void (*)()) segspt_badops,
	segspt_badops,
};

static int segspt_shmdup(struct seg *seg, struct seg *newseg);
static int segspt_shmunmap(struct seg *seg, caddr_t raddr, u_int ssize);
static void segspt_shmfree(struct seg *seg);
static faultcode_t segspt_shmfault(struct hat *hat, struct seg *seg,
		caddr_t addr, u_int len, enum fault_type type, enum seg_rw rw);
static int segspt_shmsetprot(register struct seg *seg, register caddr_t addr,
			register u_int len, register u_int prot);
static int segspt_shmcheckprot(struct seg *seg, caddr_t addr, u_int size,
			u_int prot);
static int segspt_shmincore(struct seg *seg, caddr_t addr, u_int len,
			register char *vec);
static int segspt_shmsync(struct seg *seg, register caddr_t addr, u_int len,
			int attr, u_int flags);
static int segspt_shmlockop(struct seg *seg, caddr_t addr, u_int len, int attr,
			int op, ulong *lockmap, size_t pos);
static int segspt_shmgetprot(struct seg *seg, caddr_t addr, u_int len,
			u_int *protv);
static u_offset_t segspt_shmgetoffset(struct seg *seg, caddr_t addr);
static int segspt_shmgettype(struct seg *seg, caddr_t addr);
static int segspt_shmgetvp(struct seg *seg, caddr_t addr, struct vnode **vpp);
static int segspt_shmadvice(struct seg *seg, caddr_t addr, u_int len,
			int behav);
static void segspt_shmdump(struct seg *seg);
static int segspt_shmpagelock(struct seg *, caddr_t, u_int,
			struct page ***, enum lock_type, enum seg_rw);
static int segspt_shmgetmemid(struct seg *, caddr_t, memid_t *);

struct seg_ops segspt_shmops = {
	segspt_shmdup,
	segspt_shmunmap,
	segspt_shmfree,
	segspt_shmfault,
	(faultcode_t (*)()) NULL,
	segspt_shmsetprot,
	segspt_shmcheckprot,
	(int (*)()) NULL,
	(u_int (*)()) NULL,
	segspt_shmsync,
	segspt_shmincore,
	segspt_shmlockop,
	segspt_shmgetprot,
	segspt_shmgetoffset,
	segspt_shmgettype,
	segspt_shmgetvp,
	segspt_shmadvice,	/* advise */
	segspt_shmdump,
	segspt_shmpagelock,
	segspt_shmgetmemid,
};

/* ARGSUSED */
int
sptcreate(u_int size, struct seg **sptseg, struct anon_map *amp)
{
	int err;
	struct as	*newas;
	struct	segspt_crargs sptcargs;

#ifdef DEBUG
	TNF_PROBE_1(sptcreate, "spt", /* CSTYLED */,
                	tnf_ulong, size, size );
#endif

	if (!hat_supported(HAT_SHARED_PT, (void *)0))
		return (EINVAL);
	/*
	 * get a new as for this shared memory segment
	 */
	newas = as_alloc();
	sptcargs.amp = amp;

	/*
	 * create a shared page table (spt) segment
	 */

	if (err = as_map(newas, SEGSPTADDR, size, segspt_create,
					(caddr_t)&sptcargs)) {
		as_free(newas);
		return (err);
	}
	*sptseg = sptcargs.seg_spt;
	return (0);
}

void
sptdestroy(struct as *as, struct anon_map *amp)
{

#ifdef DEBUG
	TNF_PROBE_0(sptdestroy, "spt", /* CSTYLED */);
#endif
	as_unmap(as, SEGSPTADDR, amp->size);
	as_free(as);

	TRACE_2(TR_FAC_VM, TR_ANON_SHM, "anon shm: as %x, amp %x", as, amp);
}

/*
 * called from seg_free().
 * free (i.e., unlock, unmap, return to free list)
 *  all the pages in the given seg.
 */
void
segspt_free(struct seg	*seg)
{
	struct spt_data *spt = (struct spt_data *)seg->s_data;

	TRACE_1(TR_FAC_VM, TR_ANON_SHM, "anon shm: seg %x", seg);

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	if (spt != NULL) {
		if (spt->realsize)
			segspt_free_pages(seg, seg->s_base, spt->realsize);

		kmem_free((caddr_t)spt->vp, sizeof (struct vnode));

		kmem_free(spt->ppa, ((sizeof (struct page_t *)) *
						(spt->amp->size/PAGESIZE)));
		kmem_free((caddr_t)spt, sizeof (struct spt_data));
	}
}

/* ARGSUSED */
static int
segspt_shmsync(struct seg *seg, caddr_t addr, u_int len, int attr, u_int flags)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	return (0);
}

/*
 * segspt pages are always "in core" since the memory is locked down.
 */
/* ARGSUSED */
static int
segspt_shmincore(struct seg *seg, caddr_t addr, u_int len, char *vec)
{

	caddr_t eo_seg;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));
#ifdef lint
	seg = seg;
#endif

	eo_seg = addr + len;
	while (addr < eo_seg) {
		/* page exist, and it's locked. */
		*vec++ = (char)0x9;
		addr += PAGESIZE;
	}
	return (len);
}

/*
 * called from as_ctl(, MC_LOCK,)
 *
 */
/* ARGSUSED */
static int
segspt_lockop(struct seg *seg, caddr_t addr, u_int len, int attr,
			    int op, ulong *lockmap, size_t pos)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));
	/*
	 * for spt, as->a_paglck is never set
	 * so this routine should not be called.
	 * XXX Should this be a BADOP?
	 */
	return (0);
}

static int
segspt_unmap(struct seg *seg, caddr_t raddr, u_int ssize)
{
	u_int share_size;

	TRACE_3(TR_FAC_VM, TR_ANON_SHM, "anon shm: seg %x, addr %x, size %x",
	    seg, raddr, ssize);

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * seg.s_size may have been rounded up to the largest page size
	 * in shmat().
	 * XXX This should be cleanedup. sptdestroy should take a length
	 * argument which should be the same as sptcreate. Then
	 * this rounding would not be needed (or is done in shm.c)
	 * Only the check for full segment will be needed.
	 */
	share_size = page_get_pagesize(page_num_pagesizes() - 1);
	ssize = roundup(ssize, share_size);

	if (raddr == seg->s_base && ssize == seg->s_size) {
		seg_free(seg);
		return (0);
	} else
		return (EINVAL);
}

int
segspt_badops()
{
	cmn_err(CE_PANIC, "segspt_badops is called");
	return (0);
}

int
segspt_create(struct seg *seg, caddr_t argsp)
{
	int		err = ENOMEM;
	u_int		len  = seg->s_size;
	caddr_t		addr = seg->s_base;
	struct spt_data *spt;
	struct 	segspt_crargs *sptcargs = (struct segspt_crargs *)argsp;
	struct anon_map *amp = sptcargs->amp;
	struct	cred	*cred;
	u_int		i, j, npages, anon_index;
	struct vnode	*vp;

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

#ifdef DEBUG
	TNF_PROBE_2(segspt_create, "spt", /* CSTYLED */,
                                tnf_opaque, addr, addr,
				tnf_ulong, len, len);
#endif

	npages = btopr(amp->size);

	if (err = anon_swap_adjust(npages)) {
		return (err);
	}
	err = ENOMEM;

	if ((spt = kmem_zalloc(sizeof (struct spt_data), KM_NOSLEEP)) == NULL)
		goto out1;

	if ((spt->ppa = kmem_zalloc(((sizeof (struct page_t *)) * npages),
	    KM_NOSLEEP)) == NULL)
		goto out2;

	if ((vp = kmem_zalloc(sizeof (struct vnode), KM_NOSLEEP)) == NULL)
		goto out3;

	seg->s_ops = &segspt_ops;
	spt->vp = vp;
	spt->amp = amp;
	seg->s_data = (caddr_t)spt;

	/*
	 * get array of pages for each anon slot in amp
	 */
	cred = CRED();
	anon_index = 0;

	if ((err = anon_map_getpages(amp, anon_index, ptob(npages), spt->ppa,
	    seg, addr, S_CREATE, cred)) != 0)
		goto out4;

	/*
	 * addr is initial address corresponding to the first page on ppa list
	 */
	for (i = 0; i < npages; i++) {
		/* attempt to lock all pages */
		if (!page_pp_lock(spt->ppa[i], 0, 1)) {
			/*
			 * if unable to lock any page, unlock all
			 * of them and return error
			 */
			for (j = 0; j < i; j++)
				page_pp_unlock(spt->ppa[j], 0, 1);
			for (i = 0; i < npages; i++) {
				page_unlock(spt->ppa[i]);
			}
			err = ENOMEM;
			goto out4;
		}
	}
	hat_memload_array(seg->s_as->a_hat, addr, (size_t)ptob(npages),
	    spt->ppa, PROT_ALL, HAT_LOAD_LOCK | HAT_LOAD_SHARE);

	spt->realsize = ptob(npages);
	atomic_add_word(&spt_used, npages, &spt_lock);
	sptcargs->seg_spt = seg;
	return (0);

out4:
	seg->s_data = NULL;
	kmem_free(vp, sizeof (struct vnode));
out3:
	kmem_free(spt->ppa, ((sizeof (struct page_t *)) * npages));
out2:
	kmem_free(spt, sizeof (struct spt_data));
out1:
	anon_swap_restore(npages);
	return (err);
}

/* ARGSUSED */
void
segspt_free_pages(struct seg *seg, caddr_t addr, u_int len)
{
	struct page 	*pp;
	struct spt_data *spt = (struct spt_data *)seg->s_data;
	u_int		npages, n;
	struct page	**ppa = spt->ppa;

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	TRACE_3(TR_FAC_VM, TR_ANON_SHM, "anon shm: seg %x, addr %x, spt %x",
	    seg, addr, spt);

	len = roundup(len, PAGESIZE);
	npages = btop(len);

	hat_unload(seg->s_as->a_hat, addr, len, HAT_UNLOAD_UNLOCK);

	n = npages;
	while (n--) {
		pp = *ppa++;
		page_pp_unlock(pp, 0, 1);
		page_unlock(pp);
	}

	/*
	 * mark that pages have been released
	 */
	spt->realsize = 0;
	atomic_add_word(&spt_used, -((int)npages), &spt_lock);

	anon_swap_restore(npages);
}

/*
 * return locked pages over a given range. Segspt pages are already
 * locked so we index into an pp array and return first page
 * corresponding to the addr
 */
/* ARGSUSED */
static int
segspt_shmpagelock(struct seg *seg, caddr_t addr, u_int len,
		struct page ***ppp, enum lock_type type, enum seg_rw rw)
{
	struct sptshm_data *ssd = (struct sptshm_data *)seg->s_data;
	u_int		page_index, npages, total_pages;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	page_index = seg_page(seg, addr);
	npages = btopr(len);
	total_pages = btopr(ssd->amp->size);

	/*
	 * check if the request is larger then number of pages stored
	 * in page array
	 */
	if (page_index + npages > total_pages)
		return (EFAULT);

	if (type == L_PAGEUNLOCK) {
		/* pages will be unlocked in segspt_free_pages() */

		atomic_add_word((u_int *)&ssd->softlockcnt, -npages, &spt_lock);

		if (ssd->softlockcnt == 0) {
			if (AS_ISUNMAPWAIT(seg->s_as)) {
				mutex_enter(&seg->s_as->a_contents);
				if (AS_ISUNMAPWAIT(seg->s_as)) {
					AS_CLRUNMAPWAIT(seg->s_as);
					cv_broadcast(&seg->s_as->a_cv);
				}
				mutex_exit(&seg->s_as->a_contents);
			}
		}
		return (0);
	} else if (type == L_PAGELOCK) {
		atomic_add_word((u_int *)&ssd->softlockcnt, npages, &spt_lock);
		*ppp = &ssd->ppa[page_index];
	}
	return (0);
}

int
segspt_shmattach(struct seg *seg, caddr_t *argsp)
{
	struct sptshm_data *sptarg = (struct sptshm_data *)argsp;
	struct sptshm_data *ssd;
	struct anon_map *shm_amp = sptarg->amp;
	struct spt_data	*spt_sd;
	int error;

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	ssd = (struct sptshm_data *)kmem_zalloc((sizeof (struct sptshm_data)),
						KM_NOSLEEP);
	if (ssd == NULL)
		return (ENOMEM);

	spt_sd = sptarg->sptseg->s_data;
	ssd->sptas = sptarg->sptas;
	ssd->amp = shm_amp;
	ssd->ppa = spt_sd->ppa;
	ssd->sptseg = sptarg->sptseg;
	seg->s_data = (void *)ssd;
	seg->s_ops = &segspt_shmops;
	mutex_enter(&shm_amp->lock);
	shm_amp->refcnt++;
	mutex_exit(&shm_amp->lock);

	error = hat_share(seg->s_as->a_hat, seg->s_base,
			sptarg->sptas->a_hat, 0, seg->s_size);

	return (error);
}

int
segspt_shmunmap(struct seg *seg, caddr_t raddr, u_int ssize)
{
	struct sptshm_data *ssd = (struct sptshm_data *)seg->s_data;

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	if (ssd->softlockcnt > 0)
		return (EAGAIN);

	if (ssize != seg->s_size)
		return (EINVAL);

	hat_unshare(seg->s_as->a_hat, raddr, ssize);
	seg_free(seg);
	return (0);
}

void
segspt_shmfree(struct seg *seg)
{
	struct sptshm_data *ssd = (struct sptshm_data *)seg->s_data;
	struct anon_map *shm_amp = ssd->amp;

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * Need to increment refcnt when attaching
	 * and decrement when detaching because of dup().
	 */
	mutex_enter(&shm_amp->lock);
	shm_amp->refcnt--;
	mutex_exit(&shm_amp->lock);

	kmem_free((caddr_t)seg->s_data, sizeof (struct sptshm_data));
}

/* ARGSUSED */
int
segspt_shmsetprot(struct seg *seg, caddr_t addr, u_int len, u_int prot)
{

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * Shared page table is more than shared mapping.
	 *  Individual process sharing page tables can't change prot
	 *  because there is only one set of page tables.
	 *  This will be allowed after private page table is
	 *  supported.
	 */
/* need to return correct status error? */
	return (0);
}

faultcode_t
segspt_shmfault(struct hat *hat, struct seg *seg, caddr_t addr,
		u_int len, enum fault_type type, enum seg_rw rw)
{
	struct seg		*sptseg;
	struct sptshm_data 	*ssd;
	struct spt_data 	*spt_sd;
	struct as		*curspt;
	u_int  npages = btopr(len);

#ifdef lint
	hat = hat;
	rw  = rw;
#endif

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	ssd = (struct sptshm_data *)seg->s_data;
	curspt = ssd->sptas;
	sptseg = ssd->sptseg;
	spt_sd = sptseg->s_data;

	/*
	 * Because of the way spt is implemented
	 * the realsize of the segment does not have to be
	 * equal to the segment size itself. The segment size is
	 * often in multiples of a page size larger than PAGESIZE.
	 * The realsize is rounded up to the nearest PAGESIZE
	 * based on what the user requested. This is a bit of
	 * ungliness that is historical but not easily fixed
	 * without re-designing the higher levels of ISM.
	 */
	ASSERT(addr >= seg->s_base);
	if (((addr + len) - seg->s_base) > spt_sd->realsize)
		return (FC_NOMAP);

	switch (type) {

	case F_SOFTLOCK:

		/*
		 * Because we know that every shared memory is
		 * already locked and called in the same context.
		 */
		atomic_add_word((u_int *)&ssd->softlockcnt, npages, &spt_lock);
		return (0);

	case F_SOFTUNLOCK:

		atomic_add_word((u_int *)&ssd->softlockcnt, -npages, &spt_lock);

		/*
		 * Check for softlock
		 */
		if (ssd->softlockcnt == 0) {
			/*
			 * All SOFTLOCKS are gone. Wakeup any waiting
			 * unmappers so they can try again to unmap.
			 * As an optimization check for waiters first
			 * without the mutex held, so we're not always
			 * grabbing it on softunlocks.
			 */
			if (AS_ISUNMAPWAIT(seg->s_as)) {
				mutex_enter(&seg->s_as->a_contents);
				if (AS_ISUNMAPWAIT(seg->s_as)) {
					AS_CLRUNMAPWAIT(seg->s_as);
					cv_broadcast(&seg->s_as->a_cv);
				}
				mutex_exit(&seg->s_as->a_contents);
			}
		}
		return (0);

	case F_INVAL:

		if (hat_share(seg->s_as->a_hat, seg->s_base, curspt->a_hat,
		    sptseg->s_base, sptseg->s_size) != 0) {
			cmn_err(CE_PANIC, "hat_share error in ISM fault");
		}

		return (0);

	case F_PROT:

		return (FC_PROT);

	default:
#ifdef DEBUG
		cmn_err(CE_WARN, "segspt_shmfault default type?");
#endif
		return (FC_NOMAP);
	}
}

/*
 * duplicate the shared page tables
 */
int
segspt_shmdup(struct seg *seg, struct seg *newseg)
{
	struct sptshm_data *ssd = (struct sptshm_data *)seg->s_data;
	struct anon_map *amp = ssd->amp;
	struct sptshm_data *nsd;

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	nsd = (struct sptshm_data *)kmem_zalloc((sizeof (struct sptshm_data)),
						KM_SLEEP);
	newseg->s_data = (void *) nsd;
	nsd->sptas = ssd->sptas;
	nsd->amp = amp;
	nsd->ppa = ssd->ppa;
	nsd->sptseg = ssd->sptseg;
	newseg->s_ops = &segspt_shmops;

	mutex_enter(&amp->lock);
	amp->refcnt++;
	mutex_exit(&amp->lock);

	return (hat_share(newseg->s_as->a_hat, newseg->s_base,
	    ssd->sptas->a_hat, 0, seg->s_size));
}

/* ARGSUSED */
int
segspt_shmcheckprot(struct seg *seg, caddr_t addr, u_int size, u_int prot)
{

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * ISM segment is always rw.
	 */
	return (((PROT_ALL & prot) != prot) ? EACCES : 0);
}

/* ARGSUSED */
static int
segspt_shmlockop(struct seg *seg, caddr_t addr, u_int len,
			int attr, int op, ulong *lockmap, size_t pos)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/* ISM pages are always locked. */
	return (0);
}

/* ARGSUSED */
int
segspt_shmgetprot(struct seg *seg, caddr_t addr, u_int len, u_int *protv)
{
	int pgno = seg_page(seg, addr+len) - seg_page(seg, addr) + 1;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/*
	 * ISM segment is always rw.
	 */
	while (--pgno >= 0)
		*protv++ = PROT_ALL;
	return (0);
}

/* ARGSUSED */
u_offset_t
segspt_shmgetoffset(struct seg *seg, caddr_t addr)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/* Offset does not matter in ISM memory */

	return ((u_offset_t)0);
}

/* ARGSUSED */
int
segspt_shmgettype(struct seg *seg, caddr_t addr)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/* The shared memory mapping is always MAP_SHARED */

	return (MAP_SHARED);
}

/* ARGSUSED */
int
segspt_shmgetvp(struct seg *seg, caddr_t addr, struct vnode **vpp)
{
	struct sptshm_data *ssd = (struct sptshm_data *)seg->s_data;
	struct spt_data *spt = (struct spt_data *)ssd->sptseg->s_data;

	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	*vpp = spt->vp;
	return (0);

}

/* ARGSUSED */
static int
segspt_shmadvice(struct seg *seg, caddr_t addr, u_int len, int behav)
{
	ASSERT(seg->s_as && AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	return (0);
}

/* ARGSUSED */
void
segspt_shmdump(struct seg *seg)
{
	/* no-op for ISM segment */
}

/* ARGSUSED */
int
segspt_kluster(struct seg *seg, caddr_t addr, int delta)
{
	return (0);
}

/*
 * get a memory ID for an addr in a given segment
 */
static int
segspt_shmgetmemid(struct seg *seg, caddr_t addr, memid_t *memidp)
{
	struct sptshm_data *sptshm = (struct sptshm_data *)seg->s_data;
	struct anon 	*ap;
	u_int		anon_index;
	struct anon_map	*amp = sptshm->amp;
	struct spt_data	*sptdat = sptshm->sptseg->s_data;

	anon_index = seg_page(seg, addr);

	if (addr > (seg->s_base + sptdat->realsize)) {
		return (EFAULT);
	}

	ap = amp->anon[anon_index];
	memidp->val[0] = (u_longlong_t)ap;
	memidp->val[1] = (u_longlong_t)((long)addr & PAGEOFFSET);
	return (0);
}
