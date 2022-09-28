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
 * 	(c) 1986, 1987, 1988, 1989, 1990, 1995, 1996  Sun Microsystems, Inc
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 *
 */

#ident	"@(#)vm_seg.c	1.46	96/10/17 SMI"
/*	From:	SVr4.0	"kernel:vm/vm_seg.c	1.14"		*/

/*
 * VM - segment management.
 */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/vmsystm.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>

/*
 * kstats for segment pagelock cache
 */
segplckstat_t segplckstat = {
	{ "cache_hit",		KSTAT_DATA_ULONG },
	{ "cache_miss",		KSTAT_DATA_ULONG },
	{ "active_pages",	KSTAT_DATA_ULONG },
	{ "cached_pages",	KSTAT_DATA_ULONG },
	{ "purge_count",	KSTAT_DATA_ULONG },
};

kstat_named_t *segplckstat_ptr = (kstat_named_t *)&segplckstat;
ulong_t segplckstat_ndata = sizeof (segplckstat) / sizeof (kstat_named_t);

/* #define	PDEBUG */
#if defined(PDEBUG) || defined(lint) || defined(__lint)
int pdebug = 0;
#else
#define	pdebug		0
#endif	/* PDEBUG */

#define	PPRINTF				if (pdebug) printf
#define	PPRINT(x)			PPRINTF(x)
#define	PPRINT1(x, a)			PPRINTF(x, a)
#define	PPRINT2(x, a, b)		PPRINTF(x, a, b)
#define	PPRINT3(x, a, b, c)		PPRINTF(x, a, b, c)
#define	PPRINT4(x, a, b, c, d)		PPRINTF(x, a, b, c, d)
#define	PPRINT5(x, a, b, c, d, e)	PPRINTF(x, a, b, c, d, e)

#define	P_HASHSIZE		64
#define	P_HASHMASK		(P_HASHSIZE - 1)
#define	P_BASESHIFT		8
#define	P_PMASK			0xfff

/*
 * entry in the segment page cache
 */
struct seg_pcache {
	struct seg_pcache *p_hnext;	/* list for hashed blocks */
	struct seg_pcache *p_hprev;
	int		p_active;	/* active count */
	int		p_ref;		/* ref bit */
	u_int		p_len;		/* segment length */
	caddr_t		p_addr;		/* base address */
	struct seg 	*p_seg;		/* segment */
	struct page	**p_pp;		/* pp shadow list */
	enum seg_rw	p_rw;		/* rw */
	void		(*p_callback)(struct seg *, caddr_t, u_int,
			    struct page **, enum seg_rw);
};

struct seg_phash {
	struct seg_pcache *p_hnext;	/* list for hashed blocks */
	struct seg_pcache *p_hprev;
	kmutex_t p_hmutex;		/* protects hash bucket */
};

static int seg_plazy = 1;	/* if 1, pages are cached after pageunlock */
static int seg_pwindow;		/* max # of pages that can be cached */
static int seg_plocked;		/* # of pages which are cached by pagelock */
int seg_preapahead;

static int seg_pupdate_active = 1;	/* background reclaim thread */
static int seg_preap_interval;	/* reap interval in sec. Default is 10 sec */

static kmutex_t seg_pcache;	/* protects the whole pagelock cache */
static kmutex_t seg_pmem;	/* protects window counter */
static ksema_t seg_psaync_sem;	/* sema for reclaim thread */
static struct seg_phash *p_hashtab;

#define	p_hash(seg) \
	(P_HASHMASK & \
	(((u_int)(seg) >> P_BASESHIFT) & P_PMASK))

#define	p_match(pcp, seg, addr, len, rw) \
	(((pcp)->p_seg == (seg) && \
	(pcp)->p_addr == (addr) && \
	(pcp)->p_rw == (rw) && \
	(pcp)->p_len == (len)) ? 1 : 0)

#define	p_match_pp(pcp, seg, addr, len, pp, rw) \
	(((pcp)->p_seg == (seg) && \
	(pcp)->p_addr == (addr) && \
	(pcp)->p_pp == (pp) && \
	(pcp)->p_rw == (rw) && \
	(pcp)->p_len == (len)) ? 1 : 0)


/*
 * lookup an address range in pagelock cache. Return shadow list
 * and bump up active count.
 */
struct page **
seg_plookup(struct seg *seg, caddr_t addr, u_int len, enum seg_rw rw)
{
	struct seg_pcache *pcp;
	struct seg_phash *hp;

	if (seg_plazy == 0) {
		return (NULL);
	}

	hp = &p_hashtab[p_hash(seg)];
	mutex_enter(&hp->p_hmutex);
	for (pcp = hp->p_hnext; pcp != (struct seg_pcache *)hp;
	    pcp = pcp->p_hnext) {
		if (p_match(pcp, seg, addr, len, rw)) {
			pcp->p_active++;
			mutex_exit(&hp->p_hmutex);
PPRINT5("seg_plookup hit: seg %x, addr %x, len %x, count %d, pplist %x\n",
	seg, addr, len, pcp->p_active, pcp->p_pp);
			return (pcp->p_pp);
		}
	}
	mutex_exit(&hp->p_hmutex);
PPRINT("seg_plookup miss:\n");
	return (NULL);
}

/*
 * mark address range inactive. If the cache is off or the address
 * range is not in the cache we call the segment driver to reclaim
 * the pages. Otherwise just decrement active count and set ref bit.
 */
void
seg_pinactive(struct seg *seg, caddr_t addr, u_int len, struct page **pp,
    enum seg_rw rw, void (*callback)(struct seg *, caddr_t, u_int,
    struct page **, enum seg_rw))
{
	struct seg_pcache *pcp;
	struct seg_phash *hp;

	if (seg_plazy == 0) {
		(*callback)(seg, addr, len, pp, rw);
		return;
	}
	hp = &p_hashtab[p_hash(seg)];
	mutex_enter(&hp->p_hmutex);
	for (pcp = hp->p_hnext; pcp != (struct seg_pcache *)hp;
	    pcp = pcp->p_hnext) {
		if (p_match_pp(pcp, seg, addr, len, pp, rw)) {
			pcp->p_active--;
			ASSERT(pcp->p_active >= 0);
			pcp->p_ref = 1;
			mutex_exit(&hp->p_hmutex);
			return;
		}
	}
	mutex_exit(&hp->p_hmutex);
	(*callback)(seg, addr, len, pp, rw);
}

/*
 * insert address range with shadow list into pagelock cache. If
 * the cache is off or we exceeded the allowed 'window' we just
 * return.
 */
void
seg_pinsert(struct seg *seg, caddr_t addr, u_int len, struct page **pp,
    enum seg_rw rw, void (*callback)(struct seg *, caddr_t, u_int,
    struct page **, enum seg_rw))
{
	struct seg_pcache *pcp;
	struct seg_phash *hp;
	int npages;
	int wired;

	if (seg_plazy == 0) {
		return;
	}
	npages = len >> PAGESHIFT;
	mutex_enter(&seg_pmem);
	wired = seg_plocked + npages;
	if (wired > seg_pwindow) {
		mutex_exit(&seg_pmem);
		return;
	}
	seg_plocked = wired;
	segplckstat.cached_pages.value.ul += npages;
	mutex_exit(&seg_pmem);

	pcp = kmem_alloc(sizeof (struct seg_pcache), KM_SLEEP);
	pcp->p_seg = seg;
	pcp->p_addr = addr;
	pcp->p_len = len;
	pcp->p_pp = pp;
	pcp->p_rw = rw;
	pcp->p_callback = callback;
	pcp->p_active = 1;

PPRINT4("seg_pinsert: seg %x, addr %x, len %x, pplist %x\n",
	seg, addr, len, pp);

	hp = &p_hashtab[p_hash(seg)];
	mutex_enter(&hp->p_hmutex);
	pcp->p_hnext = hp->p_hnext;
	pcp->p_hprev = (struct seg_pcache *)hp;
	hp->p_hnext->p_hprev = pcp;
	hp->p_hnext = pcp;
	mutex_exit(&hp->p_hmutex);
}

/*
 * prurge all entries from the pagelock cache if not active
 * and not recently used. Drop all locks and call through
 * the address space into the segment driver to reclaim
 * the pages. This makes sure we get the address space
 * and segment driver locking right.
 */
void
seg_ppurge_all(void)
{
	struct seg_pcache *delcallb_list = NULL;
	struct seg_pcache *pcp;
	struct seg_phash *hp;
	int npages = 0;

	/*
	 * if the cache if off or empty, return
	 */
	if (seg_plazy == 0 || seg_plocked == 0) {
		return;
	}
	for (hp = p_hashtab; hp < &p_hashtab[P_HASHSIZE]; hp++) {
		mutex_enter(&hp->p_hmutex);
		pcp = hp->p_hnext;
		while (pcp != (struct seg_pcache *)hp) {
			/*
			 * purge entries which are not active and
			 * have not been used recently
			 */
			if (!pcp->p_ref && !pcp->p_active) {
				struct as *as = pcp->p_seg->s_as;

				/*
				 * try to get the readers lock on the address
				 * space before taking out the cache element.
				 * This ensures as_pagereclaim() can actually
				 * call through the address space and free
				 * the pages. If we don't get the lock, just
				 * skip this entry. The pages will be reclaimed
				 * by the segment driver later.
				 */
				if (AS_LOCK_TRYENTER(as, &as->a_lock,
				    RW_READER)) {
					pcp->p_hprev->p_hnext = pcp->p_hnext;
					pcp->p_hnext->p_hprev = pcp->p_hprev;
					pcp->p_hprev = delcallb_list;
					delcallb_list = pcp;
				}
			} else {
				pcp->p_ref = 0;
			}
			pcp = pcp->p_hnext;
		}
		mutex_exit(&hp->p_hmutex);
	}

	/*
	 * run the delayed callback list. We don't want to hold the
	 * cache lock during a call through the address space.
	 */
	while (delcallb_list != NULL) {
		struct as *as;

		pcp = delcallb_list;
		delcallb_list = pcp->p_hprev;
		as = pcp->p_seg->s_as;

PPRINT4("seg_ppurge_all: purge seg %x, addr %x, len %x, pplist %x\n",
	pcp->p_seg, pcp->p_addr, pcp->p_len, pcp->p_pp);

		as_pagereclaim(as, pcp->p_pp, pcp->p_addr,
		    pcp->p_len, pcp->p_rw);
		AS_LOCK_EXIT(as, &as->a_lock);
		npages += pcp->p_len >> PAGESHIFT;
		kmem_free(pcp, sizeof (struct seg_pcache));
	}
	mutex_enter(&seg_pmem);
	seg_plocked -= npages;
	segplckstat.cached_pages.value.ul -= npages;
	mutex_exit(&seg_pmem);
}

/*
 * purge all entries for a given segment. Since we
 * callback into the segment driver directly for page
 * reclaim the caller needs to hold the right locks.
 */
void
seg_ppurge(struct seg *seg)
{
	struct seg_pcache *delcallb_list = NULL;
	struct seg_pcache *pcp;
	struct seg_phash *hp;
	int npages = 0;

	if (seg_plazy == 0) {
		return;
	}
	segplckstat.purge_count.value.ul++;
	hp = &p_hashtab[p_hash(seg)];
	mutex_enter(&hp->p_hmutex);
	pcp = hp->p_hnext;
	while (pcp != (struct seg_pcache *)hp) {
		if (pcp->p_seg == seg) {
			if (pcp->p_active) {
				break;
			}
			pcp->p_hprev->p_hnext = pcp->p_hnext;
			pcp->p_hnext->p_hprev = pcp->p_hprev;
			pcp->p_hprev = delcallb_list;
			delcallb_list = pcp;
		}
		pcp = pcp->p_hnext;
	}
	mutex_exit(&hp->p_hmutex);
	while (delcallb_list != NULL) {
		pcp = delcallb_list;
		delcallb_list = pcp->p_hprev;

PPRINT4("seg_ppurge: purge seg %x, addr %x, len %x, pplist %x\n",
	seg, pcp->p_addr, pcp->p_len, pcp->p_pp);

		ASSERT(seg == pcp->p_seg);
		(*pcp->p_callback)(seg, pcp->p_addr,
		    pcp->p_len, pcp->p_pp, pcp->p_rw);
		npages += pcp->p_len >> PAGESHIFT;
		kmem_free(pcp, sizeof (struct seg_pcache));
	}
	mutex_enter(&seg_pmem);
	seg_plocked -= npages;
	segplckstat.cached_pages.value.ul -= npages;
	mutex_exit(&seg_pmem);
}

/*
 * setup the pagelock cache
 */
static void
seg_pinit(void)
{
	struct seg_phash *hp;
	int i;

	mutex_init(&seg_pcache, "seg_pcache", MUTEX_DEFAULT, NULL);
	mutex_init(&seg_pmem, "seg_pmem", MUTEX_DEFAULT, NULL);

	sema_init(&seg_psaync_sem, 0, "seg_psaync_sem", SEMA_DEFAULT, NULL);

	mutex_enter(&seg_pcache);
	if (p_hashtab == NULL) {
		p_hashtab = kmem_zalloc(
			P_HASHSIZE * sizeof (struct seg_phash), KM_SLEEP);
		if (p_hashtab == NULL) {
			mutex_exit(&seg_pcache);
			cmn_err(CE_PANIC, "Can't allocate pcache hash table");
			/*NOTREACHED*/
		}
		for (i = 0; i < P_HASHSIZE; i++) {
			hp = (struct seg_phash *)&p_hashtab[i];
			hp->p_hnext = (struct seg_pcache *)hp;
			hp->p_hprev = (struct seg_pcache *)hp;
			mutex_init(&hp->p_hmutex, "seg_pmutex",
				MUTEX_DEFAULT, NULL);
		}
		if (seg_pwindow == 0) {
			u_int physmegs;

			physmegs = physmem >> (20 - PAGESHIFT);
			if (physmegs < 24) {
				/* don't use cache */
				seg_plazy = 0;
			} else if (physmegs < 64) {
				seg_pwindow = physmem >> 5; /* 3% of memory */
			} else {
				seg_pwindow = physmem >> 3; /* 12% of memory */
			}
		}
	}
	mutex_exit(&seg_pcache);
}

/*
 * called by pageout if memory is low
 */
void
seg_preap(void)
{
	/*
	 * if the cache if off or empty, return
	 */
	if (seg_plocked == 0 || seg_plazy == 0) {
		return;
	}
	sema_v(&seg_psaync_sem);
}

/*
 * run as a backgroud thread and reclaim pagelock
 * pages which have not been used recently
 */
void
seg_pasync_thread(void)
{
	if (seg_plazy && seg_pupdate_active) {
		if (seg_preap_interval == 0) {
			seg_preap_interval = 10 * hz;
		} else {
			seg_preap_interval *= hz;
		}
		(void) timeout(seg_pupdate, NULL, seg_preap_interval);
	}

	for (;;) {
		sema_p(&seg_psaync_sem);
		seg_ppurge_all();
	}
}

void
seg_pupdate(void *dummy)
{
	sema_v(&seg_psaync_sem);

	(void) timeout(seg_pupdate, dummy, seg_preap_interval);
}

static struct kmem_cache *seg_cache;

/*
 * Initialize segment management data structures.
 */
void
seg_init(void)
{
	kstat_t *ksp;

	seg_cache = kmem_cache_create("seg_cache", sizeof (struct seg),
		0, NULL, NULL, NULL, NULL, NULL, 0);

	ksp = kstat_create("unix", 0, "segplckstat", "vm", KSTAT_TYPE_NAMED,
		segplckstat_ndata, KSTAT_FLAG_VIRTUAL);
	if (ksp) {
		ksp->ks_data = (void *)segplckstat_ptr;
		kstat_install(ksp);
	}

	seg_pinit();
}

/*
 * Allocate a segment to cover [base, base+size]
 * and attach it to the specified address space.
 */
struct seg *
seg_alloc(as, base, size)
	struct as *as;
	register caddr_t base;
	register u_int size;
{
	register struct seg *new;
	caddr_t segbase;
	u_int segsize;

	segbase = (caddr_t)((u_int)base & PAGEMASK);
	segsize =
	    (((u_int)(base + size) + PAGEOFFSET) & PAGEMASK) - (u_int)segbase;

	if (!valid_va_range(&segbase, &segsize, segsize, AH_LO))
		return ((struct seg *)NULL);	/* bad virtual addr range */

	if ((as != &kas) && !valid_usr_range(segbase, segsize))
		return ((struct seg *)NULL);	/* bad virtual addr range */

	new = kmem_cache_alloc(seg_cache, KM_SLEEP);
	new->s_ops = NULL;
	new->s_data = NULL;
	if (seg_attach(as, segbase, segsize, new) < 0) {
		kmem_cache_free(seg_cache, new);
		return ((struct seg *)NULL);
	}
	/* caller must fill in ops, data */
	return (new);
}

/*
 * Attach a segment to the address space.  Used by seg_alloc()
 * and for kernel startup to attach to static segments.
 */
int
seg_attach(as, base, size, seg)
	struct as *as;
	caddr_t base;
	u_int size;
	struct seg *seg;
{
	seg->s_as = as;
	seg->s_base = base;
	seg->s_size = size;

	/*
	 * as_addseg() will add the segment at the appropraite point
	 * in the list. It will return -1 if there is overlap with
	 * an already existing segment.
	 */
	return (as_addseg(as, seg));
}

/*
 * Unmap a segment and free it from its associated address space.
 * This should be called by anybody who's finished with a whole segment's
 * mapping.  Just calls SEGOP_UNMAP() on the whole mapping .  It is the
 * responsibility of the segment driver to unlink the the segment
 * from the address space, and to free public and private data structures
 * associated with the segment.  (This is typically done by a call to
 * seg_free()).
 */
void
seg_unmap(seg)
	register struct seg *seg;
{
#ifdef DEBUG
	int ret;
#endif /* DEBUG */

	ASSERT(seg->s_as && AS_WRITE_HELD(seg->s_as, &seg->s_as->a_lock));

	/* Shouldn't have called seg_unmap if mapping isn't yet established */
	ASSERT(seg->s_data != NULL);

	/* Unmap the whole mapping */
#ifdef DEBUG
	ret = SEGOP_UNMAP(seg, seg->s_base, seg->s_size);
	ASSERT(ret == 0);
#else
	SEGOP_UNMAP(seg, seg->s_base, seg->s_size);
#endif /* DEBUG */
}

/*
 * Free the segment from its associated as. This should only be called
 * if a mapping to the segment has not yet been established (e.g., if
 * an error occurs in the middle of doing an as_map when the segment
 * has already been partially set up) or if it has already been deleted
 * (e.g., from a segment driver unmap routine if the unmap applies to the
 * entire segment). If the mapping is currently set up then seg_unmap() should
 * be called instead.
 */
void
seg_free(seg)
	register struct seg *seg;
{
	register struct as *as = seg->s_as;
	struct seg *tseg = as_removeseg(as, seg->s_base);

	ASSERT(tseg == seg);

	/*
	 * If the segment private data field is NULL,
	 * then segment driver is not attached yet.
	 */
	if (seg->s_data != NULL)
		SEGOP_FREE(seg);

	kmem_cache_free(seg_cache, seg);
}
