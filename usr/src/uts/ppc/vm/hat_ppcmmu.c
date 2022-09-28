/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)hat_ppcmmu.c	1.42	96/10/17 SMI"

/*
 * VM - Hardware Address Translation management.
 *
 * This file implements the machine specific hardware translation
 * needed by the VM system.  The machine independent interface is
 * described in <vm/hat.h> while the machine dependent interface
 * and data structures are described in <vm/hat_ppcmmu.h>.
 *
 * The hat layer manages the address translation hardware as a cache
 * driven by calls from the higher levels in the VM system.  Nearly
 * all the details of how the hardware is managed should not be visable
 * about this layer except for miscellanous machine specific functions
 * (e.g. mapin/mapout) that work in conjunction with this code.  Other
 * than a small number of machine specific places, the hat data
 * structures seen by the higher levels in the VM system are opaque
 * and are only operated on by the hat routines.  Each address space
 * contains a struct hat and a page contains an opaque pointer which
 * is used by the hat code to hold a list of active translations to
 * that page.
 *
 * Hardware address translation routines for the PowerPC MMU.
 *
 * The definitions are for 32bit implementation of PowerPC.
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <sys/machparam.h>
#include <sys/machsystm.h>
#include <sys/vtrace.h>
#include <sys/kmem.h>
#include <sys/pte.h>
#include <sys/mmu.h>
#include <sys/cmn_err.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/var.h>
#include <sys/debug.h>
#include <sys/archsystm.h>

#include <vm/hat.h>
#include <vm/hat_ppcmmu.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/page.h>
#include <vm/seg_kp.h>
#include <vm/rm.h>
#include <vm/mach_ppcmmu.h>
#include <vm/mach_page.h>

/*
 * Public functions and data referenced by this hat.
 */
extern struct	seg	*segkmap;

/*
 * Semi-private data
 */
hwpte_t		*ptes, *eptes;		/* the pte array */
hwpte_t		*mptes;			/* the upper half of pte array */
struct	hment	*hments, *ehments;	/* the hments array */
struct	ptegp	*ptegps, *eptegps;	/* PTEGPs array */
u_int		nptegp;			/* number of PTEG-pairs */
u_int		nptegmask;		/* mask for the PTEG number */
u_int		hash_pteg_mask;		/* mask used in computing PTEG offset */
u_int		mmu601;			/* flag to indicate if it is 601 */
struct	batinfo	bats[8];		/* table for Bat information */
int		cache_blockmask;

/*
 * Private data.
 */

/*
 * PTELOCK counts: Since a pte can be locked multiple times we need to
 * maintain a lock count for each pte. On PPC the PTEs of the same address
 * space are not grouped like in other architectures (e.g x86, srmmu) so we
 * need a seperate counter for each pte. The occurence of multiple locks
 * on a pte is not common and so we are using a two level scheme to minimize
 * memory for the ptelock counters. We use 2 bits per PTE in the ptegp
 * structure which keeps a lock count of upto 2 and an indication
 * if it exceeds 2. For lock counts greater than 2 we use a small hash table
 * to maintain the lock count for those ptes.
 */

/* structure of hash table entry for keeping pte lock count values */
typedef struct ptelock_hash {
	u_int	tag;		/* address of hw pte */
	u_int	lockcnt;	/* lock count for this pte */
} ptelock_hash_t;

ptelock_hash_t *ptelock_hashtab; /* hash table for pte lock counts */
int ptelock_hashtab_size; /* # of entries in ptelock_hashtab[] */
lock_t ptelock_hash_lock; /* lock to protect ptelock_hashtab[] */
u_int ptelock_hash_mask;
#ifdef HAT_DEBUG
int ptetohash_ratio = 2; /* ratio of 1:2 ptes for ptelock hash table */
#else
int ptetohash_ratio = 64; /* ratio of 1:64 ptes for ptelock hash table */
#endif
static int ptelock_hash_ops(hwpte_t *, int, int);

/* values for flags argument in ptelock_hash_ops() */
#define	HASH_CREATE	1	/* create a hash entry for the pte */
#define	HASH_DELETE	2	/* delete the hash entry for the pte */
#define	HASH_CHANGE	4	/* change to the lock count of the pte */

#define	create_ptelock(pte, cnt) 	ptelock_hash_ops(pte, cnt, HASH_CREATE)
#define	get_ptelock(pte)		ptelock_hash_ops(pte, 0, HASH_CHANGE)
#define	change_ptelock(pte, incr)	ptelock_hash_ops(pte, incr, HASH_CHANGE)
#define	delete_ptelock(pte)		ptelock_hash_ops(pte, 0, HASH_DELETE)

/*
 * Macro to ease use of PSM page structure
 */
#define	mach_pp	((machpage_t *)pp)

/*
 * stats for ppcmmu
 *	vh_ptegoverflow
 *		Number of PTEG overflows.
 *	vh_vsid_gc_wakeups
 *		How many times the vsid_gc thread is invoked.
 */
struct vmhatstat vmhatstat = {
	{ "vh_ptegoverflow",		KSTAT_DATA_ULONG },
	{ "vh_pteload",			KSTAT_DATA_ULONG },
	{ "vh_mlist_enter",		KSTAT_DATA_ULONG },
	{ "vh_mlist_enter_wait",	KSTAT_DATA_ULONG },
	{ "vh_mlist_exit",		KSTAT_DATA_ULONG },
	{ "vh_mlist_exit_broadcast",	KSTAT_DATA_ULONG },
	{ "vh_vsid_gc_wakeups",		KSTAT_DATA_ULONG },
	{ "vh_hash_ptelock",		KSTAT_DATA_ULONG },
};

/*
 * kstat data
 */
kstat_named_t *vmhatstat_ptr = (kstat_named_t *)&vmhatstat;
ulong_t vmhatstat_ndata = sizeof (vmhatstat) / sizeof (kstat_named_t);

/*
 * Global data
 */

/* Default number of vsid ranges to be used */
u_int ppcmmu_max_vsidranges = DEFAULT_VSIDRANGES;

/*
 * vsid_bitmap[]
 *	Each bit specifies if the corresponding vsid-range-id is in use.
 *	ppcmmu_alloc() sets the bit in the bit map and ppcmmu_free()
 *	resets it.
 */
u_int *vsid_bitmap;	/* bitmap for active vsid-range-ids */
struct vsid_alloc_set *vsid_alloc_head; /* head of the free list */

#define	NBINT	(sizeof (u_int) * NBBY)	/* no. of bits per u_int */
#define	SET_VSID_RANGE_INUSE(id)\
		(vsid_bitmap[(id) / NBINT] |= (1 << ((id) % NBINT)))
#define	CLEAR_VSID_RANGE_INUSE(id)\
		(vsid_bitmap[(id) / NBINT] &= ~(1 << ((id) % NBINT)))
#define	IS_VSID_RANGE_INUSE(id)\
		(vsid_bitmap[(id) / NBINT] & (1 << ((id) % NBINT)))

/*
 * External data
 */
extern struct as kas;			/* kernel's address space */

/*
 * PPCMMU has several levels of locking.  The locks must be acquired
 * in the correct order to prevent deadlocks. The locks required to change/add
 * mappings in a PTEG should be acquired in the order:
 *	mlist_lock (if there is a page structure associated with the page).
 *	hat_mutex
 *	ptegp_mutex (per PTEG pair mutex).
 *
 * Page mapping lists are locked with the per-page inuse bit, which
 * is manipulated by ppcmmu_mlist_enter() and ppcmmu_mlist_exit(). This
 * bit must be held to look at or change a page's p_mapping list.
 *
 * The ppcmmu_page_lock array of mutexes protect all pages' p_nrm fields.
 * They also protect the inuse and wanted bits.
 * To make life more interesting, the p_nrm bits may be played with without
 * holding the appropriate ppcmmu_page_lock if and only if the particular
 * page is not mapped.  This is typically done in page_create() after a
 * call to page_hashout().
 *
 * The global vsid-range-id resources are mutexed by ppcmmu_res_lock.
 * Any lock can be held when locking ppcmmu_res_lock.
 *
 * The per PTEGP structure mutex protects all the mappings in the group pair
 * and all the fields in the ptegp structure. Any lock can be held when
 * locking this mutex.
 */
#define	SPL_TABLE_SIZE	64		/* must be a power of 2 */
#define	SPL_SHIFT	6
#define	SPL_HASH(pp)	\
	&ppcmmu_page_lock[(((u_int)pp) >> SPL_SHIFT) & (SPL_TABLE_SIZE - 1)]

static kmutex_t	ppcmmu_page_lock[SPL_TABLE_SIZE];
static kmutex_t	ppcmmu_res_lock;

/*
 * condition variable on which the vsid garbage collector thread (vsid_gc)
 * waits for work.
 */
kcondvar_t ppcmmu_cv;

/* condidtion variable on which someone waits for a free vsid range */
kcondvar_t ppcmmu_vsid_cv;
int ppcmmu_vsid_wanted; /* flag to indicate waiters */

static void		ppcmmu_init(void);
static void		ppcmmu_alloc(struct hat *, struct as *);
static struct as	*ppcmmu_setup(struct as *, int);
static void		ppcmmu_free(struct hat *, struct as *);
static void		ppcmmu_swapin(struct hat *, struct as *);
static void		ppcmmu_swapout(struct hat *, struct as *);
static int		ppcmmu_dup(struct hat *, struct as *, struct as *);
void			ppcmmu_memload(struct hat *, struct as *, caddr_t,
				struct page *, u_int, int);
void			ppcmmu_devload(struct hat *, struct as *, caddr_t,
				devpage_t *, u_int, u_int, int);
#ifdef notdef
void			ppcmmu_contig_memload(struct hat *, struct as *,
				caddr_t, struct page *, u_int, int, u_int);
#endif notdef
static void		ppcmmu_unlock(struct hat *, struct as *, caddr_t,
				u_int);
faultcode_t		ppcmmu_fault(struct hat *, caddr_t);
static int		ppcmmu_probe(struct hat *, struct as *, caddr_t);
int			ppcmmu_share(struct as *, caddr_t, struct as *,
				caddr_t, u_int);
void			ppcmmu_unshare(struct as *, caddr_t, u_int);
void			ppcmmu_chgprot(struct as *, caddr_t, u_int, u_int);
void			ppcmmu_unload(struct as *, caddr_t, u_int, int);
static void		ppcmmu_sync(struct as *, caddr_t, u_int, u_int);
static void		ppcmmu_pageunload(struct page *, struct hment *);
static void		ppcmmu_pagesync(struct hat *, struct page *,
				struct hment *, u_int);
static void		ppcmmu_pagecachectl(struct page *, u_int);
u_int			ppcmmu_getkpfnum(caddr_t);
u_int			ppcmmu_getpfnum(struct as *, caddr_t);
static int		ppcmmu_map(struct hat *, struct as *, caddr_t, u_int,
				int);
static void		ppcmmu_lock_init(void);
void ppcmmu_vsid_gc(void);

/* local functions */
static void ppcmmu_pteload(struct as *, caddr_t, struct page *,
			struct pte *, int, u_int);
static void ppcmmu_pteunload(struct ptegp *, hwpte_t *, struct page *,
			struct hment *, int);
static void ppcmmu_ptesync(struct ptegp *, hwpte_t *, struct hment *, int);

static hwpte_t *find_pte(hwpte_t *pteg, u_int vsid, u_int api, u_int h);
static hwpte_t *steal_pte(struct hat *, hwpte_t *pteg, struct ptegp *ptegp);
static hwpte_t *find_free_pte(struct hat *, hwpte_t *ppteg,
			struct ptegp *ptegp, page_t *pp);
caddr_t hwpte_to_vaddr(hwpte_t *pte);
static void build_vsid_freelist(void);
static void ppcmmu_cache_flushpage(struct hat *, caddr_t);

faultcode_t hat_softlock(struct hat *, caddr_t, size_t *,
			struct page **, u_int);
faultcode_t ppcmmu_pageflip(struct hat *, caddr_t, caddr_t, size_t *,
			struct page **, struct page **);

/* ppcmmu locking operations */
static void	ppcmmu_page_enter(struct page *);
static void	ppcmmu_page_exit(struct page *);
static void	ppcmmu_hat_enter(struct as *);
static void	ppcmmu_hat_exit(struct as *);
static void	ppcmmu_mlist_enter(struct page *);
static void	ppcmmu_mlist_exit(struct page *);
static int	ppcmmu_mlist_held(struct page *);
static void	ppcmmu_cachectl_enter(struct page *);
static void	ppcmmu_cachectl_exit(struct page *);

static void		hat_freehat();
struct hat 		*hat_gethat();

kmutex_t	hat_statlock;		/* for the stuff in hat_refmod.c */
kmutex_t	hat_res_mutex;		/* protect global freehat list */
kmutex_t	hat_kill_procs_lock;	/* for killing process on memerr */

struct	hat	*hats;
struct	hat	*hatsNHATS;
struct	hat	*hatfree = (struct hat *)NULL;

int		nhats;
kcondvar_t	hat_kill_procs_cv;
int		hat_inited = 0;


/*
 *
 * The next set of routines implements the machine
 * dependent hat interface described in <vm/hat.h>
 *
 */
/*
 * Call the init routines for ppc hat.
 */
void
hat_init()
{
	register struct hat *hat;

	ppcmmu_lock_init();

	/*
	 * Allocate mmu independent hat data structures.
	 */
	nhats = v.v_proc + (v.v_proc/2);
	if ((hats = (struct hat *)kmem_zalloc(sizeof (struct hat) * nhats,
	    KM_NOSLEEP)) == NULL)
		panic("hat_init - Cannot allocate memory for hat structs");
	hatsNHATS = hats + nhats;

	for (hat = hatsNHATS - 1; hat >= hats; hat--) {
		mutex_init(&hat->hat_mutex, "hat_mutex", MUTEX_DEFAULT, NULL);
		hat_freehat(hat);
	}

	ppcmmu_init();

	/*
	 * Initialize any global state for the statistics handling.
	 * Hrm_lock protects the globally allocted memory
	 *	hrm_memlist and hrm_hashtab.
	 */
	mutex_init(&hat_statlock, "hat_statlock", MUTEX_DEFAULT, NULL);

	/*
	 * We grab the first hat for the kernel,
	 * the above initialization loop initialized kctx.
	 */
	AS_LOCK_ENTER(&kas, &kas.a_lock, RW_WRITER);
	kas.a_hat = hat_alloc(&kas);
	AS_LOCK_EXIT(&kas, &kas.a_lock);
}

/*
 * Allocate a hat structure.
 * Called when an address space first uses a hat.
 * Links allocated hat onto the hat list for the address space (as->a_hat)
 */
struct hat *
hat_alloc(struct as *as)
{
	register struct hat *hat;

	if ((hat = hat_gethat()) == NULL)
		panic("hat_alloc - no hats");

	ASSERT(AS_WRITE_HELD(as, &as->a_lock));
	hat->hat_as = as;
	hat->hat_next = NULL;
	hat->s_rmstat = 0;

	ppcmmu_alloc(hat, as);
	return (hat);
}

/*
 * Hat_setup, makes an address space context the current active one;
 * uses the default hat, calls the setup routine for the ppc mmu.
 */
void
hat_setup(struct hat *hat, int allocflag)
{
	struct as *oas;
	struct as *as;

	as = hat->hat_as;

	ppcmmu_hat_enter(as);
	oas = ppcmmu_setup(as, allocflag);
	curthread->t_mmuctx = 0;
	ppcmmu_hat_exit(as);
#ifdef lint
	oas = oas;
#endif
}

/*
 * Free all the translation resources for the specified address space.
 * Called from as_free when an address space is being destroyed.
 */
void
hat_free_start(struct hat *hat)
{
	struct as	*as = hat->hat_as;

	/* free everything, called from as_free() */
	if (hat->s_rmstat)
		hat_freestat(as, NULL);
	ppcmmu_free(hat, as);
}


void
hat_free_end(struct hat *hat)
{
	hat->hat_as = NULL;
	hat_freehat(hat);
}

/*
 * Duplicate the translations of an as into another newas
 */
/* ARGSUSED */
int
hat_dup(struct hat *hat, struct hat *newhat, caddr_t addr, size_t len,
	u_int flag)
{
	ASSERT((flag == 0) || (flag == HAT_DUP_ALL) || (flag == HAT_DUP_COW));

	if (flag == HAT_DUP_COW)
		cmn_err(CE_PANIC, "hat_dup: HAT_DUP_COW not supported");
		/* NOTREACHED */
	else if (flag != HAT_DUP_ALL)
		return (0);
	else
		return (ppcmmu_dup(hat, hat->hat_as, newhat->hat_as));
}

/*
 * Set up any translation structures, for the specified address space,
 * that are needed or preferred when the process is being swapped in.
 */
/* ARGSUSED */
void
hat_swapin(struct hat *hat)
{
	ASSERT(AS_LOCK_HELD(hat->hat_as, &hat->hat_as->a_lock));
	ppcmmu_swapin(hat, hat->hat_as);
}

/*
 * Free all of the translation resources, for the specified address space,
 * that can be freed while the process is swapped out. Called from as_swapout.
 * Also, free up the ctx that this process was using.
 */
void
hat_swapout(struct hat *hat)
{
	ASSERT(AS_LOCK_HELD(hat->hat_as, &hat->hat_as->a_lock));
	ppcmmu_swapout(hat, hat->hat_as);
}

/*
 * Make a mapping at addr to map page pp with protection prot.
 *
 * No locking at this level.  Hat_memload and hat_devload
 * are wrappers for xxx_pteload, where xxx is instance of
 * a hat module like the sun mmu.  Xxx_pteload is called from
 * machine dependent code directly.  The locking (acquiring
 * the per hat hat_mutex) that is done in xxx_pteload allows both the
 * hat layer and the machine dependent code to work properly.
 */
void
hat_memload(struct hat *hat, caddr_t addr, struct page *pp,
		u_int attr, u_int flags)
{
	ASSERT((hat->hat_as == &kas) || AS_LOCK_HELD(hat->hat_as,
				&hat->hat_as->a_lock));
	ASSERT(se_assert(&pp->p_selock));
	if (PP_ISFREE(pp))
		cmn_err(CE_PANIC,
			"hat_memload: loading a mapping to free page %x",
								(int)pp);
	if (flags & ~HAT_SUPPORTED_LOAD_FLAGS)
		cmn_err(CE_NOTE, "hat_memload: unsupported flags %d",
				flags & ~HAT_SUPPORTED_LOAD_FLAGS);

	ppcmmu_memload(hat, hat->hat_as, addr, pp, attr, flags);
}

/*
 * Cons up a struct pte using the device's pf bits and protection
 * prot to load into the hardware for address addr; treat as minflt.
 */
void
hat_devload(struct hat *hat, caddr_t addr, size_t len, u_long pfn,
		u_int attr, int flags)
{
	register devpage_t *dp = NULL;

	ASSERT((hat->hat_as == &kas) || AS_LOCK_HELD(hat->hat_as,
				&hat->hat_as->a_lock));
	if (len == 0)
		cmn_err(CE_PANIC, "hat_devload: zero len");
	if (flags & ~HAT_SUPPORTED_LOAD_FLAGS)
		cmn_err(CE_NOTE, "hat_devload: unsupported flags %d",
				flags & ~HAT_SUPPORTED_LOAD_FLAGS);
	ASSERT(hat != NULL);

	while (len) {
		/*
		 * If it's a memory page find its pp
		 */
		if (!(flags & HAT_LOAD_NOCONSIST) && pf_is_memory(pfn)) {
			dp = (devpage_t *)page_numtopp_nolock(pfn);
			if (dp == NULL)
				flags |= HAT_LOAD_NOCONSIST;
		} else {
			dp = NULL;
		}
		ppcmmu_devload(hat, hat->hat_as, addr, dp, pfn, attr, flags);

		pfn++;
		addr += MMU_PAGESIZE;
		len -= MMU_PAGESIZE;
	}
}

/*
 * Set up range of mappings for array of pp's.
 */
void
hat_memload_array(struct hat *hat, caddr_t addr, size_t len,
			struct page **ppa, u_int attr, u_int flags)
{
	caddr_t eaddr = addr + len;

	for (; addr < eaddr; addr += PAGESIZE, ppa++) {
		hat_memload(hat, addr, *ppa, attr, flags);
	}
}

/*
 * Release one hardware address translation lock on the given address range.
 */
void
hat_unlock(struct hat *hat, caddr_t addr, size_t len)
{

	ASSERT(hat != NULL);
	ASSERT(hat->hat_as == &kas || AS_LOCK_HELD(hat->hat_as,
				&hat->hat_as->a_lock));

	ppcmmu_unlock(hat, hat->hat_as, addr, len);
}

/*
 * hat_chgprot is a deprecated hat call.  New segment drivers
 * should store all attributes and use hat_*attr calls.
 *
 * Change the protections in the virtual address range
 * given to the specified virtual protection.  If vprot is ~PROT_WRITE,
 * then remove write permission, leaving the other
 * permissions unchanged.  If vprot is ~PROT_USER, remove user permissions.
 *
 */
void
hat_chgprot(struct hat *hat, caddr_t addr, size_t len, u_int vprot)
{
	ASSERT(AS_LOCK_HELD(hat->hat_as, &hat->hat_as->a_lock));

	ppcmmu_chgprot(hat->hat_as, addr, len, vprot);
}

/*
 * Enables more attributes on specified address range (ie. logical OR)
 */
void
hat_setattr(struct hat *hat, caddr_t addr, size_t len, u_int attr)
{
	ppcmmu_chgprot(hat->hat_as, addr, len, attr);
}

/*
 * Assigns attributes to the specified address range.  All the attributes
 * are specified.
 */
void
hat_chgattr(struct hat *hat, caddr_t addr, size_t len, u_int attr)
{
	if ((attr & ~HAT_PROT_MASK) != 0)
		panic("hat_chgattr - wrong attributes");

	ppcmmu_chgprot(hat->hat_as, addr, len, attr);
}

/*
 * Remove attributes on the specified address range (ie. loginal NAND)
 */
void
hat_clrattr(struct hat *hat, caddr_t addr, size_t len, u_int attr)
{

	if (attr & PROT_USER)
		ppcmmu_chgprot(hat->hat_as, addr, len, ~PROT_USER);

	if (attr & PROT_WRITE)
		ppcmmu_chgprot(hat->hat_as, addr, len, ~PROT_WRITE);
}

/*
 * Unload all the mappings in the range [addr..addr+len). addr and len must
 * be MMU_PAGESIZE aligned.
 */
void
hat_unload(struct hat *hat, caddr_t addr, size_t len, u_int flags)
{
	ASSERT(hat->hat_as == &kas || (flags & HAT_UNLOAD_OTHER) || \
	    AS_LOCK_HELD(hat->hat_as, &hat->hat_as->a_lock));
	ppcmmu_unload(hat->hat_as, addr, len, flags);
}


/*
 * Synchronize all the mappings in the range [addr..addr+len).
 */
void
hat_sync(struct hat *hat, caddr_t addr, size_t len, u_int clearflag)
{
	ppcmmu_sync(hat->hat_as, addr, len, clearflag);
}

/*
 * Remove all mappings to page 'pp'.
 */
/* ARGSUSED */
int
hat_pageunload(struct page *pp, u_int forceflag)
{
	struct hment *hme;

	ASSERT(se_assert(&pp->p_selock));

	ppcmmu_mlist_enter(pp);
	while ((hme = (struct hment *)mach_pp->p_mapping) != NULL)
		ppcmmu_pageunload(pp, hme);

	ASSERT(mach_pp->p_mapping == NULL);
	ppcmmu_mlist_exit(pp);

	return (0);
}

/*
 * synchronize software page struct with hardware,
 * zeros the reference and modified bits
 */
u_int
hat_pagesync(struct page *pp, u_int clearflag)
{
	struct hment *hme;
	struct hat *hat;
	/* ASSERT(se_assert(&pp->p_selock)); */

	ppcmmu_mlist_enter(pp);
	for (hme = mach_pp->p_mapping; hme; hme = hme->hme_next) {
		hat = &hats[hme->hme_hat];
		ppcmmu_pagesync(hat, pp, hme, clearflag & ~HAT_SYNC_STOPON_RM);
		/*
		 * If clearflag is HAT_DONTZERO, break out as soon
		 * as the "ref" or "mod" is set.
		 */
		if ((clearflag & ~HAT_SYNC_STOPON_RM) == HAT_SYNC_DONTZERO &&
		    ((clearflag & HAT_SYNC_STOPON_MOD) && PP_ISMOD(mach_pp)) ||
		    ((clearflag & HAT_SYNC_STOPON_REF) && PP_ISREF(mach_pp)))
			break;
	}
	ppcmmu_mlist_exit(pp);
	return (PP_GENERIC_ATTR(mach_pp));
}

/*
 * Mark the page as cached or non-cached (depending on flag). Make all mappings
 * to page 'pp' cached or non-cached. This is permanent as long as the page
 * identity remains the same.
 */
void
hat_pagecachectl(struct page *pp, int flag)
{
	ASSERT(se_assert(&pp->p_selock));

	ppcmmu_cachectl_enter(pp);
	ppcmmu_page_enter(pp);

	if (flag & HAT_TMPNC)
		PP_SETTNC(mach_pp);
	else if (flag & HAT_UNCACHE)
		PP_SETPNC(mach_pp);
	else {
		PP_CLRPNC(mach_pp);
		PP_CLRTNC(mach_pp);
	}

	ppcmmu_page_exit(pp);

	ppcmmu_pagecachectl(pp, flag);

	ppcmmu_cachectl_exit(pp);
}

/*
 * Returns a page frame number for a given user virtual address.
 * Returns -1 to indicate an invalid mapping
 */
u_long
hat_getpfnum(struct hat *hat, caddr_t addr)
{

	/*
	 * We would like to
	 * ASSERT(AS_LOCK_HELD(as, &as->a_lock));
	 * but we can't because the iommu driver will call this
	 * routine at interrupt time and it can't grab the as lock
	 * or it will deadlock: A thread could have the as lock
	 * and be waiting for io.  The io can't complete
	 * because the interrupt thread is blocked trying to grab
	 * the as lock.
	 */

	if (hat->hat_as == &kas)
		return ((u_long)ppcmmu_getkpfnum(addr));
	else
		return ((u_long)ppcmmu_getpfnum(hat->hat_as, addr));
}

/*
 * Returns a page frame number for a kerenl virtual address.
 * this wrapper is so we dont't have to recompile PPC scsi drivers
 * (ncrs).
 */

u_long
hat_getkpfnum(caddr_t addr)
{
	return (hat_getpfnum(kas.a_hat, addr));
}

/*
 * Return the number of mappings to a particular page.
 * This number is an approximation of the number of
 * number of people sharing the page.
 */
u_long
hat_page_getshare(page_t *pp)
{
	return (mach_pp->p_share);
}

/* ARGSUSED */
size_t
hat_getpagesize(struct hat *hat, caddr_t addr)
{
	if (ppcmmu_probe(hat, hat->hat_as, addr))
		return (MMU_PAGESIZE);	/* always the same size */
	else
		return (0);
	/*NOTREACHED*/
}

/* ARGSUSED */
void
hat_page_badecc(u_long pfn)
{
	cmn_err(CE_PANIC, "hat_page_badecc: not supported");
}

/*
 * hat_probe returns 1 if the translation for the address 'addr' is
 * loaded, zero otherwise.
 *
 * hat_probe should be used only for advisorary purposes because it may
 * occasionally return the wrong value. The implementation must guarantee that
 * returning the wrong value is a very rare event. hat_probe is used
 * to implement optimizations in the segment drivers.
 *
 * hat_probe doesn't acquire hat_mutex.
 */
int
hat_probe(struct hat *hat, caddr_t addr)
{
	return (ppcmmu_probe(hat, hat->hat_as, addr));
}

/*
 * For compatability with AT&T and later optimizations
 */
/* ARGSUSED */
void
hat_map(struct hat *hat, caddr_t addr, size_t len, u_int flags)
{
	ASSERT(hat != NULL);
	ppcmmu_map(hat, hat->hat_as, addr, len, flags);
}

void
hat_page_setattr(page_t *pp, u_int flag)
{
	ASSERT(!(flag & ~(P_MOD | P_REF | P_RO)));

	if ((mach_pp->p_nrm & flag) == flag) {
		/* attribute already set */
		return;
	}

	ppcmmu_page_enter(pp);
	mach_pp->p_nrm |= flag;
	ppcmmu_page_exit(pp);
}

void
hat_page_clrattr(page_t *pp, u_int flag)
{
	ASSERT(!(flag & ~(P_MOD | P_REF | P_RO)));

	ppcmmu_page_enter(pp);
	mach_pp->p_nrm &= ~flag;
	ppcmmu_page_exit(pp);
}

u_int
hat_page_getattr(page_t *pp, u_int flag)
{
	ASSERT(!(flag & ~(P_MOD | P_REF | P_RO)));
	return (mach_pp->p_nrm & flag);
}

/*
 * Copy top level mapping elements (L1 ptes or whatever)
 * that map from saddr to (saddr + len) in sas
 * to top level mapping elements from daddr in das.
 *
 * Hat_share()/unshare() return an (non-zero) error
 * when saddr and daddr are not properly aligned.
 *
 * The top level mapping element determines the alignment
 * requirement for saddr and daddr, depending on different
 * architectures.
 *
 * When hat_share()/unshare() are not supported,
 * HATOP_SHARE()/UNSHARE() return 0
 */
/* ARGSUSED */
int
hat_share(struct hat *hat, caddr_t addr,
	struct hat *ism_hatid, caddr_t sptaddr, size_t size)
{
	return (0);
}

/*
 * Invalidate top level mapping elements in as
 * starting from addr to (addr + size).
 */
/* ARGSUSED */
void
hat_unshare(struct hat *hat, caddr_t addr, u_int size)
{
	cmn_err(CE_PANIC, "hat_unshare: not implemented \n");
}

/*
 * Get a hat structure from the freelist
 */
struct hat *
hat_gethat()
{
	struct hat *hat;

	mutex_enter(&hat_res_mutex);
	if ((hat = hatfree) == NULL)	/* "shouldn't happen" */
		panic("hat_gethat - out of hats");

	hatfree = hat->hat_next;
	hat->hat_next = NULL;

	mutex_exit(&hat_res_mutex);
	return (hat);
}

static void
hat_freehat(hat)
	register struct hat *hat;
{
	int i;

	mutex_enter(&hat->hat_mutex);

	mutex_enter(&hat_res_mutex);
	hat->hat_as = (struct as *)NULL;

	for (i = 0; i < HAT_PRIVSIZ; i++)
		hat->hat_data[i] = 0;

	mutex_exit(&hat->hat_mutex);

	hat->hat_next = hatfree;
	hatfree = hat;
	mutex_exit(&hat_res_mutex);
}

/*
 * Enter a hme on the mapping list for page pp
 */
void
hme_add(hme, pp)
	register struct hment *hme;
	register page_t *pp;
{
	ASSERT(ppcmmu_mlist_held(pp));

	hme->hme_prev = NULL;
	hme->hme_next = mach_pp->p_mapping;
	hme->hme_page = pp;
	if (mach_pp->p_mapping) {
		((struct hment *)mach_pp->p_mapping)->hme_prev = hme;
		ASSERT(mach_pp->p_share > 0);
	} else  {
		ASSERT(mach_pp->p_share == 0);
	}
	mach_pp->p_mapping = hme;
	mach_pp->p_share++;
}

/*
 * remove a hme from the mapping list for page pp
 */
void
hme_sub(hme, pp)
	register struct hment *hme;
	register page_t *pp;
{
	ASSERT(ppcmmu_mlist_held(pp));
	ASSERT(hme->hme_page == pp);

	if (mach_pp->p_mapping == NULL)
		panic("hme_sub - no mappings");

	ASSERT(mach_pp->p_share > 0);
	mach_pp->p_share--;

	if (hme->hme_prev) {
		ASSERT(mach_pp->p_mapping != hme);
		ASSERT(hme->hme_prev->hme_page == pp);
		hme->hme_prev->hme_next = hme->hme_next;
	} else {
		ASSERT(mach_pp->p_mapping == hme);
		mach_pp->p_mapping = hme->hme_next;
		ASSERT((mach_pp->p_mapping == NULL) ?
		    (mach_pp->p_share == 0) : 1);
	}

	if (hme->hme_next) {
		ASSERT(hme->hme_next->hme_page == pp);
		hme->hme_next->hme_prev = hme->hme_prev;
	}

	/*
	 * zero out the entry
	 */
	hme->hme_next = NULL;
	hme->hme_prev = NULL;
	hme->hme_hat = NULL;
	hme->hme_page = (page_t *)NULL;
}


void
hat_mlist_enter(pp)
	page_t *pp;
{
	if (pp)
		ppcmmu_mlist_enter(pp);
}

void
hat_mlist_exit(pp)
	page_t *pp;
{
	if (pp)
		ppcmmu_mlist_exit(pp);
}

int
hat_mlist_held(pp)
	page_t *pp;
{
	return (ppcmmu_mlist_held(pp));
}

void
hat_page_enter(pp)
	page_t *pp;
{
	ppcmmu_page_enter(pp);
}

void
hat_page_exit(pp)
	page_t *pp;
{
	ppcmmu_page_exit(pp);
}

void
hat_enter(struct hat *hat)
{
	ppcmmu_hat_enter(hat->hat_as);
}

void
hat_exit(struct hat *hat)
{
	ppcmmu_hat_exit(hat->hat_as);
}

void
hat_cachectl_enter(struct page *pp)
{
	ppcmmu_cachectl_enter(pp);
}

void
hat_cachectl_exit(pp)
	struct page *pp;
{
	ppcmmu_cachectl_exit(pp);
}

/*
 * Yield the memory claim requirement for an address space.
 *
 * This is currently implemented as the number of active hardware
 * translations that have page structures.  Therefore, it can
 * underestimate the traditional resident set size, eg, if the
 * physical page is present and the hardware translation is missing;
 * and it can overestimate the rss, eg, if there are active
 * translations to a frame buffer with page structs.
 * Also, it does not take sharing into account.
 */
size_t
hat_get_mapped_size(struct hat *hat)
{
	if (hat != NULL)
		return ((size_t)ptob(hat->ppc_rss));
	else
		return (0);
}

#define	ASCHUNK	64
/*
 * Kill process(es) that use the given page. (Used for parity recovery)
 * If we encounter the kernel's address space, give up (return -1).
 * Otherwise, we return 0.
 */
/* ARGSUSED */
hat_kill_procs(pp, addr)
	page_t	*pp;
	caddr_t	addr;
{
#ifdef NOTYET
	register struct sf_hment *hme;
	register struct hat *hat;
	struct	as	*as;
	struct	proc	*p;
	struct	as	*as_array[ASCHUNK];
	int	loop	= 0;
	int	opid	= -1;
	int	i;

	ppcmmu_mlist_enter(pp);
again:
	if (pp->p_mapping) {
		bzero((caddr_t)&as_array[0], ASCHUNK * sizeof (int));
		for (i = 0; i < ASCHUNK; i++) {
			hme = (struct sf_hment *)pp->p_mapping;
			hat = &hats[hme->hme_hat];
			as = hat->sfmmu_as;

			/*
			 * If the address space is the kernel's, then fail.
			 * The only thing to do with corrupted kernel memory
			 * is die.  The caller is expected to panic if this
			 * is true.
			 */
			if (as == &kas) {
				ppcmmu_mlist_exit(pp);
				printf("parity error: ");
				printf("kernel address space\n");
				return (-1);
			}
			as_array[i] = as;

			if (hme->hme_next)
				hme = hme->hme_next;
			else
				break;
		}
	}

	for (i = 0; i < ASCHUNK; i++) {

		as = as_array[i];
		if (as == NULL)
			break;

		/*
		 * Note that more than one process can share the
		 * same address space, if vfork() was used to create it.
		 * This means that we have to look through the entire
		 * process table and not stop at the first match.
		 */
		mutex_enter(&pidlock);
		for (p = practive; p; p = p->p_next) {
			k_siginfo_t siginfo;

			if (p->p_as == as) {
				/* limit messages about same process */
				if (opid != p->p_pid) {
					printf("parity error: killing pid %d\n",
					    (int)p->p_pid);
					opid =  p->p_pid;
					uprintf("pid %d killed: parity error\n",
					    (int)p->p_pid);
				}

				bzero((caddr_t)&siginfo, sizeof (siginfo));
				siginfo.si_addr = addr;
				siginfo.si_signo = SIGBUS;
				/*
				 * the following code should probably be
				 * something from siginfo.h
				 */
				siginfo.si_code = FC_HWERR;

				mutex_enter(&p->p_lock);
				sigaddq(p, NULL, &siginfo, KM_NOSLEEP);
				mutex_exit(&p->p_lock);
			}
		}
		mutex_exit(&pidlock);
	}

	/*
	 * Wait for previously signaled processes to die off,
	 * thus removing their mappings from the mapping list.
	 * XXX - change me to cv_timed_wait.
	 */
	(void) timeout(hat_kill_procs_wakeup, (caddr_t)&hat_kill_procs_cv, hz);

	mutex_enter(&hat_kill_procs_lock);
	cv_wait(&hat_kill_procs_cv, &hat_kill_procs_lock);
	mutex_exit(&hat_kill_procs_lock);

	/*
	 * More than ASCHUNK mappings on the list for the page,
	 * loop to kill off the rest of them.  This will terminate
	 * with failure if there are more than ASCHUNK*20 mappings
	 * or a process will not die.
	 */
	if (pp->p_mapping) {
		loop++;
		if (loop > 20) {
			ppcmmu_mlist_exit(pp);
			return (-1);
		}
		goto again;
	}
	ppcmmu_mlist_exit(pp);
#endif /* NOTYET */

	return (0);
}

/*
 * Initilaize locking for the hat layer, called early during boot.
 */
static void
ppcmmu_lock_init(void)
{
	u_int i;

	mutex_init(&ppcmmu_res_lock, NULL, MUTEX_DEFAULT, NULL);

	for (i = 0; i < SPL_TABLE_SIZE; i++) {
		mutex_init(&ppcmmu_page_lock[i], NULL, MUTEX_DEFAULT, NULL);
	}
}

/*
 * Initialize the hardware address translation structures.
 * Called after the vm structures have been allocated
 * and mapped in.
 *
 * The page table must be allocated by this time, we need to allocate
 * hments array, ptegps array and vsid-range allocation list and
 * bit map for vsid-range-inuse. It is assumed that ppcmmu_param_init()
 * has already been called.
 */
static void
ppcmmu_init(void)
{
	register int	i;

	/* the kernel page table should be allocated if we are here */
	ASSERT(nptegp != 0);

	/* allocate hments array */
	if ((hments = (struct hment *)
	    kmem_zalloc(sizeof (struct hment) * nptegp * NPTEPERPTEGP,
	    KM_NOSLEEP)) == NULL) {
		cmn_err(CE_PANIC, "Cannot allocate memory for hment structs");
	}
	ehments = hments + nptegp * NPTEPERPTEGP;

	/* allocate ptegps array */
	if ((ptegps = (struct ptegp *)
	    kmem_zalloc(sizeof (struct ptegp) * nptegp, KM_NOSLEEP)) == NULL) {
		cmn_err(CE_PANIC, "Cannot allocate memory for ptegp structs");
	}
	eptegps = ptegps + nptegp;
	/* initialize per ptegp mutexes */
	for (i = 0; i < nptegp; i++) {
		mutex_init(&ptegps[i].ptegp_mutex, NULL, MUTEX_DEFAULT, NULL);
	}

	if (ppcmmu_max_vsidranges > MAX_VSIDRANGES)
		ppcmmu_max_vsidranges = MAX_VSIDRANGES;

	/* allocate the bit map for vsid-range-inuse */
	i = (ppcmmu_max_vsidranges + (NBINT - 1)) / NBINT;
	if ((vsid_bitmap = (u_int *) kmem_zalloc(i * NBPW, KM_NOSLEEP)) == NULL)
		cmn_err(CE_PANIC, "Cannot allocate memory for vsid_bitmap");
	/*
	 * Note:
	 * Assign the vsid range KERNEL_VSID_RANGE for the kernel use; We
	 * assume that the boot has used the same vsid-range-id when loading
	 * segment registers. So we just mark it as used and fix the vsid free
	 * list. We also mark vsid range id 0 as used to make it an invalid
	 * vsid range for allocation.
	 */
	SET_VSID_RANGE_INUSE(0);
	SET_VSID_RANGE_INUSE(KERNEL_VSID_RANGE);
	build_vsid_freelist(); /* build the initial vsid_alloc_set list */

	/*
	 * Allocate hash table for ptelock counts. The current allocation
	 * is based on the page table size with a ratio of 1:64 ptes.
	 * Since we keep counter for upto the lock count of 2 in the ptegp
	 * structure itself, this hash table should get used for fewer
	 * locked pages (e.g segkmap segment pages, pages from user threads
	 * doing physio on the same page, etc.).
	 */
	ptelock_hashtab_size = (nptegp * NPTEPERPTEGP)/ptetohash_ratio;
	if ((ptelock_hashtab = (struct ptelock_hash *)
	    kmem_zalloc(sizeof (struct ptelock_hash) * ptelock_hashtab_size,
	    KM_NOSLEEP)) == NULL) {
	    cmn_err(CE_PANIC, "Cannot allocate memory for ptelock_hashtab[]");
	}
	ptelock_hash_mask = ptelock_hashtab_size - 1;
}

/*
 * Allocate ppcmmu specific hat data for this address space. The only resource
 * that needs to be allocated is vsid-range-id for this address space.
 */
static void
ppcmmu_alloc(register struct hat *hat, register struct as *as)
{
	register struct vsid_alloc_set *p;
	static int vsid_gc_initialized = 0;
	static int kas_hat_initialized = 0;

	if (vsid_valid(hat->hat_data[0]))
		return;

	/*
	 * Initialize VSID range for the kernel address space if it is not
	 * already done.
	 */
	if (as == &kas && !kas_hat_initialized) {
		kas_hat_initialized = 1;
		hat->hat_data[0] = KERNEL_VSID_RANGE;
		return;
	}

	mutex_enter(&ppcmmu_res_lock);

	p = vsid_alloc_head;
	if (p != NULL) {
		ASSERT(p->vs_nvsid_ranges != 0);
		hat->hat_data[0] = p->vs_vsid_range_id;
		ASSERT(!IS_VSID_RANGE_INUSE(p->vs_vsid_range_id));
		SET_VSID_RANGE_INUSE(p->vs_vsid_range_id);
		p->vs_nvsid_ranges--;
		p->vs_vsid_range_id ++;
		/*
		 * if this set is empty then free the set structure.
		 */
		if (p->vs_nvsid_ranges == 0) {
			vsid_alloc_head = p->vs_next;
			kmem_free(p, sizeof (struct vsid_alloc_set));
		}
	} else {
		/* No free vsid range to use, let the thread fault on it. */
		hat->hat_data[0] = (u_int)VSIDRANGE_INVALID;
		ppcmmu_vsid_wanted++;
	}

	/*
	 * If we allocated the last vsid_range then wakeup the kernel thread
	 * ppcmmu_vsid_gc().
	 */
	if (vsid_alloc_head == NULL) {
		if (vsid_gc_initialized)
			cv_broadcast(&ppcmmu_cv);
		else {
			/*
			 * Create the vsid_gc thread and run it.
			 * XXXPPC check the thread priority?
			 */
			if (thread_create(NULL, NULL, ppcmmu_vsid_gc,
			    NULL, 0, &p0, TS_RUN, minclsyspri) == NULL) {
				cmn_err(CE_PANIC,
					"ppcmmu_alloc: thread_create");
			}
			vsid_gc_initialized = 1;
		}
		vmhatstat.vh_vsid_gc_wakeups.value.ul++;
	}
	mutex_exit(&ppcmmu_res_lock);
}

/*
 * Free all the translation resources for the specified address space.
 * Called from as_free when an address space is being destroyed.
 *
 * On PowerPC the VSID range is unique and by invalidating its use the
 * translations are ineffective. The kernel thread ppcmmu_vsid_gc()
 * unloads the stale translations before the VSID range can be reused.
 */
/* ARGSUSED */
void
ppcmmu_free(register struct hat *hat, register struct as *as)
{
	mutex_enter(&hat->hat_mutex);
	mutex_enter(&ppcmmu_res_lock);
	ASSERT(vsid_valid(hat->hat_data[0]));
	CLEAR_VSID_RANGE_INUSE(hat->hat_data[0]);
	hat->hat_data[1] = (u_int)-1;	/* mark that ppcmmu_free() is called */
	mutex_exit(&ppcmmu_res_lock);
	mutex_exit(&hat->hat_mutex);
}

/*
 * As with SRMMU implementation, we do not duplicate any translations
 * when we are duplicating the address space. Rather, we let the
 * forked process fault in the mappings that the parent process
 * had.
 */
/* ARGSUSED */
int
ppcmmu_dup(struct hat *hat, struct as *as, struct as *newas)
{
	return (0);
}

/*
 * Set up any translation structures, for the specified address space,
 * that are needed or preferred when the process is being swapped in.
 */
/* ARGSUSED */
void
ppcmmu_swapin(struct hat *hat, struct as *as)
{
}

/*
 * Free all of the translation resources, for the specified address space,
 * that can be freed while the process is swapped out. Called from as_swapout.
 *
 * Note: Needs work in the common swapout code to make this work.
 */
/* ARGSUSED */
void
ppcmmu_swapout(struct hat *hat, struct as *as)
{
}

/*
 * Set up addr to map to page pp with protection prot.
 */
/* ARGSUSED */
void
ppcmmu_memload(struct hat *hat, struct as *as, caddr_t addr,
	struct page *pp, u_int attr, int flags)
{
	struct pte apte;

	ASSERT(se_assert(&pp->p_selock));

	ppcmmu_mempte(hat, pp, attr & HAT_PROT_MASK, &apte, addr);
	ppcmmu_mlist_enter(pp);
	mutex_enter(&hat->hat_mutex);
	ppcmmu_pteload(as, addr, pp, &apte, flags, attr);
	mutex_exit(&hat->hat_mutex);
	ppcmmu_mlist_exit(pp);
}

/*
 * Cons up a struct pte using the device's pf bits and protection
 * prot to load into the hardware for address addr; treat as minflt.
 *
 * Note: hat_devload can be called to map real memory (e.g.
 * /dev/kmem) and even though hat_devload will determine pf is
 * for memory, it will be unable to get a shared lock on the
 * page (because someone else has it exclusively) and will
 * pass dp = NULL.  If ppcmmu_pteload doesn't get a non-NULL
 * page pointer it can't cache memory.  Since all memory must
 * always be cached for ppc, we call pf_is_memory to cover
 * this case.
 */
/* ARGSUSED */
void
ppcmmu_devload(struct hat *hat, struct as *as, caddr_t addr, devpage_t *dp,
	u_int pf, u_int attr, int flags)
{
	struct	pte pte;
	u_int prot = attr & HAT_PROT_MASK;

#if defined(_NO_LONGLONG)
	*(u_long *)&pte = MMU_INVALID_PTE;
	*((u_long *)&pte+1) = MMU_INVALID_PTE;
#else
	*(u_longlong_t *)&pte = MMU_INVALID_PTE;
#endif
	pte.pte_ppn = pf;
	pte.pte_pp = ppcmmu_vtop_prot(addr, prot);
	pte.pte_wimg = WIMG(pf_is_memory(pf), !mmu601);
	pte.pte_vsid = VSID(hat->hat_data[0], addr);
	pte.pte_valid = PTE_VALID;
	pte.pte_api = API((u_int)addr);

	if (dp != NULL)
		ppcmmu_mlist_enter(dp);
	mutex_enter(&hat->hat_mutex);
	ppcmmu_pteload(as, addr, (struct page *)dp, &pte, flags, attr);
	mutex_exit(&hat->hat_mutex);
	if (dp != NULL)
		ppcmmu_mlist_exit(dp);
}

#ifdef notdef
/*
 * Set up translations to a range of virtual addresses backed by
 * physically contiguous memory.
 *
 * Note: We can use this for BAT mappings.
 */
void
ppcmmu_contig_memload(struct hat *hat, struct as *as, caddr_t addr,
	struct page *pp, u_int attr, int flags, u_int len)
{
	struct pte apte;

	ASSERT((len & MMU_PAGEOFFSET) == 0);
	while (len) {
		ppcmmu_mempte(hat, pp, attr & HAT_PROT_MASK, &apte, addr);
		ppcmmu_mlist_enter(pp);
		mutex_enter(&hat->hat_mutex);
		ppcmmu_pteload(as, addr, pp, &apte, flags, attr);
		mutex_exit(&hat->hat_mutex);
		ppcmmu_mlist_exit(pp);
		pp = page_next(pp);
		addr += MMU_PAGESIZE;
		len -= MMU_PAGESIZE;
	}
}
#endif

/*
 * Release one hardware address translation lock on the given address.
 * This means clearing the lock bit(s) for the translation(s) in the PTEGP(s).
 */
static void
ppcmmu_unlock(struct hat *hat, struct as *as, caddr_t addr, u_int len)
{
	register hwpte_t *pte;
	register struct ptegp *ptegp;
	register caddr_t a;

	ASSERT(as->a_hat == hat);
	ASSERT((len & MMU_PAGEOFFSET) == 0);

	mutex_enter(&hat->hat_mutex);

	for (a = addr; a < addr + len; a += MMU_PAGESIZE) {

		pte = ppcmmu_ptefind(as, a, PTEGP_LOCK);
		if (pte == NULL) {
			mutex_exit(&hat->hat_mutex);
			cmn_err(CE_PANIC, "ppcmmu_unlock: null pte");
		}
		/*
		 * Decrement the lock count for this mapping.
		 */
		ptegp = hwpte_to_ptegp(pte);
		pteunlock(ptegp, pte);
		mutex_exit(&ptegp->ptegp_mutex);
	}

	mutex_exit(&hat->hat_mutex);
}

/* ARGSUSED */
faultcode_t
ppcmmu_fault(struct hat *hat, caddr_t addr)
{
	return (FC_NOMAP);
}

/*
 * Change the protections in the virtual address range given to the
 * specified virtual protection.  If vprot is ~PROT_WRITE, then remove
 * write permission, leaving the other permissions unchanged.  If vprot
 * is ~PROT_USER, remove user permissions.
 *
 * Note: This function needs changes to support BAT mappings.
 */
void
ppcmmu_chgprot(struct as *as, caddr_t addr, u_int len, u_int vprot)
{
	register hwpte_t *pte;
	register u_int pprot;
	register caddr_t a, ea;
	register struct ptegp *ptegp;
	int newprot = 0;

	ASSERT(vsid_valid(as->a_hat->hat_data[0]));

	mutex_enter(&as->a_hat->hat_mutex);

	/*
	 * Convert the virtual protections to physical ones. We can do
	 * this once with the first address because the kernel won't be
	 * in the same segment with the user, so it will always be
	 * one or the other for the entire length. If vprot is ~PROT_WRITE,
	 * turn off write permission.
	 */
	if (vprot != (u_int)(~PROT_USER) && vprot != (u_int)(~PROT_WRITE) &&
		vprot != 0)
		pprot = ppcmmu_vtop_prot(addr, vprot);

	for (a = addr, ea = addr + len; a < ea; a += MMU_PAGESIZE) {
		/* find the pte and get the PTEGP mutex */
		if ((pte = ppcmmu_ptefind(as, a, PTEGP_LOCK)) == NULL)
			continue;
		ptegp = hwpte_to_ptegp(pte);
		if (vprot == (u_int)(~PROT_WRITE)) {
			switch (pp_from_hwpte(pte)) {
			case MMU_STD_SRWXURWX:
				pprot = MMU_STD_SRXURX;
				newprot = 1;
				break;
			case MMU_STD_SRWX:
				/*
				 * on PPC we can't write protect kernel
				 * without giving read access to user.
				 */
				newprot = 0;
				break;
			}
		} else if (vprot == (u_int)(~PROT_USER)) {
			switch (pp_from_hwpte(pte)) {
			case MMU_STD_SRWXURWX:
			case MMU_STD_SRXURX:
			case MMU_STD_SRWXURX:
				pprot = MMU_STD_SRWX;
				newprot = 1;
				break;
			}
		} else if (pp_from_hwpte(pte) != pprot)
			newprot = 1;
		if (newprot) {
			mmu_update_pte_prot(pte, pprot, a);
			newprot = 0;
		}
		/*
		 * On the machines with split I/D caches we need to
		 * flush the I-cache for this page. The algorithm used to
		 * determine flushing:
		 *	if new protection specifies PROT_EXEC then
		 *		if page is marked 'not-flushed' then
		 *			do 'dcbst' and 'icbi' on the page.
		 *			mark the page as 'flushed'
		 *	if new protection specifies PROT_WRITE then
		 *		mark the page as 'not-flushed'
		 */
		if (!unified_cache) {
			register page_t *pp;

			pp = hwpte_to_hme(pte)->hme_page;
			if (pp) {
				if ((vprot & PROT_EXEC) &&
					!PAGE_IS_FLUSHED(mach_pp)) {
					ppcmmu_cache_flushpage(as->a_hat, a);
					SET_PAGE_FLUSHED(mach_pp);
				}
				if (vprot & PROT_WRITE)
					CLR_PAGE_FLUSHED(mach_pp);
			}
		}
		mutex_exit(&ptegp->ptegp_mutex);
	}

	mutex_exit(&as->a_hat->hat_mutex);
}

/*
 * Unload all the mappings in the range [addr..addr+len).
 * addr and len must be MMU_PAGESIZE aligned.
 *
 * Algorithm:
 *	Get hat mutex.
 *	Loop:
 *		Search for PTE and get the ptegp mutex if found.
 *		Mark hme as BUSY (to avoid hme stealing).
 *		If pp is not NULL then:
 *			Release the ptegp mutex.
 *			Get the mlist lock.
 *			Get the ptegp_mutex lock.
 *		Call ppcmmu_pteunload() to unload the mapping.
 *		Release the ptegp_mutex lock.
 *		If pp is not NULL then release mlist lock.
 *	Release hat mutex.
 *
 */
void
ppcmmu_unload(struct as *as, caddr_t addr, u_int len, int flags)
{
	register hwpte_t *pte;
	register caddr_t a, ea;
	register struct ptegp *ptegp;
	register struct hment *hme;
	register int as_freeing = 0;
	register struct hat *hat;
	struct page *pp;
	u_int vsidrange;

	ASSERT((len & MMU_PAGEOFFSET) == 0);

	if (flags & HAT_UNLOAD_UNMAP)
		flags = (flags & ~HAT_UNLOAD_UNMAP) | HAT_UNLOAD;

	hat = as->a_hat;
	as_freeing = hat->hat_data[1]; /* ppcmmu_free() called for this as? */
	vsidrange = hat->hat_data[0];
	mutex_enter(&hat->hat_mutex);

	for (a = addr, ea = addr + len; a < ea; a += MMU_PAGESIZE) {
		/* find the pte and get the PTEGP mutex */
		if ((pte = ppcmmu_ptefind(as, a, PTEGP_LOCK)) == NULL) {
			continue;
		}
		ptegp = hwpte_to_ptegp(pte);
		hme = hwpte_to_hme(pte);
		hme->hme_impl |= HME_BUSY;

		if ((flags & HAT_UNLOAD_UNLOCK) && (hwpte_valid(pte))) {
			pteunlock(ptegp, pte);
		}
		/*
		 * If we are trying to unload mapping(s) for an 'as' for
		 * which ppcmmu_free() is already done then make sure we
		 * are looking at the pte that belongs to this 'as'.
		 */
		if (as_freeing && (hat != &hats[hme->hme_hat])) {
			mutex_exit(&ptegp->ptegp_mutex);
			continue;
		}
		pp = hme->hme_page;
		if (pp) {
			/*
			 * Acquire the locks in the correct order.
			 */
			mutex_exit(&ptegp->ptegp_mutex);
			mutex_exit(&hat->hat_mutex);
			ppcmmu_mlist_enter(pp);
			mutex_enter(&hat->hat_mutex);
			mutex_enter(&ptegp->ptegp_mutex);
			/*
			 * make sure the mapping hasn't been removed/replaced
			 * while we were trying to get the locks.
			 */
			if (hwpte_valid(pte) && ((vsid_from_hwpte(pte) >>
				VSIDRANGE_SHIFT) == vsidrange) &&
				(!as_freeing || (hat == &hats[hme->hme_hat])))
				ppcmmu_pteunload(ptegp, pte, pp, hme, flags |
					(hme->hme_nosync ? PPCMMU_NOSYNC :
					PPCMMU_RMSYNC));
			mutex_exit(&ptegp->ptegp_mutex);
			ppcmmu_mlist_exit(pp);
		} else {
			ppcmmu_pteunload(ptegp, pte, pp, hme, flags |
			    (hme->hme_nosync ? PPCMMU_NOSYNC : PPCMMU_RMSYNC));
			mutex_exit(&ptegp->ptegp_mutex);
		}
	}

	mutex_exit(&hat->hat_mutex);
}

/*
 * Unload a hardware translation that maps page `pp'.
 */
static void
ppcmmu_pageunload(struct page *pp, struct hment *hme)
{
	hwpte_t *pte;
	struct hat *hat;
	struct ptegp *ptegp;

	ASSERT(ppcmmu_mlist_held(pp));

	hat = &hats[hme->hme_hat];
	mutex_enter(&hat->hat_mutex);
	hme->hme_impl |= HME_BUSY;
	ptegp = hme_to_ptegp(hme);
	pte = hme_to_hwpte(hme);
	mutex_enter(&ptegp->ptegp_mutex);
	ppcmmu_pteunload(ptegp, pte, pp, hme, PPCMMU_RMSYNC);
	mutex_exit(&ptegp->ptegp_mutex);
	mutex_exit(&hat->hat_mutex);
}


/*
 * Get all the hardware dependent attributes for a page struct
 */
static void
ppcmmu_pagesync(struct hat *hat, struct page *pp, struct hment *hme,
	u_int clearflag)
{
	hwpte_t *pte;
	struct ptegp *ptegp;

	ASSERT(ppcmmu_mlist_held(pp));
	ASSERT(se_assert(&pp->p_selock));

	mutex_enter(&hat->hat_mutex);
	pte = hme_to_hwpte(hme);
	ptegp = hwpte_to_ptegp(pte);
	/*
	 * If the mapping is stale (i.e ppcmmu_free() didn't unload the mappings
	 * but the page scanner is checking those mappings) then unload
	 * the mapping.
	 */
	mutex_enter(&ptegp->ptegp_mutex);
	if (!IS_VSID_RANGE_INUSE(hat->hat_data[0])) {
		ppcmmu_pteunload(ptegp, pte, pp, hme,
				clearflag ? PPCMMU_RMSYNC : PPCMMU_RMSTAT);
	} else {
		ppcmmu_ptesync(ptegp, pte, hme,
				clearflag ? PPCMMU_RMSYNC : PPCMMU_RMSTAT);
	}
	mutex_exit(&ptegp->ptegp_mutex);
	mutex_exit(&hat->hat_mutex);
}

/*
 * Returns a page frame number for a given kernel virtual address.
 *
 * Return -1 to indicate an invalid mapping (needed by kvtoppid)
 */
u_int
ppcmmu_getkpfnum(caddr_t addr)
{
	register struct spte *spte;
	struct batinfo *bat;
	extern struct batinfo *ppcmmu_findbat(caddr_t);

	if (addr >= Sysbase && addr < Syslimit) {
		spte = &Sysmap[mmu_btop(addr - Sysbase)];
		if (spte_valid(spte))
			return (spte->spte_ppn);
	}

	/* search the BAT mappings first */
	bat = ppcmmu_findbat(addr);
	if (bat)
		return ((u_int)(bat->batinfo_paddr +
			(addr - bat->batinfo_vaddr)) >> MMU_PAGESHIFT);
	else
		/* no BAT mapping found, search in the Page Table */
		return (ppcmmu_getpfnum(&kas, addr));
}

/*
 * ppcmmu_getpfnum(as, addr)
 *	Given the address of an address space returns the physcial page number
 *	of that address. Returns -1 if no mapping found.
 *
 * Note: Needs work to support Bat mappings.
 */
u_int
ppcmmu_getpfnum(struct as *as, caddr_t addr)
{
	register hwpte_t *hwpte;
	u_int pfn;

	mutex_enter(&as->a_hat->hat_mutex);
	hwpte = ppcmmu_ptefind(as, addr, PTEGP_LOCK);
	if (hwpte) {
		/* valid mapping found */
		pfn = ppn_from_hwpte(hwpte);
		mutex_exit(&hwpte_to_ptegp(hwpte)->ptegp_mutex);
		mutex_exit(&as->a_hat->hat_mutex);
		return (pfn);
	}
	mutex_exit(&as->a_hat->hat_mutex);

	return ((u_int)-1); /* no valid mapping found */
}

/*
 * Make all the mappings to page 'pp' non-cached/cached.
 */
/*ARGSUSED1*/
static void
ppcmmu_pagecachectl(struct page *pp, u_int flag)
{
	struct hment *hme;
	struct hat *hat;
	register hwpte_t *pte;
	register struct ptegp *ptegp;
	register caddr_t vaddr;
	register u_int owimg, nwimg;
	int flushed = 0; /* flag to indicate if the page is flushed */

	ASSERT(se_assert(&pp->p_selock) && ppcmmu_mlist_held(pp));

	for (hme = mach_pp->p_mapping; hme; hme = hme->hme_next) {
		hat = &hats[hme->hme_hat];
			pte = hme_to_hwpte(hme);
			ptegp = hme_to_ptegp(hme);
			mutex_enter(&ptegp->ptegp_mutex);
			owimg = wimg_from_hwpte(pte);
			if ((owimg & WIMG_CACHE_DIS) &&
			    (!PP_ISNC(mach_pp))) {
				/*
				 * Convert non-cached translation to cached
				 * translation. We need to flush the TLB
				 * entry.
				 */
				nwimg = WIMG(1, 0);
				vaddr = hwpte_to_vaddr(pte);
				mmu_update_pte_wimg(pte, nwimg, vaddr);
			} else if (!(owimg & WIMG_CACHE_DIS) &&
					(PP_ISNC(mach_pp))) {
				/*
				 * Convert cached translation to non-cached
				 * translation. We need to flush the cache
				 * and the TLB entry for this.
				 */
				nwimg = WIMG(0, !mmu601);
				vaddr = hwpte_to_vaddr(pte);
				mmu_update_pte_wimg(pte, nwimg, vaddr);
			}
			/*
			 * Flush the cache if necessary.
			 */
			if (!unified_cache && !flushed) {
				ppcmmu_cache_flushpage(hat, vaddr);
				SET_PAGE_FLUSHED(mach_pp);
				flushed = 1;
			}
			mutex_exit(&ptegp->ptegp_mutex);
	}
}

/*
 * For compatibility with AT&T and later optimizations
 */
/*ARGSUSED*/
static int
ppcmmu_map(struct hat *hat, struct as *as, caddr_t addr, u_int len, int	flags)
{
	return (0);
}

/*
 * End of machine independent interface routines.
 *
 * The next few routines implement some machine dependent functions
 * needed for the PPCMMU. Note that each hat implementation can define
 * whatever additional interfaces make sense for that machine.
 *
 * Start machine specific interface routines.
 */

/*
 * Called by UNIX during pagefault to insure that the hat data structures are
 * setup for this address space.
 *
 * If the address space didn't have a valid vsid-range (VSIDRANGE_INVALID)
 * and allocflag is set then we try to allocate a vsid-range for this
 * address space. And reload the segment registers for the new vsid.
 */
static struct as *
ppcmmu_setup(register struct as *as, int allocflag)
{
	/*
	 * kas is already set up, it is part of every address space,
	 * so we can stay in whatever context that is currently active.
	 */
	if (as == &kas)
		return (NULL);

	ASSERT(MUTEX_HELD(&as->a_hat->hat_mutex));

	if (!vsid_valid(as->a_hat->hat_data[0]) && allocflag)
		ppcmmu_alloc(as->a_hat, as);

	kpreempt_disable();
	/* reload segment registers for this address space */
	mmu_segload(as->a_hat->hat_data[0]);
	kpreempt_enable();

	return (NULL);
}

/*
 * Returns a pointer to the hwpte if there is a valid mapping for the virtual
 * address. Also if the 'lockneeded' flag is TRUE then it returns with the
 * PTEGP mutex held. It returns NULL if no valid mapping exists.
 */
hwpte_t *
ppcmmu_ptefind(struct as *as, register caddr_t addr, int lockflag)
{
	register hwpte_t *primary_pteg;
	register hwpte_t *secondary_pteg;
	register u_int vsid;
	register u_int api;
	register hwpte_t *hwpte;
	register struct ptegp *ptegp;
	u_int hash1;

	ASSERT(MUTEX_HELD(&as->a_hat->hat_mutex));

	/*
	 * If the address space doesn't have a valid VSID then return.
	 */
	if (!vsid_valid(as->a_hat->hat_data[0])) {
		return (NULL);
	}

	/* search the primary PTEG */
	vsid = (as->a_hat->hat_data[0] << VSIDRANGE_SHIFT) +
			((u_int)addr >> SEGREGSHIFT);
	/* compute the primary hash value */
	hash1 = ((vsid & HASH_VSIDMASK) ^
		(((u_int)addr >> MMU_STD_PAGESHIFT) & HASH_PAGEINDEX_MASK));
	api = (((u_int)addr >> APISHIFT) & APIMASK);
	primary_pteg = hash_get_primary(hash1);
	ptegp = hwpte_to_ptegp(primary_pteg);

	mutex_enter(&ptegp->ptegp_mutex);
	/* search the primary PTEG for the mapping */
	if (!(hwpte = find_pte(primary_pteg, vsid, api, H_PRIMARY))) {
		/* no mapping in the primary PTEG, search the secondary PTEG */
		secondary_pteg = hash_get_secondary(primary_pteg);
		hwpte = find_pte(secondary_pteg, vsid, api, H_SECONDARY);
	}
	if ((hwpte == NULL) || (lockflag == PTEGP_NOLOCK))
		mutex_exit(&ptegp->ptegp_mutex);

	return (hwpte);
}

/*
 * This routine converts virtual page protections to physical ones.
 */
u_int
ppcmmu_vtop_prot(caddr_t addr, register u_int vprot)
{
	if ((vprot & PROT_ALL) != vprot) {
		cmn_err(CE_PANIC, "ppcmmu_vtop_prot -- bad prot %x", vprot);
	}

	if (vprot & PROT_USER) { /* user permission */
		if (addr >= (caddr_t)KERNELBASE) {
			cmn_err(CE_PANIC,
				"user addr %x vprot %x in kernel space\n",
				addr, vprot);
		}
	}

	switch (vprot) {
	case PROT_READ:
		/*
		 * XXXPPC TEMPORARY WORKAROUND FOR UFS_HOLE PROBLEM.
		 *
		 * If the page is for read-only and it is in the segkmap
		 * segment (used by filesystem code) to map HOLEs in the
		 * file with page of zeros and expect write faults then
		 * the best we can do is to give kernel-read-user-read
		 * protection on this page.
		 */
		if (addr >= segkmap->s_base &&
			(addr < (segkmap->s_base + segkmap->s_size)))
			return (MMU_STD_SRXURX);
		/*FALLTHROUGH*/
	case 0:
	case PROT_USER:
		/* Best we can do for these is kernel access only */
	case PROT_EXEC:
	case PROT_WRITE:
	case PROT_READ | PROT_EXEC:
	case PROT_WRITE | PROT_EXEC:
	case PROT_READ | PROT_WRITE:
	case PROT_READ | PROT_WRITE | PROT_EXEC:
		return (MMU_STD_SRWX);
	case PROT_EXEC | PROT_USER:
	case PROT_READ | PROT_USER:
	case PROT_READ | PROT_EXEC | PROT_USER:
		return (MMU_STD_SRXURX);
	case PROT_WRITE | PROT_USER:
	case PROT_READ | PROT_WRITE | PROT_USER:
	case PROT_WRITE | PROT_EXEC | PROT_USER:
	case PROT_READ | PROT_WRITE | PROT_EXEC | PROT_USER:
		return (MMU_STD_SRWXURWX);
	default:
		cmn_err(CE_PANIC, "ppcmmu_vtop_prot: bad vprot %x", vprot);
		/* NOTREACHED */
	}
}

u_int
ppcmmu_ptov_prot(register hwpte_t *ppte)
{
	register u_int vprot;

	if (!hwpte_valid(ppte)) {
		vprot = 0;
	} else {
		switch (pp_from_hwpte(ppte)) {
		case MMU_STD_SRXURX:
			vprot = PROT_READ | PROT_EXEC | PROT_USER;
			break;
		case MMU_STD_SRWXURX:
			vprot = PROT_READ | PROT_WRITE | PROT_EXEC | PROT_USER;
			break;
		case MMU_STD_SRWXURWX:
			vprot = PROT_READ | PROT_WRITE | PROT_EXEC | PROT_USER;
			break;
		case MMU_STD_SRWX:
			vprot = PROT_READ | PROT_WRITE | PROT_EXEC;
			break;
		default:
			vprot = 0;
			break;
		}
	}
	return (vprot);
}

/* ARGSUSED */
static int
ppcmmu_probe(struct hat *hat, struct as *as, caddr_t addr)
{
	register hwpte_t *hwpte;

	if (addr >= (caddr_t)KERNELBASE)
		return (ppcmmu_getkpfnum(addr) == -1 ? 0 : 1);

	mutex_enter(&hat->hat_mutex);
	hwpte = ppcmmu_ptefind(as, addr, PTEGP_NOLOCK);
	mutex_exit(&hat->hat_mutex);
	return (hwpte ? 1 : 0);
}

/*
 * Construct a pte for a page.
 */
void
ppcmmu_mempte(struct hat *hat, register struct page *pp, u_int vprot,
	register struct pte *pte, caddr_t addr)
{
	ASSERT(se_assert(&pp->p_selock));

#if defined(_NO_LONGLONG)
	*(u_long *)pte = MMU_INVALID_PTE;
	*((u_long *)pte+1) = MMU_INVALID_PTE;
#else
	*(u_longlong_t *)pte = MMU_INVALID_PTE;
#endif
	pte->pte_ppn = page_pptonum(pp);
	pte->pte_wimg = WIMG(pf_is_memory(pte->pte_ppn), !mmu601);
	pte->pte_pp = ppcmmu_vtop_prot(addr, vprot);
	pte->pte_vsid = VSID(hat->hat_data[0], addr);
	pte->pte_valid = PTE_VALID;
	pte->pte_api = API((u_int)addr);
}

/*
 * Loads the pte for the given address with the given pte. Also sets it
 * up as a mapping for page pp, if there is one.  The lock bit in the
 * PTEGP structure is set if the translation is to be locked.
 *
 * Locking:
 *	1. At the entry hat_mutex and mlist lock (if pp != NULL) are
 *	   already held.
 *	2. ptegp mutex is held until we enter the new mapping.
 */
static void
ppcmmu_pteload(struct as *as, caddr_t addr, struct page *pp,
	struct pte *pte, int flags, u_int attr)
{
	register hwpte_t *hwpte;
	register hwpte_t *ppteg;
	register hwpte_t *spteg;
	register struct ptegp *ptegp;
	register struct hment *hme;
	register struct hat *hat;
	int remap = 0;
	int hbit;
	u_int hash1;
	int do_flush = 0;
	u_int prot = attr & HAT_PROT_MASK;

	ASSERT(as != NULL);
	ASSERT(MUTEX_HELD(&as->a_hat->hat_mutex));
	ASSERT((pp == NULL) || ppcmmu_mlist_held(pp));

	vmhatstat.vh_pteload.value.ul++;

	hat = as->a_hat;

try_again:
	/*
	 * Make sure the vsid-range is setup for the address space.
	 */
	if (!vsid_valid(hat->hat_data[0])) {
		ppcmmu_alloc(hat, as);
		if (!vsid_valid(hat->hat_data[0])) {
			/*
			 * Can't allocate vsid now, wait until vsid_gc
			 * tells us to try again.
			 */
			if (pp)
				ppcmmu_mlist_exit(pp);
			cv_wait(&ppcmmu_vsid_cv, &hat->hat_mutex);
			if (pp) {
				mutex_exit(&hat->hat_mutex);
				ppcmmu_mlist_enter(pp);
				mutex_enter(&hat->hat_mutex);
			}
			goto try_again;
		}
		ppcmmu_setup(as, 0);
	}
	pte->pte_vsid = VSID(hat->hat_data[0], addr);

	hwpte = ppcmmu_ptefind(as, addr, PTEGP_LOCK);

	if (hwpte) {
		hme = hwpte_to_hme(hwpte);
		remap = 1;
		ASSERT(hme->hme_valid);
		ptegp = hwpte_to_ptegp(hwpte);
	}

	if (pp != NULL) {
		pte->pte_wimg = WIMG(!PP_ISNC(mach_pp), !mmu601);
		if (remap && (ppn_from_hwpte(hwpte) != pte->pte_ppn)) {
			mutex_exit(&ptegp->ptegp_mutex);
			mutex_exit(&hat->hat_mutex);
			cmn_err(CE_PANIC, "ppcmmu_pteload: remap page");
		}
		/*
		 * For split I/D caches we may need to flush the I-cache.
		 * Determine if we need to do this according to the
		 * algorithm.
		 *	if this is the first mapping to the page then
		 *		if mapping with PROT_EXEC then
		 *			do_flush = 1.
		 *		else
		 *			mark the page as 'not-flushed'
		 *	Else if mapping PROT_EXEC and page not flushed
		 *	before then
		 *		do_flush = 1
		 */
		if (!unified_cache) {
			if (mach_pp->p_mapping == NULL) {
				if (prot & PROT_EXEC)
					do_flush = 1;
				else
					CLR_PAGE_FLUSHED(mach_pp);
			} else {
				if ((prot & PROT_EXEC) &&
					!(PAGE_IS_FLUSHED(mach_pp)))
					do_flush = 1;
			}
		}
	}

	if (remap == 0) {
		/*
		 * New mapping. Try to find a free slot in primary PTEG.
		 * If no free slot found then try in the secondary PTEG.
		 * If secondary PTEG is also full then try to steal a slot
		 * from the primary PTEG and if that fails try to steal
		 * a slot from secondary PTEG.
		 */
		hash1 = ((pte->pte_vsid & HASH_VSIDMASK) ^
			(((u_int)addr >> MMU_STD_PAGESHIFT) &
						HASH_PAGEINDEX_MASK));
		ppteg = hash_get_primary(hash1); /* primary PTEG address */
		ptegp = hwpte_to_ptegp(ppteg);
		mutex_enter(&ptegp->ptegp_mutex);
		hwpte = find_free_pte(hat, ppteg, ptegp, pp);
		if (hwpte)
			hbit = H_PRIMARY;
		else {
			spteg = hash_get_secondary(ppteg);
			hwpte = find_free_pte(hat, spteg, ptegp, pp);
			if (hwpte)
				hbit = H_SECONDARY;
			else {
				/* no free slot in either PTEG, steal one */
				hwpte = steal_pte(hat, ppteg, ptegp);
				if (!hwpte) {
					/* try to steal from secondary PTEG */
					hwpte = steal_pte(hat, spteg,
								ptegp);
					hbit = H_SECONDARY;
				}
				else
					hbit = H_PRIMARY;
				if (!hwpte) {
					/*
					 * Could not steal a slot, PANIC?
					 * or try again?
					 */
					mutex_exit(&ptegp->ptegp_mutex);
					mutex_exit(&hat->hat_mutex);
					cmn_err(CE_PANIC,
					    "ppcmmu_pteload: pte overflow!!!");
				}
				vmhatstat.vh_pteoverflow.value.ul++;
			}
		}
		pte->pte_h = hbit;
		hme = hwpte_to_hme(hwpte);
		ASSERT((hme->hme_valid == 0) && (hme->hme_next != hme));
	} else
		pte->pte_h = hbit_from_hwpte(hwpte); /* copy h bit from hwpte */

	/* update the pte */
	mmu_update_pte(hwpte, pte, remap, addr);

	/* Do the I-cache flush if necessary */
	if (do_flush) {
		ppcmmu_cache_flushpage(hat, addr);
		if (prot & PROT_WRITE)
			/*
			 * as long as the page is writable mark the page as
			 * 'not-flushed'.
			 */
			CLR_PAGE_FLUSHED(mach_pp);
		else
			SET_PAGE_FLUSHED(mach_pp);
	}

	/*
	 * If we need to lock, increment the lock count in the lockmap
	 * and/or update the hash table entry.
	 */
	if (flags & HAT_LOAD_LOCK)
		ptelock(ptegp, hwpte);

	if (!remap) {
		int mask;

		mask = ((hwpte >= mptes) ? 0x100 : 1);
		mask <<= ((u_int)hwpte & PTEOFFSET) >> PTESHIFT;
		ptegp->ptegp_validmap |= mask; /* update validmap bitmap */
	}

	if (pp != NULL && !remap) {
		hme_add(hme, pp);
		as->a_hat->ppc_rss += 1;
	}
	if (!remap) {
		hme->hme_hat = hat - hats;
		hme->hme_valid = 1;
	}
	hme->hme_impl &= ~HME_BUSY;

	/*
	 * Note whether this translation will be ghostunloaded or not.
	 */
	hme->hme_nosync = ((attr & HAT_NOSYNC) != 0);

	mutex_exit(&ptegp->ptegp_mutex);
}

/*
 * End machine specific interface routines.
 *
 * The remainder of the routines are private to this module and are
 * used by the routines above to implement a service to the outside
 * caller.
 *
 * Start private routines.
 */

/*
 * Unload the pte. If required, sync the referenced & modified bits.
 */
static void
ppcmmu_pteunload(register struct ptegp *ptegp, register hwpte_t *pte,
	register struct page *pp, register struct hment *hme, int flags)
{
	int mask;
	int lkcnt_shift;
	int count;

	ASSERT(hwpte_valid(pte));
	ASSERT((pp == NULL) || ppcmmu_mlist_held(pp));
	ASSERT(hme->hme_next != hme);
	ASSERT(hme->hme_valid);
	ASSERT(MUTEX_HELD(&hats[hme->hme_hat].hat_mutex));
	ASSERT(MUTEX_HELD(&ptegp->ptegp_mutex));

	/*
	 * Sync ref and mod bits back to the page and invalidate pte.
	 */
	ppcmmu_ptesync(ptegp, pte, hme, PPCMMU_INVSYNC | flags);

	ASSERT(!hwpte_valid(pte));

	/*
	 * Remove the pte from the list of mappings for the page.
	 */
	if (pp != NULL) {
		ASSERT(hme->hme_next != hme);
		hats[hme->hme_hat].ppc_rss -= 1;
		hme_sub(hme, pp);
	}
	hme->hme_valid = 0;
	hme->hme_nosync = 0;
	hme->hme_impl &= ~HME_BUSY;
	/*
	 * Update ptegp structure.
	 */
	mask = ((pte >= mptes) ? 0x100 : 1);
	mask <<= ((u_int)pte & PTEOFFSET) >> PTESHIFT;
	ptegp->ptegp_validmap &= ~mask; /* update validmap bitmap */
	/* update lock count bit map and delete any hash entry for this pte */
	if (pte >= mptes)
		lkcnt_shift = LKCNT_UPPER_SHIFT;
	else
		lkcnt_shift = LKCNT_LOWER_SHIFT;
	lkcnt_shift += (((u_int)pte & PTEOFFSET) >> PTESHIFT) * LKCNT_NBITS;
	count = (ptegp->ptegp_lockmap >> lkcnt_shift) & LKCNT_MASK;
	/*
	 * XXXPPC: Is it valid to ASSERT that the mapping has been unlocked
	 * (i.e lock count is zero) before unloading it?
	 *
	 * ASSERT(count == 0);
	 */
	if (count == LKCNT_HASHED)
		delete_ptelock(pte); /* free the hash table entry */
	ptegp->ptegp_lockmap &= ~(LKCNT_MASK << lkcnt_shift);
}

/*
 * Sync the referenced and modified bits of the page struct
 * with the pte. Clears the bits in the pte.
 * Also, synchronizes the Cacheable bit in the pte
 * with the noncacheable bit in the page struct.
 *
 * Any change to the PTE requires the TLBs to be
 * flushed, so subsequent accesses or modifies will
 * really cause the memory image of the PTE to be
 * modified.
 *
 * NOTE: Assumes that there is no activity
 * on the page while munging the TLB entry, too (ugh).
 */
static void
ppcmmu_ptesync(register struct ptegp *ptegp, register hwpte_t *pte,
	struct hment *hme, int flags)
{
	register struct page *pp;
	struct as *as;
	caddr_t vaddr;
	u_int rm;
	kmutex_t *spl;

	pp = hme->hme_page;
	ASSERT(MUTEX_HELD(&hats[hme->hme_hat].hat_mutex));
	ASSERT(&ptegp->ptegp_mutex);

	vaddr = hwpte_to_vaddr(pte);

	if (pp != NULL) {
		if (flags & (PPCMMU_RMSYNC | PPCMMU_RMSTAT)) {
			if (flags & PPCMMU_INVSYNC)
				rm = mmu_delete_pte(pte, vaddr);
			else if (flags & PPCMMU_RMSYNC)
				rm = mmu_pte_reset_rmbits(pte, vaddr);
			else
				rm = rmbits_from_hwpte(pte);

			if (rm) {
				as = hats[hme->hme_hat].hat_as;
				if ((flags & PPCMMU_RMSYNC) &&
						as->a_hat->s_rmstat) {

					hat_setstat(as, vaddr, PAGESIZE, rm);
				}
				if (!hme->hme_nosync) {
					spl = SPL_HASH(pp);
					mutex_enter(spl);
					if (rm & 0x2)
						PP_SETREF(mach_pp);
					if (rm & 0x1)
						PP_SETMOD(mach_pp);
					mutex_exit(spl);
				}
			}
		} else if (flags & PPCMMU_INVSYNC) {
			(void) mmu_delete_pte(pte, vaddr);
		}
	} else if (flags & PPCMMU_INVSYNC) {
		(void) mmu_delete_pte(pte, vaddr);
	}
}

static void
ppcmmu_sync(struct as *as, caddr_t addr, u_int len, u_int clearflag)
{
	register caddr_t	a, ea;
	register hwpte_t	*pte;
	struct ptegp		*ptegp;
	struct hment		*hme;

	mutex_enter(&as->a_hat->hat_mutex);

	for (a = addr, ea = addr + len; a < ea; a += MMU_PAGESIZE) {
		if ((pte = ppcmmu_ptefind(as, a, PTEGP_LOCK)) == NULL)
			continue;
		ptegp = hwpte_to_ptegp(pte);
		hme = hwpte_to_hme(pte);
		ppcmmu_ptesync(ptegp, pte, hme,
				clearflag ? PPCMMU_RMSYNC : PPCMMU_RMSTAT);
		mutex_exit(&ptegp->ptegp_mutex);
	}

	mutex_exit(&as->a_hat->hat_mutex);
}
/*
 * Locking primitves accessed by HATLOCK macros
 */

/* ARGSUSED */
static void
ppcmmu_page_enter(struct page *pp)
{
	mutex_enter(SPL_HASH(pp));
}

/* ARGSUSED */
static void
ppcmmu_page_exit(struct page *pp)
{
	mutex_exit(SPL_HASH(pp));
}

static void
ppcmmu_hat_enter(struct as *as)
{
	mutex_enter(&as->a_hat->hat_mutex);
}

static void
ppcmmu_hat_exit(struct as *as)
{
	mutex_exit(&as->a_hat->hat_mutex);
}

#ifdef DEBUG

#define	MLIST_ENTER_STAT() (vmhatstat.vh_mlist_enter.value.ul++)
#define	MLIST_WAIT_STAT() (vmhatstat.vh_mlist_enter_wait.value.ul++)
#define	MLIST_EXIT_STAT() (vmhatstat.vh_mlist_exit.value.ul++)
#define	MLIST_BROADCAST_STAT() (vmhatstat.vh_mlist_exit_broadcast.value.ul++)

#else

#define	MLIST_ENTER_STAT()
#define	MLIST_WAIT_STAT()
#define	MLIST_EXIT_STAT()
#define	MLIST_BROADCAST_STAT()

#endif

static void
ppcmmu_mlist_enter(struct page *pp)
{
	if (pp) {
		kmutex_t *spl;

		spl = SPL_HASH(pp);
		mutex_enter(spl);
		MLIST_ENTER_STAT();
		while (MLIST_INUSE(mach_pp)) {
			MLIST_WAIT_STAT();
			SET_MLIST_WANTED(mach_pp);
			cv_wait(&mach_pp->p_mlistcv, spl);
		}
		SET_MLIST_INUSE(mach_pp);
		mutex_exit(spl);
	}
}

static void
ppcmmu_mlist_exit(struct page *pp)
{
	if (pp) {
		kmutex_t *spl;

		spl = SPL_HASH(pp);
		mutex_enter(spl);
		MLIST_EXIT_STAT();
		if (MLIST_WANTED(mach_pp)) {
			MLIST_BROADCAST_STAT();
			cv_broadcast(&mach_pp->p_mlistcv);
		}
		CLR_MLIST_WANTED(mach_pp);
		CLR_MLIST_INUSE(mach_pp);
		mutex_exit(spl);
	}
}

static int
ppcmmu_mlist_held(struct page *pp)
{
	return (MLIST_INUSE(mach_pp));
}

static void
ppcmmu_cachectl_enter(struct page *pp)
{
	ppcmmu_mlist_enter(pp);
}

static void
ppcmmu_cachectl_exit(struct page *pp)
{
	ppcmmu_mlist_exit(pp);
}

/*ARGSUSED*/
int
ppcmmu_share(struct as *as, caddr_t addr, struct as *sptas, caddr_t sptaddr,
	u_int size)
{
	return (EINVAL);
}

/*ARGSUSED*/
void
ppcmmu_unshare(struct as *as, caddr_t addr, u_int size)
{ }

/*
 * Local functions for the HAT.
 */

/*
 * hwpte_to_ptegp(hwpte_t *)
 *	Given the address of hw pte in the ptes[] array return the address of
 *	the ptegp structure corresponding to this pte.
 */
ptegp_t *
hwpte_to_ptegp(hwpte_t *p)
{
	u_int i;

	ASSERT(p >= ptes && p < eptes);
	i = ((u_int)p >> 6) & nptegmask; /* PTEG number for this pte */
	if (i >= nptegp)
		return (&ptegps[(~i & nptegmask)]);
	else
		return (&ptegps[i]);
}

/*
 * hme_to_ptegp(struct hment *p)
 *	Given the address of hme return the address of the ptegp structure
 *	corresponding to this hme.
 */
ptegp_t *
hme_to_ptegp(struct hment *p)
{
	u_int i;

	i = (p - hments) >> 3; /* PTEG number for this hme */
	if (i >= nptegp)
		return (&ptegps[(~i & nptegmask)]);
	else
		return (&ptegps[i]);
}

/*
 * find_pte(pteg, vsid, api, h)
 *	Search the PTEG for a valid mapping given the vsid, api and H bit.
 *	Returns NULL if no PTE in the group matches.
 */
static hwpte_t *
find_pte(hwpte_t *pteg, u_int vsid, u_int api, u_int h)
{
	u_int pte_w0;
	register int i;

	pte_w0 = api | (h << PTEW0_HASH_SHIFT)  | PTEW0_VALID_MASK |
		(vsid << PTEW0_VSID_SHIFT);

	for (i = NPTEPERPTEG; i; i--) {
		if (pte_w0 == ((u_int *)pteg)[PTEWORD0])
			return (pteg);
		pteg++;
	}

	return (0);
}

/*
 * steal_pte(hat, pteg, ptegp)
 *	hat   - hat structure for the new mapping.
 *	pteg  - address of PTEG. (i.e PTE0).
 *	ptegp - pointer to ptegp structure.
 *
 * hat_mutex and ptegp_mutex are already held at the entry to this routine.
 *
 * Steal a PTE slot from the PTEG. Returns NULL if it cannot steal a slot
 * from the group, otherwise it returns pointer to the freed PTE slot.
 *
 * Algorithm:
 *	Loop (for NPTEPERPTEG times):
 *		If the PTE is not locked and hme is not busy (or not stolen
 *		in the previous scan) and can get mlist lock for the page
 *		then steal this slot, unload the mapping and return the
 *		pointer to this slot.
 *		Else continue the loop.
 *	Return NULL.
 *
 * Note: Locks should be acquired in the order:
 *		mlist_lock
 *		hat_mutex
 *		ptegp_mutex
 */
static hwpte_t *
steal_pte(struct hat *hat, register hwpte_t *pteg, register struct ptegp *ptegp)
{
	register struct hment *hme;
	register struct page *pp;
	register u_int lockmap;
	register u_int mask;
	register int i;
	register hwpte_t *p;
	int try_again = 0;

	ASSERT(MUTEX_HELD(&hat->hat_mutex));
	ASSERT(MUTEX_HELD(&ptegp->ptegp_mutex));
	ASSERT(((u_int)pteg & PTEOFFSET) == 0);

Loop:
	lockmap = ptegp->ptegp_lockmap;
	mask = ((pteg >= mptes) ?
		(LKCNT_MASK << LKCNT_UPPER_SHIFT) : LKCNT_MASK);
	p = pteg;
	for (i = NPTEPERPTEG; i; i--, mask <<= LKCNT_SHIFT, p++) {
		if (lockmap & mask)
			continue;		/* pte is locked */
		hme = hwpte_to_hme(p);
		if (hme->hme_impl & HME_BUSY)
			continue;		/* hme is busy */
		/*
		 * Try to avoid picking up the same hme which was stolen
		 * last time we serached in this group. If we don't find
		 * another one to steal then we come back to this.
		 */
		if (hme->hme_impl & HME_STOLEN) {
			hme->hme_impl &= ~HME_STOLEN;
			try_again = 1;
			continue;
		}
		if (((pp = hme->hme_page) != NULL) && ppcmmu_mlist_held(pp))
			continue;
		hme->hme_impl |= HME_BUSY;

		if (pp || (hat != &hats[hme->hme_hat])) {
			register struct hat *other_hat;

			/* acquire the necessary locks in the right order */
			mutex_exit(&ptegp->ptegp_mutex);
			mutex_exit(&hat->hat_mutex);
			if (pp) {
				ppcmmu_mlist_enter(pp);
				/*
				 * Check if the hme has changed while we were
				 * getting the mlist lock.
				 * (lock paranoia!?)
				 */
				if (hme->hme_valid && pp != hme->hme_page) {
					ppcmmu_mlist_exit(pp);
					mutex_enter(&hat->hat_mutex);
					mutex_enter(&ptegp->ptegp_mutex);
					continue; /* continue the search */
				}
			}
			if (hme->hme_valid) {
				other_hat = &hats[hme->hme_hat];
				mutex_enter(&other_hat->hat_mutex);
				mutex_enter(&ptegp->ptegp_mutex);
				/* check if hme is still valid */
				if (hme->hme_valid) {
					ppcmmu_pteunload(ptegp, p, pp, hme,
						hme->hme_nosync ?
						PPCMMU_NOSYNC : PPCMMU_RMSYNC);
				}
				/* get back the right hat mutex */
				if (hat != other_hat) {
					mutex_exit(&other_hat->hat_mutex);
					if (!mutex_tryenter(&hat->hat_mutex)) {
					    mutex_exit(&ptegp->ptegp_mutex);
					    mutex_enter(&hat->hat_mutex);
					    mutex_enter(&ptegp->ptegp_mutex);
					}
				}
			} else {
				mutex_enter(&hat->hat_mutex);
				mutex_enter(&ptegp->ptegp_mutex);
			}
			if (pp)
				ppcmmu_mlist_exit(pp);
			/*
			 * If someone grabbed our stolen PTE
			 * while the ptegp mutex is freed and
			 * reacquired then continue the search.
			 * (lock paranoia!?)
			 */
			if (hme->hme_valid)
				continue;
		} else {
			ppcmmu_pteunload(ptegp, p, pp, hme,
			    hme->hme_nosync ?
			    PPCMMU_NOSYNC : PPCMMU_RMSYNC);
		}
		hme->hme_impl = HME_STOLEN; /* Mark the hme as STOLEN */
		return (p);
	}

	if (try_again) {
		try_again = 0;
		goto Loop;
	}

	return (NULL);
}

/*
 * Find a free PTE slot in the PTEG. If we find a free solt in the group
 * we use it otherwise we look for any stale mapping in the group before
 * we say NO.
 */
static hwpte_t *
find_free_pte(struct hat *hat, register hwpte_t *pteg,
	register struct ptegp *ptegp, register page_t *pp)
{
	register u_int validmap;
	register u_int mask;
	register int i;
	int repeat = 1; /* repeat count */
	u_int mask_all;
	register hwpte_t *pte;

	ASSERT(&ptegp->ptegp_mutex);
	ASSERT(((u_int)pteg & PTEOFFSET) == 0);
	ASSERT(MUTEX_HELD(&hat->hat_mutex));
	ASSERT((pp == NULL) || ppcmmu_mlist_held(pp));

again:
	if (pteg >= mptes) {
		mask = 0x100;
		mask_all = 0xff00;
	} else {
		mask = 0x1;
		mask_all = 0xff;
	}
	pte = pteg;
	validmap = ptegp->ptegp_validmap;
	if ((validmap & mask_all) != mask_all) {
		/*
		 * We have at least one free slot.
		 */
		for (i = NPTEPERPTEG; i; i--, mask <<= 1, pte++) {
			if ((validmap & mask) == 0)
				return (pte);
		}
	}

	/*
	 * Look for any stale mappings in the group, if we find one then
	 * unload it (to sync rm bits) and try again.
	 */
	for (i = NPTEPERPTEG; i != 0; i--, pte++) {
		register struct hment *hme;
		register u_int vsid;
		register struct page *opp;
		register struct hat *ohat;

		vsid = vsid_from_hwpte(pte);
		if (IS_VSID_RANGE_INUSE(vsid >> VSIDRANGE_SHIFT))
			continue;	/* vsid is in use */

		/* stale mapping, unload it to sync rm bits */
		hme = hwpte_to_hme(pte);
		opp = hme->hme_page;
		ohat = &hats[hme->hme_hat];

		/*
		 * Obtain all the locks in the correct order.
		 */
		mutex_exit(&ptegp->ptegp_mutex);
		mutex_exit(&hat->hat_mutex); /* release our hat mutex */
		if (opp && (opp != pp)) {
			/*
			 * The pp for the stale mapping is different from
			 * our pp for which we are trying to make a mapping.
			 * We need to give up mlist lock on our pp to avoid
			 * possible deadlock in case we block here. XXX
			 */
			ppcmmu_mlist_exit(pp);
			ppcmmu_mlist_enter(opp);
		}
		mutex_enter(&ohat->hat_mutex);
		mutex_enter(&ptegp->ptegp_mutex);
		/*
		 * While we were getting the locks pte could
		 * have been unloaded/reloaded, so check it again
		 * before trying to unload it.
		 */
		if (hme->hme_valid && (vsid == vsid_from_hwpte(pte))) {
			ppcmmu_pteunload(ptegp, pte, opp, hme,
			    hme->hme_nosync ?
			    PPCMMU_NOSYNC : PPCMMU_RMSYNC);
		}
		/*
		 * Release locks for the old (stale) hat and
		 * reacquire the locks for our hat.
		 */
		mutex_exit(&ptegp->ptegp_mutex);
		mutex_exit(&ohat->hat_mutex);
		if (opp && (opp != pp)) {
			ppcmmu_mlist_exit(opp);
			ppcmmu_mlist_enter(pp);
		}
		mutex_enter(&hat->hat_mutex);
		mutex_enter(&ptegp->ptegp_mutex);
		goto again; /* try now */
	}
	if (repeat--)
		goto again;
	return (NULL);
}

/*
 * Given the address of a pte in the page table compute the virtual address
 * of the page for this mapping.
 */
caddr_t
hwpte_to_vaddr(register hwpte_t *pte)
{
	u_long api, vsid, vaddr, h1;

	api = api_from_hwpte(pte);
	vsid = vsid_from_hwpte(pte);
	if (hbit_from_hwpte(pte) == H_PRIMARY)
		h1 = ((u_long)pte & 0xFFC0) >> 6;
	else
		h1 = (((u_long)pte & 0xFFC0) >> 6) ^ hash_pteg_mask;
	vaddr = ((h1 ^ (vsid & 0x3FF)) << MMU_STD_PAGESHIFT) | (api << 22) |
		(vsid << 28);
	return ((caddr_t)vaddr);
}

/*
 * Unlock the pte: Decrement the lock count in the lockmap and/or
 * the hash table entry for this pte.
 */
void
pteunlock(struct ptegp *ptegp, hwpte_t *pte)
{
	int lkcnt_shift;
	int count;

	if (pte >= mptes)
		lkcnt_shift = LKCNT_UPPER_SHIFT;
	else
		lkcnt_shift = LKCNT_LOWER_SHIFT;
	lkcnt_shift += (((u_int)pte & PTEOFFSET) >> PTESHIFT) * LKCNT_NBITS;
	count = (ptegp->ptegp_lockmap >> lkcnt_shift) & LKCNT_MASK;
	if (count == LKCNT_HASHED) {
		count = change_ptelock(pte, -1);
		if (count == 0) {
			/* free the hash table entry */
			delete_ptelock(pte);
			ptegp->ptegp_lockmap &= ~(LKCNT_MASK << lkcnt_shift);
		}
	} else {
		/*
		 * XXXPPC: Currently we don't enable this ASSERT because
		 * this function gets called from the console initialization
		 * stuff from startup() before hat_kern_setup() is done.
		 * Jordan, what are the console requirements here?
		 *
		 * ASSERT(count != 0);
		 */
		ptegp->ptegp_lockmap &= ~(LKCNT_MASK << lkcnt_shift);
		if (--count > 0)
			ptegp->ptegp_lockmap |= count << lkcnt_shift;
	}
}

/*
 * Lock the pte: Increment the lock count in the lockmap
 * and/or update the hash table entry.
 */
void
ptelock(struct ptegp *ptegp, hwpte_t *pte)
{
	int lkcnt_shift;
	int count;

	if (pte >= mptes)
		lkcnt_shift = LKCNT_UPPER_SHIFT;
	else
		lkcnt_shift = LKCNT_LOWER_SHIFT;
	lkcnt_shift += (((u_int)pte & PTEOFFSET) >> PTESHIFT) * LKCNT_NBITS;
	count = (ptegp->ptegp_lockmap >> lkcnt_shift) & LKCNT_MASK;
	if (count == LKCNT_HASHED)
		(void) change_ptelock(pte, +1);
	else {
		count += 1;
		ptegp->ptegp_lockmap &= ~(LKCNT_MASK << lkcnt_shift);
		ptegp->ptegp_lockmap |= count << lkcnt_shift;
		if (count == LKCNT_HASHED)
			(void) create_ptelock(pte, count);
	}
}

/*
 * ptelock_hash_ops(pte, count, flags)
 * where
 *	pte:	Pointer to the hw pte.
 *	count:	New lockcnt value for this pte.
 *	flags:	HASH_CREATE	Allocate an entry for this pte.
 *		HASH_CHANGE	Change lockcnt value (+count) for this pte.
 *		HASH_DELETE	Delete the entry for this pte.
 */
static int
ptelock_hash_ops(hwpte_t *pte, int count, int flags)
{
	register int hashval, i;
	u_int target = 0;

	/* compute the hash value for this pte */
	i = hashval = ((u_int)pte >> PTESHIFT) & ptelock_hash_mask;

	if ((flags & HASH_CREATE) == 0)
		target = (u_int)pte;

	lock_set(&ptelock_hash_lock);
	/*
	 * Start the search forward beginning with the hashed entry.
	 */
	while (i < ptelock_hashtab_size) {
		if (ptelock_hashtab[i].tag == target)
			goto found;
		++i;
	}
	/* failed, try the search backward */
	i = hashval - 1;
	while (i >= 0) {
		if (ptelock_hashtab[i].tag == target)
			goto found;
		--i;
	}
	lock_clear(&ptelock_hash_lock);

	if (flags & HASH_CREATE) {
		/* no free entry found in the hash table - panic!! */
		cmn_err(CE_PANIC,
			"Out of hash entries in ptelock_hashtab[]...\n");
	} else {
		/* could not find the entry for this pte */
		cmn_err(CE_PANIC, "No hash entry found for the pte %x\n", pte);
	}
	/*NOTREACHED*/
found:
	switch (flags) {
	case HASH_DELETE:
		/* free the hash entry */
		ptelock_hashtab[i].tag = 0;
		ptelock_hashtab[i].lockcnt = 0;
		break;
	case HASH_CHANGE:
		/* change the lockcnt */
		ptelock_hashtab[i].lockcnt += count;
		break;
	case HASH_CREATE:
		/* initialize the entry */
		ptelock_hashtab[i].tag = (u_int)pte;
		ptelock_hashtab[i].lockcnt = count;
		vmhatstat.vh_hash_ptelock.value.ul++;
		break;
	}
	lock_clear(&ptelock_hash_lock);
	return (ptelock_hashtab[i].lockcnt);
}

/*
 * Kernel thread to unload stale mappings from the page table and recreate
 * the vsid free list.
 *
 * Algorithm:
 *	Get ppcmmu_res_lock mutex.
 *	Loop for ever:
 *		Loop for each PTEGpair:
 *			Get PTEGP mutex lock.
 *			Scan the PTEs and for each stale mapping get mlist
 *			lock (if it has page structure) and unload the
 *			mapping.
 *			Release mlist lock if held.
 *			Release PTEGP mutex.
 *		Reconstruct free vsid list from the vsid_bitmap array.
 *		cv_wait(&ppcmmu_cv, &ppcmmu_res_lock).
 *
 */
void
ppcmmu_vsid_gc(void)
{
	register struct ptegp *ptegp;
	register hwpte_t *pte;
	register struct hment *hme;
	register struct page *pp;
	register int i;
	register struct hat *hat;

	mutex_enter(&ppcmmu_res_lock);

	for (;;) {
		pte = ptes;
		while (pte < eptes) {
			ptegp = hwpte_to_ptegp(pte);
			mutex_enter(&ptegp->ptegp_mutex);
			for (i = 0; i < NPTEPERPTEG; i++, pte++) {
				register u_int vsid;

				if (!hwpte_valid(pte))
					continue;
				vsid = vsid_from_hwpte(pte);
				if (IS_VSID_RANGE_INUSE(vsid >>
							VSIDRANGE_SHIFT))
					continue;
				/* stale mapping, unload it */
				hme = hwpte_to_hme(pte);
				hat = &hats[hme->hme_hat];
				pp = hme->hme_page;
				/*
				 * Acquire the locks in the correct order.
				 */
				mutex_exit(&ptegp->ptegp_mutex);
				if (pp)
					ppcmmu_mlist_enter(pp);
				mutex_enter(&hat->hat_mutex);
				mutex_enter(&ptegp->ptegp_mutex);
				/*
				 * make sure the pte hasn't been modified
				 * while we were getting the locks.
				 */
				if (hme->hme_valid &&
				    (vsid == vsid_from_hwpte(pte))) {
					ppcmmu_pteunload(ptegp, pte, pp, hme,
					    hme->hme_nosync ?
					    PPCMMU_NOSYNC : PPCMMU_RMSYNC);
				}
				mutex_exit(&hat->hat_mutex);
				if (pp)
					ppcmmu_mlist_exit(pp);
			}
			mutex_exit(&ptegp->ptegp_mutex);
		}

		/* Re construct the free vsid sets */
		build_vsid_freelist();

		/*
		 * wakeup anyone waiting for a free vsid.
		 */
		if (ppcmmu_vsid_wanted) {
			ppcmmu_vsid_wanted = 0;
			cv_broadcast(&ppcmmu_vsid_cv);
		}
		/* Wait until the wakeup call from ppcmmu_alloc() */
		cv_wait(&ppcmmu_cv, &ppcmmu_res_lock);
	}
}

/*
 * Rebuild the free vsid-range-set list from the vsid_bitmap.
 * If memory allocation fails in allocating vsid_alloc set structures then
 * it terminates building the rest of the list. It is assumed that this
 * routine is woken up only when the vsid_range free list is empty.
 */
static void
build_vsid_freelist(void)
{
	register u_int cword;
	register u_int *bp;
	register int n;
	register int bitsleft;
	register int free_ranges = 0;
	register u_int next_free_vsid;
	int nzeros;
	struct vsid_alloc_set *head = NULL;
	struct vsid_alloc_set *p;
	extern int cntlzw(); /* 'cntlzw' instruction */

	n = ppcmmu_max_vsidranges/NBINT;
	bp = vsid_bitmap + (n - 1); /* start from the last word in the bitmap */

	next_free_vsid = ppcmmu_max_vsidranges;
	for (cword = *bp; n; n--, cword = *--bp) {
		if (cword == 0) {
			free_ranges += NBINT;
			continue;
		}

		nzeros = cntlzw(cword); /* number of leading zeros */
		free_ranges += nzeros;
		bitsleft = NBINT;
		do {
			/*
			 * allocate a set and attach it at the head of the
			 * list
			 */
			if (free_ranges) {
				p = kmem_zalloc(sizeof (struct vsid_alloc_set),
					KM_NOSLEEP);
				if (!p) {
					cmn_err(CE_WARN,
						"No memory for vsid_alloc_set");
					break;
				}
				p->vs_nvsid_ranges = free_ranges;
				next_free_vsid -= free_ranges;
				p->vs_vsid_range_id = next_free_vsid;
				free_ranges = 0;
				if (head)
					p->vs_next = head;
				else
					p->vs_next = NULL;
				head = p;
			}
			next_free_vsid--;
			bitsleft -= (nzeros + 1);
			cword <<= nzeros + 1;
			nzeros = cntlzw(cword); /* number of leading zeros */
			if (nzeros > bitsleft) {
				free_ranges += bitsleft;
				break;
			}
			free_ranges += nzeros;
		} while (bitsleft > 0);
	}

	/*
	 * If the loop terminates normally then 'free_ranges' would be 0 because
	 * the vsid_range_id '0' (used by kernel) is always set (i.e in use).
	 */
	ASSERT((p == NULL) || (free_ranges == NULL));

	ASSERT(vsid_alloc_head == NULL); /* old list should be empty */

	/* setup the head pointer to the newly built list */
	vsid_alloc_head = head;
#ifdef HAT_DEBUG
	/*
	 * Match the vsid_bitmap with the vsid_freelist generated.
	 */
	for (p = vsid_alloc_head; p; p = p->vs_next) {
		int j, cnt;

		j = p->vs_vsid_range_id;
		for (cnt = p->vs_nvsid_ranges; cnt; cnt--, j++) {
			if (IS_VSID_RANGE_INUSE(j)) {
				prom_printf("*** ERROR FOR VSID RANGE %x ***\n",
					j);
				debug_enter(NULL);
				cmn_err(CE_PANIC,
				    "build_vsid_freelist: INTERNAL ERROR!!\n");
			}
		}
	}
#endif
}

/*
 * Flush the cache for the specified page in the specified context.
 *
 * Flushing the cache for a page requires the context (atleast the corresponding
 * segment register should be for this context) to be the active context. This
 * generally happens thru /proc where a debugger has changed the contents
 * of the page in the program's address space but the active context is of
 * the debugger's. Generally only the debuggers write to the proc's address
 * space so the solution implemented here is the simplest but not efficient.
 */
static void
ppcmmu_cache_flushpage(struct hat *hat, caddr_t addr)
{
	extern u_int mfsrin(caddr_t);
	extern void mtsrin(u_int, caddr_t);
	register u_int old_sr;

	/*
	 * If this is a kernel page or we are in the right context then
	 * simply do the flush.
	 */
	if ((addr >= (caddr_t)KERNELBASE) ||
		(((mfsrin(0) & SR_VSID) >> 4) == hat->hat_data[0])) {
		mmu_cache_flushpage(addr);
		return;
	}

	/*
	 * Flushing for a different context.
	 */
	/* save the old segment register value */
	old_sr = mfsrin(addr);
	kpreempt_disable();
	/* load the segment register for the other context */
	mtsrin(VSID(hat->hat_data[0], addr) | SR_KU, addr);
	/* do the flush */
	mmu_cache_flushpage(addr);
	/* restore the segment register */
	mtsrin(old_sr, addr);
	kpreempt_enable();
}

/*
 * This function is currently not supported on this platform. For what
 * it's supposed to do, see hat.c and hat_srmmu.c
 */
/*ARGSUSED*/
faultcode_t
hat_softlock(hat, addr, lenp, ppp, flags)
	struct hat *hat;
	caddr_t	addr;
	size_t	*lenp;
	page_t	**ppp;
	u_int	flags;
{
	return (FC_NOSUPPORT);
}

/*
 * This function is currently not supported on this platform. For what
 * it's supposed to do, see hat.c and hat_srmmu.c
 */
/*ARGSUSED*/
faultcode_t
hat_pageflip(hat, addr_to, kaddr, lenp, pp_to, pp_from)
	struct hat *hat;
	caddr_t addr_to, kaddr;
	size_t	*lenp;
	page_t	**pp_to, **pp_from;
{
	return (FC_NOSUPPORT);
}

#ifdef HAT_DEBUG
/*
 * Dump the information on the mappings in the address space. And return the
 * number of mappings found.
 */
int
ppcmmu_dumpas(as)
	register struct as *as;
{
	register struct ptegp *ptegp;
	register hwpte_t *pte;
	register struct hment *hme;
	register struct page *pp;
	register int i;
	register u_int range;
	register int mappings = 0;
	register u_int v;

	v = as->a_hat->hat_data[0];

	kpreempt_disable();
	prom_printf("MAPPINGS IN ADDRESS SPACE 0x%x:\n", as);
	pte = ptes;
	while (pte < eptes) {
		ptegp = hwpte_to_ptegp(pte);
		for (i = 0; i < NPTEPERPTEG; i++, pte++) {
			if (!hwpte_valid(pte))
				continue;
			range = vsid_from_hwpte(pte) >> VSIDRANGE_SHIFT;
			if (range != v)
				continue;
			hme = hwpte_to_hme(pte);
			pp = hme->hme_page;
			prom_printf("\taddr %x pte %x (%x %x) hme %x pp %x\n",
				hwpte_to_vaddr(pte), pte, ((u_int *)pte)[0],
				((u_int *)pte)[1], hme, pp);
			mappings++;
		}
	}
	prom_printf("Total of %d mappings found.\n", mappings);
	kpreempt_enable();
	return (mappings);
}
#endif /* HAT_DEBUG */

/*
 * return supported features
 */
/*ARGSUSED1*/
int
hat_supported(enum hat_features feature, void *arg)
{
	switch (feature) {
	case    HAT_SHARED_PT:
		return (0);	/* Does not support ISM */
	default:
		return (0);
	}
}

int
hat_stats_enable(struct hat *hat)
{
	mutex_enter(&hat->hat_mutex);
	hat->s_rmstat++;
	mutex_exit(&hat->hat_mutex);
	return (1);
}

void
hat_stats_disable(struct hat *hat)
{
	mutex_enter(&hat->hat_mutex);
	hat->s_rmstat--;
	mutex_exit(&hat->hat_mutex);
}
