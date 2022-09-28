/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)mach_page.c 1.13     96/07/09 SMI"

/*
 * This file contains functions to access the machine dependent part
 * of the page structure.
 */

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/mach_page.h>

u_int	page_pptonum(page_t *);

/*
 * The following is for libkvm
 */
u_int pagenum_offset = (u_int)&(((machpage_t *)0)->p_pagenum);

/*
 * Search the memory segments to locate the desired page.  Within a
 * segment, pages increase linearly with one page structure per
 * physical page frame (size PAGESIZE).  The search begins
 * with the segment that was accessed last, to take advantage of locality.
 * If the hint misses, we start from the beginning of the sorted memseg list
 */

#ifdef DEBUG
#define	MEMSEG_SEARCH_STATS
#endif

#ifdef MEMSEG_SEARCH_STATS
struct memseg_stats {
    u_int nsearch;
    u_int nlastwon;
    u_int nhashwon;
} memseg_stats;
kmutex_t memseg_stats_lock;

extern void  atomic_add_word(u_int *, int, kmutex_t *);

#define	MEMSEG_STAT_INCR(v) \
	atomic_add_word(&memseg_stats.v, 1, &memseg_stats_lock)
#else
#define	MEMSEG_STAT_INCR(x)
#endif

struct memseg *memsegs;		/* list of memory segments */

/*
 * Some data structures for pfn to pp lookup.
 */
#define	MEM_HASH_SHIFT  0x9
#define	N_MEM_SLOTS	0x200		/* must be a power of 2 */
#define	MEMSEG_PFN_HASH(pfn)   (((pfn)/mhash_per_slot) & (N_MEM_SLOTS - 1))

void	build_pfn_hash();

static u_int mhash_per_slot;
static struct memseg *memseg_hash[N_MEM_SLOTS];

page_t *
page_numtopp_nolock(pfnum)
    register u_int pfnum;
{
	static struct memseg *last_memseg_by_pfnum = NULL;
	register struct memseg *seg;
	register machpage_t *pp;

	/*
	 *	XXX - Since page_numtopp_nolock is called in many places where
	 *	the search fails more than it succeeds. It maybe worthwhile
	 *	to put a check for pf_is_memory or a pfnum <= max_pfn (set at
	 *	boot time).
	 *
	 *	if (!pf_is_memory(pfnum) || (pfnum > max_pfn))
	 *		return (NULL);
	 */

	MEMSEG_STAT_INCR(nsearch);

	/* Try last winner first */
	if (((seg = last_memseg_by_pfnum) != NULL) &&
		(pfnum >= seg->pages_base) && (pfnum < seg->pages_end)) {
		MEMSEG_STAT_INCR(nlastwon);
		pp = seg->pages + (pfnum - seg->pages_base);
		return ((page_t *) pp);
	}

	/* Else Try hash */
	if (((seg = memseg_hash[MEMSEG_PFN_HASH(pfnum)]) != NULL) &&
		(pfnum >= seg->pages_base) && (pfnum < seg->pages_end)) {
		MEMSEG_STAT_INCR(nhashwon);
		last_memseg_by_pfnum = seg;
		pp = seg->pages + (pfnum - seg->pages_base);
		return ((page_t *) pp);
	}

	/* Else Brute force */
	for (seg = memsegs; seg != NULL; seg = seg->next) {
		if (pfnum >= seg->pages_base && pfnum < seg->pages_end) {
			last_memseg_by_pfnum = seg;
			pp = seg->pages + (pfnum - seg->pages_base);
			return ((page_t *) pp);
		}
	}
	return ((page_t *)NULL);

}

/*
 * Given a page and a count return the page struct that is
 * n structs away from the current one in the global page
 * list.
 *
 * This function wraps to the first page upon
 * reaching the end of the memseg list.
 */
page_t *
page_nextn(page_t *pp, u_int n)
{
	static struct memseg *last_page_next_memseg = NULL;
	register struct memseg *seg;
	machpage_t *mpp = (machpage_t *)pp;

	if (((seg = last_page_next_memseg) == NULL) ||
		!(mpp >= seg->pages && mpp < seg->epages)) {

		for (seg = memsegs; seg; seg = seg->next) {
			if (mpp >= seg->pages && mpp < seg->epages)
				break;
		}

		if (seg == NULL) {
			cmn_err(CE_PANIC,
			    "page_next: page 0x%x not in memsegs!", pp);
			/* NOTREACHED */
		}
	}


	while ((mpp + n) >= seg->epages) {
		n -= seg->epages - mpp;
		seg = seg->next;
		if (seg == NULL)
			seg = memsegs;
		mpp = seg->pages;
	}
	last_page_next_memseg = seg;
	return ((page_t *) (mpp + n));
}


/*
 * Returns next page in list. Note: this function wraps
 * to the first page in the list upon reaching the end
 * of the list. Callers should be aware of this fact.
 */

/* We should change this be a #define */

page_t *
page_next(page_t *pp)
{
	return (page_nextn(pp, 1));
}

page_t *
page_first()
{
	return ((page_t *)memsegs->pages);
}


void
build_pfn_hash()
{
	u_int index, cur;
	struct memseg *pseg;

	/*
	 * Physmax is the last valid pfn.
	 */
	mhash_per_slot = (physmax + 1) >> MEM_HASH_SHIFT;
	for (pseg = memsegs; pseg != NULL; pseg = pseg-> next) {
		index = MEMSEG_PFN_HASH(pseg->pages_base);
		cur = pseg->pages_base;
		do {
			if (index >= N_MEM_SLOTS)
				index = MEMSEG_PFN_HASH(cur);

			if (memseg_hash[index] == NULL ||
			    memseg_hash[index]->pages_base > pseg->pages_base) {
				memseg_hash[index] = pseg;
			}
			cur += mhash_per_slot;
			index++;
		} while (cur < pseg->pages_end);
	}
}

/*
 * Return the pagenum for the pp
 */
u_int
page_pptonum(page_t *pp)
{
	return (((machpage_t *)pp)->p_pagenum);
}

/*
 * interface to the referenced and modified etc bits
 * in the PSM part of the page struct
 * when no locking is desired.
 */
void
page_set_props(page_t *pp, u_int flags)
{
	ASSERT((flags & ~(P_MOD | P_REF | P_RO)) == 0);
	((machpage_t *)pp)->p_nrm |= (u_char)flags;
}

void
page_clr_all_props(page_t *pp)
{
	((machpage_t *)pp)->p_nrm = 0;
}
