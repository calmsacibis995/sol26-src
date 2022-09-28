/*
 *
 * Copyright (c) 1989, 1990, 1991 by Sun Microsystems, Inc.
 */

#pragma ident   "@(#)hat_sfmmu.c 1.139     96/10/22 SMI"

/*
 * VM - Hardware Address Translation management for Spitfire MMU.
 *
 * This file implements the machine specific hardware translation
 * needed by the VM system.  The machine independent interface is
 * described in <vm/hat.h> while the machine dependent interface
 * and data structures are described in <vm/hat_sfmmu.h>.
 *
 * The hat layer manages the address translation hardware as a cache
 * driven by calls from the higher levels in the VM system.
 */

#include <sys/types.h>
#include <vm/hat.h>
#include <vm/hat_sfmmu.h>
#include <vm/page.h>
#include <vm/mach_page.h>
#include <sys/pte.h>
#include <sys/systm.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <sys/machparam.h>
#include <sys/vtrace.h>
#include <sys/kmem.h>
#include <sys/mmu.h>
#include <sys/cmn_err.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/debug.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_kp.h>
#include <vm/rm.h>
#include <sys/t_lock.h>
#include <sys/msgbuf.h>
#include <sys/obpdefs.h>
#include <sys/vm_machparam.h>
#include <sys/var.h>
#include <sys/trap.h>
#include <sys/machtrap.h>
#include <sys/scb.h>
#include <sys/bitmap.h>
#include <sys/machlock.h>
#include <sys/membar.h>
#include <sys/atomic_prim.h>
#include <sys/cpu_module.h>
#include <sys/prom_debug.h>

/*
 * SFMMU specific hat functions
 */
void	hat_pagecachectl(struct page *, int);
void	hat_page_badecc(u_long);

/* flags for hat_pagecachectl */
#define	HAT_CACHE	0x0
#define	HAT_UNCACHE	0x1
#define	HAT_TMPNC	0x2

/*
 * Flag to disable large page support.
 */
int	disable_large_pages = 0;

/*
 * Private sfmmu data structures for hat management
 */
static struct kmem_cache *sfmmuid_cache;

/*
 * Private sfmmu data structures for ctx management
 */
static struct ctx	*ctxhand;	/* hand used while stealing ctxs */
static struct ctx	*ctxfree;	/* head of free ctx list */
static struct ctx	*ctxdirty;	/* head of dirty ctx list */

/*
 * sfmmu static variables for hmeblk resource management.
 */
static struct kmem_cache *sfmmu8_cache;
static struct kmem_cache *sfmmu1_cache;

static struct hme_blk 	*hblk1_flist;	/* freelist for 1 hment hme_blks */
static struct hme_blk 	*hblk8_flist;	/* freelist for 8 hment hme_blks */
static struct hme_blk 	*hblk1_flist_t;	/* tail of hblk1 freelist */
static struct hme_blk 	*hblk8_flist_t;	/* tail of hblk8 freelist */
static int 		hblk1_avail;	/* number of free 1 hme hme_blks */
static int 		hblk8_avail;	/* number of free 8 hme hme_blks */
static kmutex_t 	hblk1_lock;	/* mutex for hblk1 freelist */
static kmutex_t 	hblk8_lock;	/* mutex for hblk8 freelist */
static kmutex_t 	ctx_lock;	/* mutex for ctx structures */
static kmutex_t 	ism_lock;	/* mutex for ism map blk cache */

static int 	hblk8_allocated, hblk1_allocated;
static int	hblk8_prealloc_count;
static u_int	hblkalloc_inprog;

#ifdef DEBUG
static int 	hblk8_inuse, hblk1_inuse;
static int	nhblk8_allocated, nhblk1_allocated;
#define		HBLK_DEBUG_COUNTER_INCR(counter, value)	counter += value
#define		HBLK_DEBUG_COUNTER_DECR(counter, value)	counter -= value
#else
#define		HBLK_DEBUG_COUNTER_INCR(counter, value)
#define		HBLK_DEBUG_COUNTER_DECR(counter, value)
#endif /* DEBUG */

/*
 * private data for ism
 */
static struct kmem_cache *ism_map_blk_cache;
#define	ISMID_STARTADDR	NULL

/*
 * Private sfmmu routines (prototypes)
 */
static struct hme_blk *sfmmu_shadow_hcreate(sfmmu_t *, caddr_t, int);
static struct 	hme_blk *sfmmu_hblk_alloc(sfmmu_t *, caddr_t,
			struct hmehash_bucket *, int, hmeblk_tag);
static struct sf_hment *
		sfmmu_hblktohme(struct hme_blk *, caddr_t, int *);
static caddr_t	sfmmu_hblk_unload(struct hat *, struct hme_blk *, caddr_t,
			caddr_t, u_int);
static caddr_t	sfmmu_hblk_sync(struct hat *, struct hme_blk *, caddr_t,
			caddr_t, int);
static void	sfmmu_hblk_free(struct hmehash_bucket *, struct hme_blk *,
			u_longlong_t);
static struct hme_blk *sfmmu_hblk_grow(int, int);
static struct hme_blk *sfmmu_hblk_steal(int);
static int	sfmmu_steal_this_hblk(struct hmehash_bucket *,
			struct hme_blk *, u_longlong_t, u_longlong_t,
			struct hme_blk *);

static struct hme_blk *
		sfmmu_hmetohblk(struct sf_hment *);

static void	sfmmu_memload_batchsmall(struct hat *, caddr_t, machpage_t **,
		    u_int, u_int, u_int);
void		sfmmu_tteload(struct hat *, tte_t *, caddr_t, machpage_t *,
			u_int);
static int	sfmmu_tteload_array(sfmmu_t *, tte_t *, caddr_t, machpage_t **,
			u_int);
static struct hmehash_bucket *sfmmu_tteload_acquire_hashbucket(sfmmu_t *,
					caddr_t, int);
static struct hme_blk *sfmmu_tteload_find_hmeblk(sfmmu_t *,
				struct hmehash_bucket *, caddr_t, int);
static int	sfmmu_tteload_addentry(sfmmu_t *, struct hme_blk *, tte_t *,
			caddr_t, machpage_t **, u_int);
static void	sfmmu_tteload_release_hashbucket(struct hmehash_bucket *);

static int	sfmmu_pagearray_setup(caddr_t, machpage_t **, tte_t *, int);
u_int		sfmmu_user_vatopfn(caddr_t, sfmmu_t *);
void		sfmmu_memtte(tte_t *, u_int, u_int, int);
static void	sfmmu_vac_conflict(struct hat *, caddr_t, machpage_t *);
static int	sfmmu_vacconflict_array(caddr_t, machpage_t *, int *);

static struct ctx *
		sfmmu_get_ctx(sfmmu_t *);
static void	sfmmu_free_ctx(sfmmu_t *, struct ctx *);
static void	sfmmu_free_sfmmu(sfmmu_t *);
static tte_t	sfmmu_gettte(struct hat *, caddr_t);
static caddr_t	sfmmu_hblk_unlock(struct hme_blk *, caddr_t, caddr_t);
static void	sfmmu_chgattr(struct hat *, caddr_t, size_t, u_int, int);
static cpuset_t	sfmmu_pageunload(machpage_t *, struct sf_hment *);
static cpuset_t	sfmmu_pagesync(machpage_t *, struct sf_hment *, u_int);
static void	sfmmu_ttesync(struct hat *, caddr_t, tte_t *, machpage_t *);
static void	sfmmu_page_cache(machpage_t *, int, int);
static void	sfmmu_tlbcache_demap(caddr_t, sfmmu_t *, struct hme_blk *,
			int, int, int);
static void	sfmmu_cache_flush(int, int);
static void	sfmmu_cache_flushcolor(int);
static void	sfmmu_tlb_demap(caddr_t, sfmmu_t *, struct hme_blk *, int);
static void	sfmmu_tlb_ctx_demap(sfmmu_t *);
static caddr_t	sfmmu_hblk_chgattr(struct hme_blk *, caddr_t, caddr_t, u_int,
			int);

static u_longlong_t	sfmmu_vtop_attr(u_int, int mode, tte_t *);
static u_int	sfmmu_ptov_attr(tte_t *);
static caddr_t	sfmmu_hblk_chgprot(struct hme_blk *, caddr_t, caddr_t, u_int);
static u_int	sfmmu_vtop_prot(u_int, u_int *);
int		sfmmu_add_nucleus_hblks(caddr_t, int);
static int	sfmmu_idcache_constructor(void *, void *, int);
static void	sfmmu_idcache_destructor(void *, void *);
static int	sfmmu_hblkcache_constructor(void *, void *, int);
static void	sfmmu_hblkcache_reclaim(void *);
static void	sfmmu_tte_unshare(struct hat *, struct hat *, caddr_t, size_t);
static int	ism_cache_constructor(void *, void *, int);
static ism_map_blk_t *ism_map_blk_alloc(void);
static void	sfmmu_hblk_tofreelist(struct hme_blk *, u_longlong_t);
static void	sfmmu_reuse_ctx(struct ctx *, sfmmu_t *);
static void	sfmmu_shadow_hcleanup(sfmmu_t *, struct hme_blk *,
			struct hmehash_bucket *);
static void	sfmmu_free_hblks(sfmmu_t *, caddr_t, caddr_t, int);

static void	hme_add(struct sf_hment *, machpage_t *);
static void	hme_sub(struct sf_hment *, machpage_t *);
static void	sfmmu_rm_large_mappings(machpage_t *, int);

static void	hat_lock_init(void);
static void	hat_kstat_init(void);
static int	sfmmu_kstat_percpu_update(kstat_t *ksp, int rw);

#ifdef DEBUG
static void	sfmmu_check_hblk_flist();
#endif

/*
 * Semi-private sfmmu data structures.  Some of them are initialize in
 * startup or in hat_init. Some of them are private but accessed by
 * assembly code or mach_sfmmu.c
 */
struct hmehash_bucket *uhme_hash;	/* user hmeblk hash table */
struct hmehash_bucket *khme_hash;	/* kernel hmeblk hash table */
int 		uhmehash_num;		/* # of buckets in user hash table */
int 		khmehash_num;		/* # of buckets in kernel hash table */
struct ctx	*ctxs, *ectxs;		/* used by <machine/mmu.c> */
u_int		nctxs = 0;		/* total number of contexts */

int		cache = 0;		/* describes system cache */

caddr_t		tsballoc_base;		/* base of bopalloced tsbs */
struct tsb_info tsb_bases[NCPU];
int tsb_szcode;				/* TSB size code 0-7 */

/*
 * kstat data
 */
struct sfmmu_global_stat sfmmu_global_stat;

/*
 * Global data
 */
extern cpuset_t cpu_ready_set;
extern u_int vac_mask;
extern uint shm_alignment;
extern int do_virtual_coloring;
extern struct as kas;			/* kernel's address space */
extern int pf_is_memory(uint);
extern void trap(struct regs *, u_int, u_int, caddr_t);

sfmmu_t *ksfmmup;			/* kernel's hat id */
struct ctx *kctx;			/* kernel's context */

#ifdef DEBUG
static void		chk_tte(tte_t *, tte_t *, tte_t *, struct hme_blk *);
#endif

/* sfmmu locking operations */
static void	sfmmu_page_enter(machpage_t *);
static void	sfmmu_page_exit(machpage_t *);
static int	sfmmu_mlist_held(machpage_t *);

/* sfmmu internal locking operations - accessed directly */
static kmutex_t	*sfmmu_mlist_enter(machpage_t *);
static void	sfmmu_mlist_exit(kmutex_t *);

/* array of mutexes protecting a page's mapping list and p_nrm field */
#define	MLIST_SIZE		(0x40)
#define	MLIST_HASH(pp)		&mml_table[(((u_int)(pp))>>6) & (MLIST_SIZE-1)]
kmutex_t			mml_table[MLIST_SIZE];

#define	SPL_TABLE_SIZE	64
#define	SPL_SHIFT	6
#define	SPL_HASH(pp)	\
	&sfmmu_page_lock[(((u_int)pp) >> SPL_SHIFT) & (SPL_TABLE_SIZE - 1)]

static	kmutex_t	sfmmu_page_lock[SPL_TABLE_SIZE];

/*
 * bit mask for managing vac conflicts on large pages.
 * bit 1 is for uncache flag.
 * bits 2 through min(num of cache colors + 1,31) are
 * for cache colors that have already been flushed.
 */
#define	CACHE_UNCACHE		1
#define	CACHE_NUM_COLOR		(shm_alignment >> MMU_PAGESHIFT)

#define	CACHE_VCOLOR_MASK(vcolor)	(2 << (vcolor & (CACHE_NUM_COLOR - 1)))

#define	CacheColor_IsFlushed(flag, vcolor) \
					((flag) & CACHE_VCOLOR_MASK(vcolor))

#define	CacheColor_SetFlushed(flag, vcolor) \
					((flag) |= CACHE_VCOLOR_MASK(vcolor))
/*
 * Flags passed to sfmmu_page_cache to flush page from vac or not.
 */
#define	CACHE_FLUSH	0
#define	CACHE_NO_FLUSH	1

/*
 * Flags passed to sfmmu_tlbcache_demap
 */
#define	FLUSH_NECESSARY_CPUS	0
#define	FLUSH_ALL_CPUS		1

/*
 * Move to some cache specific header file - XXX
*/
#define	VAC_ALIGNED(a1, a2) ((((u_int)(a1) ^ (u_int)(a2)) & vac_mask) == 0)

extern int ecache_linesize;

#define	IsAligned(val, sz)	((((u_int)(val)) & ((sz) - 1)) == 0)

#ifdef DEBUG

struct ctx_trace stolen_ctxs[TRSIZE];
struct ctx_trace *ctx_trace_first = &stolen_ctxs[0];
struct ctx_trace *ctx_trace_last = &stolen_ctxs[TRSIZE-1];
struct ctx_trace *ctx_trace_ptr = &stolen_ctxs[0];
u_int	num_ctx_stolen = 0;

int	ism_debug = 0;

#endif /* DEBUG */


tte_t	hw_tte;

/*
 * Initialize the hardware address translation structures.
 * Called by hat_init() after the vm structures have been allocated
 * and mapped in.
 */
void
hat_init()
{
	register struct ctx	*ctx;
	register struct ctx	*cur_ctx = NULL;
	int 			i, j, one_pass_over;

	hat_statinit();
	hat_lock_init();
	hat_kstat_init();

	/*
	 * HW bits only in a TTE
	 */
	hw_tte.tte_bit.v = 1;
	hw_tte.tte_bit.sz = 3;
	hw_tte.tte_bit.nfo = 1;
	hw_tte.tte_bit.ie = 1;
	hw_tte.tte_bit.pahi = 0x3FF;
	hw_tte.tte_bit.palo = 0x7FFFF;
	hw_tte.tte_bit.l = 1;
	hw_tte.tte_bit.cp = 1;
	hw_tte.tte_bit.cv = 1;
	hw_tte.tte_bit.e = 1;
	hw_tte.tte_bit.p = 1;
	hw_tte.tte_bit.w = 1;
	hw_tte.tte_bit.g = 1;
	/* Initialize the hash locks */
	for (i = 0; i < khmehash_num; i++) {
		mutex_init(&khme_hash[i].hmehash_mutex, "khmehash_lock",
			MUTEX_DEFAULT, NULL);
	}
	for (i = 0; i < uhmehash_num; i++) {
		mutex_init(&uhme_hash[i].hmehash_mutex, "uhmehash_lock",
			MUTEX_DEFAULT, NULL);
	}
	khmehash_num--;		/* make sure counter starts from 0 */
	uhmehash_num--;		/* make sure counter starts from 0 */

	/*
	 * Initialize ctx structures
	 * We keep a free list of ctxs. That will be used to get/free ctxs.
	 * The first NUM_LOCKED_CTXS (0, .. NUM_LOCKED_CTXS-1)
	 * contexts are always not available. The rest of the contexts
	 * are put in a free list in the following fashion:
	 * Adjacent ctxs are not chained together - every (CTX_GAP)th one
	 * is chained next to each other. This results in a better hashing
	 * on ctxs at the begining. Later on the free list becomes random
	 * as processes exit randomly.
	 */
	kctx = &ctxs[KCONTEXT];
	ctx = &ctxs[NUM_LOCKED_CTXS];
	ctxhand = ctxfree = ctx;		/* head of free list */
	one_pass_over = 0;
	for (j = 0; j < CTX_GAP; j++) {
		for (i = NUM_LOCKED_CTXS + j; i < nctxs; i = i + CTX_GAP) {
			if (one_pass_over) {
				cur_ctx->c_free = &ctxs[i];
				cur_ctx->c_refcnt = 0;
				one_pass_over = 0;
			}
			cur_ctx = &ctxs[i];
			if ((i + CTX_GAP) < nctxs) {
				cur_ctx->c_free = &ctxs[i + CTX_GAP];
				cur_ctx->c_refcnt = 0;
			}
		}
		one_pass_over = 1;
	}
	cur_ctx->c_free = NULL;		/* tail of free list */

	sfmmuid_cache = kmem_cache_create("sfmmuid_cache", sizeof (sfmmu_t),
		0, sfmmu_idcache_constructor, sfmmu_idcache_destructor,
		NULL, NULL, NULL, 0);

	sfmmu8_cache = kmem_cache_create("sfmmu8_cache", HME8BLK_SZ,
		HMEBLK_ALIGN, sfmmu_hblkcache_constructor, NULL,
		sfmmu_hblkcache_reclaim, (void *)HME8BLK_SZ, NULL, 0);

	sfmmu1_cache = kmem_cache_create("sfmmu1_cache", HME1BLK_SZ,
		HMEBLK_ALIGN, sfmmu_hblkcache_constructor, NULL,
		NULL, (void *)HME1BLK_SZ, NULL, 0);

	/*
	 * We grab the first hat for the kernel,
	 */
	AS_LOCK_ENTER(&kas, &kas.a_lock, RW_WRITER);
	kas.a_hat = hat_alloc(&kas);
	AS_LOCK_EXIT(&kas, &kas.a_lock);
}

/*
 * Initialize locking for the hat layer, called early during boot.
 */
static void
hat_lock_init()
{
	int i;
	char name_buf[100];

	mutex_init(&ctx_lock, "sfmmu_ctx_lock", MUTEX_DEFAULT, NULL);

	/* initialize locks for hme_blk freelists */
	mutex_init(&hblk8_lock, "sfmmu_hblk8_lock", MUTEX_DEFAULT, NULL);
	mutex_init(&hblk1_lock, "sfmmu_hblk1_lock", MUTEX_DEFAULT, NULL);

	/* initialize lock ism */
	mutex_init(&ism_lock, "sfmmu_ism_lock", MUTEX_DEFAULT, NULL);

	/*
	 * initialize the array of mutexes protecting a page's mapping
	 * list and p_nrm field.
	 */
	for (i = 0; i < MLIST_SIZE; i++) {
		(void) sprintf(name_buf, "mlist_%d", i);
		mutex_init(&mml_table[i], name_buf, MUTEX_DEFAULT, NULL);
	}

	for (i = 0; i < SPL_TABLE_SIZE; i++) {
		(void) sprintf(name_buf, "sfpage_%d", i);
		mutex_init(&sfmmu_page_lock[i], name_buf, MUTEX_DEFAULT, NULL);
	}
}

/*
 * Allocate initial hmeblks. would be nice to do this as part of
 * hat_init(), but bootops were still being used for allocation then.
 */
void
sfmmu_hblk_init()
{
	struct hme_blk *first, *hmeblkp;
	int i, nhblks;

	/*
	 * record the # of hmeblks used so far in sfmmu stat.
	 * we can tune the nucleus hmeblk allocation based on this value.
	 */
	SFMMU_STAT_SET(sf_hblk8_startup_use, hblk8_allocated - hblk8_avail);

	/*
	 * Make sure we allocate enough hme_blks to map the entire kernel,
	 * or the entire physical memory, which ever is lower. Also add
	 * some margin to be safe.
	 */
	hblk8_prealloc_count = MIN(physmem, mmu_btop((u_int)SYSEND -
		(u_int)KERNELBASE)) / (HMEBLK_SPAN(TTE8K) >> MMU_PAGESHIFT);
	hblk8_prealloc_count += (hblk8_prealloc_count >> 1);

	/*
	 * we have allocated (nucleus) hmeblks earlier, subtract them.
	 */
	nhblks = hblk8_prealloc_count - hblk8_allocated;

	while (nhblks > 0) {
		first = kmem_cache_alloc(sfmmu8_cache, KM_SLEEP);
		SFMMU_STAT(sf_hblk8_dalloc);
		for (i = 1, hmeblkp = first; i < nhblks && hmeblkp; i++) {
			hmeblkp->hblk_next = kmem_cache_alloc(
				sfmmu8_cache, KM_SLEEP);
			SFMMU_STAT(sf_hblk8_dalloc);
			hmeblkp = hmeblkp->hblk_next;

			if (hblk8_avail <= HME8_TRHOLD)
				break;	/* add allocated hblks in freelist */
		}

		if (hmeblkp == NULL) {
			cmn_err(CE_PANIC, "Can not allocate initial hmeblks\n");
		}
		hmeblkp->hblk_next = NULL;

		HBLK8_FLIST_LOCK();
		if (hblk8_avail == 0) {
			hblk8_flist = first;
		} else {
			hblk8_flist_t->hblk_next = first;
		}
		hblk8_flist_t = hmeblkp;
		hblk8_avail += i;
		hblk8_allocated += i;
		HBLK8_FLIST_UNLOCK();
		nhblks -= i;
	}
}

/*
 * Allocate a hat structure.
 * Called when an address space first uses a hat.
 */
struct hat *
hat_alloc(struct as *as)
{
	sfmmu_t *sfmmup;
	struct ctx *ctx;
	extern u_int get_color_start(struct as *);

	ASSERT(AS_WRITE_HELD(as, &as->a_lock));
	sfmmup = kmem_cache_alloc(sfmmuid_cache, KM_SLEEP);
	sfmmup->sfmmu_as = as;

	if (as == &kas) {			/* XXX - 1 time only */
		ctx = kctx;
		ksfmmup = sfmmup;
		sfmmup->sfmmu_cnum = ctxtoctxnum(ctx);
		ctx->c_sfmmu = sfmmup;
		sfmmup->sfmmu_clrstart = 0;
	} else {

		/*
		 * Just set to invalid ctx. When it faults, it will
		 * get a valid ctx. This would avoid the situation
		 * where we get a ctx, but it gets stolen and then
		 * we fault when we try to run and so have to get
		 * another ctx.
		 */
		sfmmup->sfmmu_cnum = INVALID_CONTEXT;
		/* initialize original physical page coloring bin */
		sfmmup->sfmmu_clrstart = get_color_start(as);
	}
	sfmmup->sfmmu_rss = 0;
	sfmmup->sfmmu_lttecnt = 0;
	sfmmup->sfmmu_ismblk = NULL;
	CPUSET_ZERO(sfmmup->sfmmu_cpusran);
	sfmmup->sfmmu_free = 0;
	sfmmup->sfmmu_rmstat = 0;
	sfmmup->sfmmu_clrbin = sfmmup->sfmmu_clrstart;
	return (sfmmup);
}

/*
 * Hat_setup, makes an address space context the current active one.
 * In sfmmu this translates to setting the secondary context with the
 * corresponding context.
 */
void
hat_setup(struct hat *sfmmup, int allocflag)
{
	struct ctx *ctx;
	u_int ctx_num;

#ifdef lint
	allocflag = allocflag;			/* allocflag is not used */
#endif /* lint */

	/*
	 * Make sure that we have a valid ctx and it doesn't get stolen
	 * after this point.
	 */
	if (sfmmup != ksfmmup)
		sfmmu_disallow_ctx_steal(sfmmup);

	ctx = sfmmutoctx(sfmmup);
	kpreempt_disable();
	CPUSET_ADD(sfmmup->sfmmu_cpusran, CPU->cpu_id);
	kpreempt_enable();
	ctx_num = ctxtoctxnum(ctx);
	ASSERT(sfmmup == ctx->c_sfmmu);

	/* curiosity check - delete someday */
	if (sfmmup == ksfmmup) {
		cmn_err(CE_PANIC, "hat_setup called with kas");
	}

	ASSERT(ctx_num);
	sfmmu_setctx_sec(ctx_num);

	/*
	 * Allow ctx to be stolen.
	 */
	if (sfmmup != ksfmmup)
		sfmmu_allow_ctx_steal(sfmmup);

	curthread->t_mmuctx = 0;	/* XXX not used - use it resume */
}

/*
 * Free all the translation resources for the specified address space.
 * Called from as_free when an address space is being destroyed.
 */
void
hat_free_start(struct hat *sfmmup)
{
	ASSERT(AS_WRITE_HELD(sfmmup->sfmmu_as, &sfmmup->sfmmu_as->a_lock));
	ASSERT(sfmmup != ksfmmup);

	sfmmup->sfmmu_free = 1;
}

void
hat_free_end(struct hat *sfmmup)
{
	ASSERT(!sfmmup->sfmmu_lttecnt);
	ASSERT(!sfmmup->sfmmu_rss);

	if (sfmmup->sfmmu_rmstat) {
		hat_freestat(sfmmup->sfmmu_as, NULL);
	}
	sfmmu_tlb_ctx_demap(sfmmup);
	xt_sync(sfmmup->sfmmu_cpusran);
	sfmmu_free_ctx(sfmmup, sfmmutoctx(sfmmup));
	sfmmu_free_sfmmu(sfmmup);

	kmem_cache_free(sfmmuid_cache, sfmmup);
}

/*
 * Set up any translation structures, for the specified address space,
 * that are needed or preferred when the process is being swapped in.
 */
/* ARGSUSED */
void
hat_swapin(struct hat *hat)
{
}

/*
 * Free all of the translation resources, for the specified address space,
 * that can be freed while the process is swapped out. Called from as_swapout.
 * Also, free up the ctx that this process was using.
 */
void
hat_swapout(struct hat *sfmmup)
{
	struct hmehash_bucket *hmebp;
	struct hme_blk *hmeblkp;
	struct hme_blk *pr_hblk = NULL;
	struct hme_blk *nx_hblk;
	struct ctx *ctx;
	int i;
	u_longlong_t hblkpa, prevpa, nx_pa;

	/*
	 * There is no way to go from an as to all its translations in sfmmu.
	 * Here is one of the times when we take the big hit and traverse
	 * the hash looking for hme_blks to free up.  Not only do we free up
	 * this as hme_blks but all those that are free.  We are obviously
	 * swaping because we need memory so let's free up as much
	 * as we can.
	 */
	ASSERT(sfmmup != KHATID);
	for (i = 0; i <= UHMEHASH_SZ; i++) {
		hmebp = &uhme_hash[i];

		SFMMU_HASH_LOCK(hmebp);
		hmeblkp = hmebp->hmeblkp;
		hblkpa = hmebp->hmeh_nextpa;
		prevpa = 0;
		pr_hblk = NULL;
		while (hmeblkp) {
			if ((hmeblkp->hblk_tag.htag_id == sfmmup) &&
			    !hmeblkp->hblk_shw_bit && !hmeblkp->hblk_lckcnt) {
				sfmmu_hblk_unload(sfmmup, hmeblkp,
					(caddr_t)get_hblk_base(hmeblkp),
					get_hblk_endaddr(hmeblkp), HAT_UNLOAD);
			}
			nx_hblk = hmeblkp->hblk_next;
			nx_pa = hmeblkp->hblk_nextpa;
			if (!hmeblkp->hblk_vcnt && !hmeblkp->hblk_hmecnt) {
				ASSERT(!hmeblkp->hblk_lckcnt);
				sfmmu_hblk_hash_rm(hmebp, hmeblkp,
					prevpa, pr_hblk);
				sfmmu_hblk_free(hmebp, hmeblkp, hblkpa);
			} else {
				pr_hblk = hmeblkp;
				prevpa = hblkpa;
			}
			hmeblkp = nx_hblk;
			hblkpa = nx_pa;
		}
		SFMMU_HASH_UNLOCK(hmebp);
	}

	/*
	 * Now free up the ctx so that others can reuse it.
	 */
	mutex_enter(&ctx_lock);
	ctx = sfmmutoctx(sfmmup);

	if (sfmmup->sfmmu_cnum != INVALID_CONTEXT &&
		rwlock_hword_enter(&ctx->c_refcnt, WRITER_LOCK) == 0) {
		sfmmu_reuse_ctx(ctx, sfmmup);
		/*
		 * Put ctx back to the free list.
		 */
		ctx->c_free = ctxfree;
		ctxfree = ctx;
		rwlock_hword_exit(&ctx->c_refcnt, WRITER_LOCK);
	}
	mutex_exit(&ctx_lock);
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

	if (flag == HAT_DUP_COW) {
		cmn_err(CE_PANIC, "hat_dup: HAT_DUP_COW not supported");
	}
	return (0);
}

/*
 * Set up addr to map to page pp with protection prot.
 * As an optimization we also load the TSB with the
 * corresponding tte but it is no big deal if  the tte gets kicked out.
 */
void
hat_memload(struct hat *hat, caddr_t addr, struct page *gen_pp,
	u_int attr, u_int flags)
{
	tte_t tte;
	machpage_t *pp = PP2MACHPP(gen_pp);

	ASSERT(hat != NULL);
	ASSERT((hat == ksfmmup) ||
		AS_LOCK_HELD(hat->sfmmu_as, &hat->sfmmu_as->a_lock));
	ASSERT(se_assert(&gen_pp->p_selock));
	ASSERT(!((uint)addr & MMU_PAGEOFFSET));
	ASSERT(!(flags & ~SFMMU_LOAD_ALLFLAG));
	ASSERT(!(attr & ~SFMMU_LOAD_ALLATTR));

	if (PP_ISFREE(gen_pp)) {
		cmn_err(CE_PANIC,
			"hat_memload: loading a mapping to free page %x", pp);
	}
	if (flags & ~SFMMU_LOAD_ALLFLAG)
		cmn_err(CE_NOTE, "hat_memload: unsupported flags %d",
		    flags & ~SFMMU_LOAD_ALLFLAG);

	sfmmu_memtte(&tte, pp->p_pagenum, attr, TTE8K);
	sfmmu_tteload_array(hat, &tte, addr, &pp, flags);
}

/*
 * hat_devload can be called to map real memory (e.g.
 * /dev/kmem) and even though hat_devload will determine pf is
 * for memory, it will be unable to get a shared lock on the
 * page (because someone else has it exclusively) and will
 * pass dp = NULL.  If tteload doesn't get a non-NULL
 * page pointer it can't cache memory.
 */
void
hat_devload(struct hat *hat, caddr_t addr, size_t len, u_long pfn,
	u_int attr, int flags)
{
	tte_t tte;
	struct machpage *pp = NULL;
	int use_lgpg = 0;

	ASSERT(hat != NULL);
	ASSERT(!(flags & ~SFMMU_LOAD_ALLFLAG));
	ASSERT(!(attr & ~SFMMU_LOAD_ALLATTR));
	ASSERT((hat == ksfmmup) ||
		AS_LOCK_HELD(hat->sfmmu_as, &hat->sfmmu_as->a_lock));
	if (len == 0)
		cmn_err(CE_PANIC, "hat_devload: zero len");
	if (flags & ~SFMMU_LOAD_ALLFLAG)
		cmn_err(CE_NOTE, "hat_devload: unsupported flags %d",
		    flags & ~SFMMU_LOAD_ALLFLAG);

	/*
	 * If it's a memory page find its pp
	 */
	if (!(flags & HAT_LOAD_NOCONSIST) && pf_is_memory(pfn)) {
		pp = (machpage_t *)page_numtopp_nolock(pfn);
		if (pp == NULL)
			flags |= HAT_LOAD_NOCONSIST;
	}

	if (flags & HAT_LOAD_NOCONSIST) {
		attr |= SFMMU_UNCACHEVTTE;
		use_lgpg = 1;
	}
	if (!pf_is_memory(pfn)) {
		attr |= SFMMU_UNCACHEPTTE | HAT_NOSYNC;
		use_lgpg = 1;
		switch (attr & HAT_ORDER_MASK) {
			case HAT_STRICTORDER:
			case HAT_UNORDERED_OK:
				/*
				 * we set the side effect bit for all non
				 * memory mappings unless merging is ok
				 */
				attr |= SFMMU_SIDEFFECT;
				break;
			case HAT_MERGING_OK:
			case HAT_LOADCACHING_OK:
			case HAT_STORECACHING_OK:
				break;
			default:
				cmn_err(CE_PANIC, "hat_devload: bad attr");
				break;
		}
	}
	while (len) {
		if (!use_lgpg) {
			sfmmu_memtte(&tte, pfn, attr, TTE8K);
			sfmmu_tteload_array(hat, &tte, addr, &pp, flags);
			len -= MMU_PAGESIZE;
			addr += MMU_PAGESIZE;
			pfn++;
			continue;
		}
		/*
		 *  try to use large pages, check va/pa alignments
		 */
		if ((len >= MMU_PAGESIZE4M) &&
		    !((u_long)addr & MMU_PAGEOFFSET4M) &&
		    !(mmu_ptob(pfn) & MMU_PAGEOFFSET4M)) {
			sfmmu_memtte(&tte, pfn, attr, TTE4M);
			sfmmu_tteload_array(hat, &tte, addr, &pp, flags);
			len -= MMU_PAGESIZE4M;
			addr += MMU_PAGESIZE4M;
			pfn += MMU_PAGESIZE4M / MMU_PAGESIZE;
		} else if ((len >= MMU_PAGESIZE512K) &&
		    !((u_long)addr & MMU_PAGEOFFSET512K) &&
		    !(mmu_ptob(pfn) & MMU_PAGEOFFSET512K)) {
			sfmmu_memtte(&tte, pfn, attr, TTE512K);
			sfmmu_tteload_array(hat, &tte, addr, &pp, flags);
			len -= MMU_PAGESIZE512K;
			addr += MMU_PAGESIZE512K;
			pfn += MMU_PAGESIZE512K / MMU_PAGESIZE;
		} else if ((len >= MMU_PAGESIZE64K) &&
		    !((u_long)addr & MMU_PAGEOFFSET64K) &&
		    !(mmu_ptob(pfn) & MMU_PAGEOFFSET64K)) {
			sfmmu_memtte(&tte, pfn, attr, TTE64K);
			sfmmu_tteload_array(hat, &tte, addr, &pp, flags);
			len -= MMU_PAGESIZE64K;
			addr += MMU_PAGESIZE64K;
			pfn += MMU_PAGESIZE64K / MMU_PAGESIZE;
		} else {
			sfmmu_memtte(&tte, pfn, attr, TTE8K);
			sfmmu_tteload_array(hat, &tte, addr, &pp, flags);
			len -= MMU_PAGESIZE;
			addr += MMU_PAGESIZE;
			pfn++;
		}
	}
}

/*
 * Map the largest extend possible out of the page array. The array may NOT
 * be in order.  The largest possible mapping a page can have
 * is specified in the p_cons field.  The p_cons field
 * cannot change as long as there any mappings (large or small)
 * to any of the pages that make up the large page. (ie. any
 * promotion/demotion of page size is not up to the hat but up to
 * the page free list manager).  The array
 * should consist of properly aligned contigous pages that are
 * part of a big page for a large mapping to be created.
 */
void
hat_memload_array(struct hat *hat, caddr_t addr, size_t len,
	struct page **gen_pps, u_int attr, u_int flags)
{
	int  ttesz, numpg, npgs, mapsz;
	tte_t tte;
	machpage_t *pp;
	machpage_t **pps = (machpage_t **)gen_pps;

	ASSERT(!((uint)addr & MMU_PAGEOFFSET));

	/* Get number of pages */
	npgs = len >> MMU_PAGESHIFT;

	if (npgs < NHMENTS || disable_large_pages) {
		sfmmu_memload_batchsmall(hat, addr, pps, attr, flags, npgs);
		return;
	}

	while (npgs >= NHMENTS) {
		pp = *pps;
		for (ttesz = pp->p_cons; ttesz != TTE8K; ttesz--) {
			numpg = TTEPAGES(ttesz);
			mapsz = numpg << MMU_PAGESHIFT;
			if ((npgs >= numpg) &&
			    IsAligned(addr, mapsz) &&
			    IsAligned(pp->p_pagenum, numpg)) {
				/*
				 * At this point we have enough pages and
				 * we know the virtual address and the pfn
				 * are properly aligned.  We still need
				 * to check for physical contiguity but since
				 * it is very likely that this is the case
				 * we will assume they are so and undo
				 * the request if necessary.  It would
				 * be great if we could get a hint flag
				 * like HAT_CONTIG which would tell us
				 * the pages are contigous for sure.
				 */
				sfmmu_memtte(&tte, (*pps)->p_pagenum,
					attr, ttesz);
				if (!sfmmu_tteload_array(hat, &tte, addr,
				    pps, flags)) {
					break;
				}
			}
		}
		if (ttesz == TTE8K) {
			/*
			 * We were not able to map array using a large page
			 * batch a hmeblk or fraction at a time.
			 */
			numpg = ((uint)addr >> MMU_PAGESHIFT) & (NHMENTS-1);
			numpg = NHMENTS - numpg;
			ASSERT(numpg <= npgs);
			mapsz = numpg * MMU_PAGESIZE;
			sfmmu_memload_batchsmall(hat, addr, pps, attr, flags,
							numpg);
		}
		addr += mapsz;
		npgs -= numpg;
		pps += numpg;
	}

	if (npgs) {
		sfmmu_memload_batchsmall(hat, addr, pps, attr, flags, npgs);
	}
}

/*
 * Function tries to batch 8K pages into the same hme blk.
 */
static void
sfmmu_memload_batchsmall(struct hat *hat, caddr_t vaddr, machpage_t **pps,
		    u_int attr, u_int flags, u_int npgs)
{
	machpage_t *pp;
	tte_t tte;
	struct hmehash_bucket *hmebp;
	struct hme_blk *hmeblkp;
	int index;

	while (npgs) {
		/*
		 * Acquire the hash bucket.
		 */
		hmebp = sfmmu_tteload_acquire_hashbucket(hat, vaddr, TTE8K);
		ASSERT(hmebp);

		/*
		 * Find the hment block.
		 */
		hmeblkp = sfmmu_tteload_find_hmeblk(hat, hmebp, vaddr, TTE8K);
		ASSERT(hmeblkp);

		do {
			/*
			 * Make the tte.
			 */
			pp = *pps;
			sfmmu_memtte(&tte, pp->p_pagenum, attr, TTE8K);

			/*
			 * Add the translation.
			 */
			(void) sfmmu_tteload_addentry(hat, hmeblkp, &tte,
					vaddr, pps, flags);

			/*
			 * Goto next page.
			 */
			pps++;
			npgs--;

			/*
			 * Goto next address.
			 */
			vaddr += MMU_PAGESIZE;

			/*
			 * Don't crossover into a different hmentblk.
			 */
			index = ((u_int)vaddr >> MMU_PAGESHIFT) & (NHMENTS-1);

		} while (index != 0 && npgs != 0);

		/*
		 * Release the hash bucket.
		 */

		sfmmu_tteload_release_hashbucket(hmebp);
	}
}

/*
 * Construct a tte for a page:
 *
 * tte_valid = 1
 * tte_size = size
 * tte_nfo = attr & HAT_NOFAULT
 * tte_ie = attr & HAT_STRUCTURE_LE
 * tte_hmenum = hmenum
 * tte_pahi = pp->p_pagenum >> TTE_PASHIFT;
 * tte_palo = pp->p_pagenum & TTE_PALOMASK;
 * tte_ref = 1 (optimization)
 * tte_wr_perm = attr & PROT_WRITE;
 * tte_no_sync = attr & HAT_NOSYNC
 * tte_lock = attr & SFMMU_LOCKTTE
 * tte_cp = !(attr & SFMMU_UNCACHEPTTE)
 * tte_cv = !(attr & SFMMU_UNCACHEVTTE)
 * tte_e = attr & SFMMU_SIDEFFECT
 * tte_priv = !(attr & PROT_USER)
 * tte_hwwr = if nosync is set and it is writable we set the mod bit (opt)
 * tte_glb = 0
 */
void
sfmmu_memtte(tte_t *ttep, u_int pfn, u_int attr, int tte_sz)
{
	ASSERT(!(attr & ~SFMMU_LOAD_ALLATTR));

	ttep->tte_inthi =
		MAKE_TTE_INTHI(tte_sz, 0, pfn, attr);   /* hmenum = 0 */

	ttep->tte_intlo = MAKE_TTE_INTLO(pfn, attr);
	if (TTE_IS_NOSYNC(ttep)) {
		TTE_SET_REF(ttep);
		if (TTE_IS_WRITABLE(ttep)) {
			TTE_SET_MOD(ttep);
		}
	}
}

/*
 * This function will add a translation to the hme_blk and allocate the
 * hme_blk if one does not exist.
 * If a page structure is specified then it will add the
 * corresponding hment to the mapping list.
 * It will also update the hmenum field for the tte.
 */
void
sfmmu_tteload(struct hat *sfmmup, tte_t *ttep, caddr_t vaddr, machpage_t *pp,
	u_int flags)
{
	(void) sfmmu_tteload_array(sfmmup, ttep, vaddr, &pp, flags);
}

/*
 * This function will add a translation to the hme_blk and allocate the
 * hme_blk if one does not exist.
 * If a page structure is specified then it will add the
 * corresponding hment to the mapping list.
 * It will also update the hmenum field for the tte.
 * Furthermore, it attempts to create a large page translation
 * for <addr,hat> at page array pps.  It assumes addr and first
 * pp is correctly aligned.  It returns 0 if successful and 1 otherwise.
 */
static int
sfmmu_tteload_array(sfmmu_t *sfmmup, tte_t *ttep, caddr_t vaddr,
	machpage_t **pps, u_int flags)
{
	struct hmehash_bucket *hmebp;
	struct hme_blk *hmeblkp;
	int ret, size;

	/*
	 * Get mapping size.
	 */
	size = ttep->tte_size;
	ASSERT(!((uint)vaddr & TTE_PAGE_OFFSET(size)));

	/*
	 * Acquire the hash bucket.
	 */
	hmebp = sfmmu_tteload_acquire_hashbucket(sfmmup, vaddr, size);
	ASSERT(hmebp);

	/*
	 * Find the hment block.
	 */
	hmeblkp = sfmmu_tteload_find_hmeblk(sfmmup, hmebp, vaddr, size);
	ASSERT(hmeblkp);

	/*
	 * Add the translation.
	 */
	ret = sfmmu_tteload_addentry(sfmmup, hmeblkp, ttep, vaddr, pps, flags);

	/*
	 * Release the hash bucket.
	 */

	sfmmu_tteload_release_hashbucket(hmebp);

	return (ret);


}

/*
 * Function locks and returns a pointer to the hash bucket for vaddr and size.
 */
static struct hmehash_bucket *
sfmmu_tteload_acquire_hashbucket(sfmmu_t *sfmmup, caddr_t vaddr, int size)
{
	struct hmehash_bucket *hmebp;
	int hmeshift;

	hmeshift = HME_HASH_SHIFT(size);

	hmebp = HME_HASH_FUNCTION(sfmmup, vaddr, hmeshift);

	SFMMU_HASH_LOCK(hmebp);

	return (hmebp);
}

/*
 * Function returns a pointer to an hmeblk in the hash bucket, hmebp. If the
 * hmeblk doesn't exists for the [sfmmup, vaddr & size] signature, a hmeblk is
 * allocated.
 */
static struct hme_blk *
sfmmu_tteload_find_hmeblk(sfmmu_t *sfmmup, struct hmehash_bucket *hmebp,
	caddr_t vaddr, int size)
{
	hmeblk_tag hblktag;
	int hmeshift;
	struct hme_blk *hmeblkp, *pr_hblk;
	u_longlong_t hblkpa, prevpa;

	hblktag.htag_id = sfmmup;
	hmeshift = HME_HASH_SHIFT(size);
	hblktag.htag_bspage = HME_HASH_BSPAGE(vaddr, hmeshift);
	hblktag.htag_rehash = HME_HASH_REHASH(size);

ttearray_realloc:

	HME_HASH_SEARCH_PREV(hmebp, hblktag, hmeblkp, hblkpa, pr_hblk, prevpa);
	if (hmeblkp == NULL) {
		hmeblkp = sfmmu_hblk_alloc(sfmmup, vaddr, hmebp, size, hblktag);
	} else {
		/*
		 * It is possible for 8k and 64k hblks to collide since they
		 * have the same rehash value. This is because we
		 * lazily free hblks and 8K/64K blks could be lingering.
		 * If we find size mismatch we free the block and & try again.
		 */
		if (get_hblk_ttesz(hmeblkp) != size) {
			ASSERT(!hmeblkp->hblk_vcnt);
			ASSERT(!hmeblkp->hblk_hmecnt);
			sfmmu_hblk_hash_rm(hmebp, hmeblkp, prevpa, pr_hblk);
			sfmmu_hblk_free(hmebp, hmeblkp, hblkpa);
			goto ttearray_realloc;
		}
		if (hmeblkp->hblk_shw_bit) {
			/*
			 * if the hblk was previously used as a shadow hblk then
			 * we will change it to a normal hblk
			 */
			if (hmeblkp->hblk_shw_mask) {
				sfmmu_shadow_hcleanup(sfmmup, hmeblkp, hmebp);
				ASSERT(SFMMU_HASH_LOCK_ISHELD(hmebp));
				goto ttearray_realloc;
			} else {
				hmeblkp->hblk_shw_bit = 0;
			}
		}
		SFMMU_STAT(sf_hblk_hit);
	}

	ASSERT(get_hblk_ttesz(hmeblkp) == size);
	ASSERT(!hmeblkp->hblk_shw_bit);

	return (hmeblkp);
}

/*
 * Function adds a tte entry into the hmeblk. It returns 0 if successful and 1
 * otherwise.
 */
static int
sfmmu_tteload_addentry(sfmmu_t *sfmmup, struct hme_blk *hmeblkp, tte_t *ttep,
	caddr_t vaddr, machpage_t **pps, u_int flags)
{
	machpage_t *pp = *pps;
	int hmenum, size, cnt;
	int remap;
	tte_t tteold, flush_tte;
#ifdef DEBUG
	tte_t orig_old;
#endif DEBUG
	struct sf_hment *sfhme;
	kmutex_t *pml;
	struct ctx *ctx;

	/*
	 * remove this panic when we decide to let user virtual address
	 * space be >= USERLIMIT.
	 */
	if (!TTE_IS_PRIVILEGED(ttep) && (u_int)vaddr >= USERLIMIT) {
			cmn_err(CE_PANIC, "user addr %x in kernel space\n",
				vaddr);
	}
	if (TTE_IS_GLOBAL(ttep)) {
		cmn_err(CE_PANIC, "sfmmu_tteload: creating global tte\n");
	}


#ifdef	DEBUG
	if (pf_is_memory(sfmmu_ttetopfn(ttep, vaddr)) &&
	    !TTE_IS_PCACHEABLE(ttep)) {
		cmn_err(CE_PANIC, "sfmmu_tteload: non cacheable memory tte\n");
	}
#endif /* DEBUG */


	if (flags & HAT_LOAD_SHARE) {
		/*
		 * Don't load TSB for dummy as in ISM
		 */
		flags |= SFMMU_NO_TSBLOAD;
	}

	size = ttep->tte_size;
	switch (size) {
	case (TTE8K):
		SFMMU_STAT(sf_tteload8k);
		break;
	case (TTE64K):
		SFMMU_STAT(sf_tteload64k);
		break;
	case (TTE512K):
		SFMMU_STAT(sf_tteload512k);
		break;
	case (TTE4M):
		SFMMU_STAT(sf_tteload4m);
		break;
	}

	ASSERT(!((uint)vaddr & TTE_PAGE_OFFSET(size)));

	sfhme = sfmmu_hblktohme(hmeblkp, vaddr, &hmenum);

	/*
	 * Need to grab mlist lock here so that pageunload
	 * will not change tte behind us.
	 */
	if (pp) {
		pml = sfmmu_mlist_enter(pp);
	}

	sfmmu_copytte(&sfhme->hme_tte, &tteold);
	/*
	 * Look for corresponding hment and if valid verify
	 * pfns are equal.
	 */
	remap = TTE_IS_VALID(&tteold);
	if (remap) {
		if (TTE_TO_PFN(vaddr, &tteold) != TTE_TO_PFN(vaddr, ttep)) {
			cmn_err(CE_PANIC, "sfmmu_tteload - tte remap,"
				"hmeblkp 0x%x\n", hmeblkp);
		}
		ASSERT(tteold.tte_size == ttep->tte_size);
	}

	if (pp) {
		if (size == TTE8K) {
			/*
			 * Handle VAC consistency
			 */
			if (!remap && (cache & CACHE_VAC) && !PP_ISNC(pp)) {
				sfmmu_vac_conflict(sfmmup, vaddr, pp);
			}

			if (TTE_IS_WRITABLE(ttep) && PP_ISRO(pp)) {
				sfmmu_page_enter(pp);
				PP_CLRRO(pp);
				sfmmu_page_exit(pp);
			} else if (!PP_ISMAPPED(pp) &&
			    (!TTE_IS_WRITABLE(ttep)) && !(PP_ISMOD(pp))) {
				sfmmu_page_enter(pp);
				if (!(PP_ISMOD(pp))) {
					PP_SETRO(pp);
				}
				sfmmu_page_exit(pp);
			}

		} else if (sfmmu_pagearray_setup(vaddr, pps, ttep, remap)) {
			/*
			 * sfmmu_pagearray_setup failed so return
			 */
			sfmmu_mlist_exit(pml);
			return (1);
		}
	}

	/*
	 * Make sure hment is not on a mapping list.
	 */
	ASSERT(remap || (sfhme->hme_page == NULL));

	/* if it is not a remap then hme->next better be NULL */
	ASSERT((!remap) ? sfhme->hme_next == NULL : 1);

	if (flags & HAT_LOAD_LOCK) {
		if (((int)hmeblkp->hblk_lckcnt + 1) >= MAX_HBLK_LCKCNT) {
			cmn_err(CE_PANIC,
				"too high lckcnt-hmeblk = 0x%x\n", hmeblkp);
		}
		atomic_add_hword(&hmeblkp->hblk_lckcnt, 1, NULL);
	}

	if (pp && PP_ISNC(pp)) {
		/*
		 * If the physical page is marked to be uncacheable, like
		 * by a vac conflict, make sure the new mapping is also
		 * uncacheable.
		 */
		TTE_CLR_VCACHEABLE(ttep);
		ASSERT(PP_GET_VCOLOR(pp) == NO_VCOLOR);
	}
	ttep->tte_hmenum = hmenum;

#ifdef DEBUG
	orig_old = tteold;
#endif DEBUG

	while (sfmmu_modifytte_try(&tteold, ttep, &sfhme->hme_tte) < 0) {
		;
#ifdef DEBUG
		chk_tte(&orig_old, &tteold, ttep, hmeblkp);
#endif DEBUG
	}

	if (!TTE_IS_VALID(&tteold)) {
		cnt = TTEPAGES(size);
		atomic_add_hword(&hmeblkp->hblk_vcnt, 1, NULL);
		atomic_add_word((u_int *)&sfmmup->sfmmu_rss, cnt, NULL);
		if (size != TTE8K) {

			/*
			 * Make sure that we have a valid ctx and
			 * it doesn't get stolen after this point.
			 */
			if (sfmmup != ksfmmup)
				sfmmu_disallow_ctx_steal(sfmmup);

			ctx = sfmmutoctx(sfmmup);
			atomic_add_hword(&sfmmup->sfmmu_lttecnt, 1, NULL);
			ctx->c_flags |= LTTES_FLAG;
			/*
			 * Now we can allow our ctx to be stolen.
			 */
			if (sfmmup != ksfmmup)
				sfmmu_allow_ctx_steal(sfmmup);
		}
	}
	ASSERT(TTE_IS_VALID(&sfhme->hme_tte));

	SFMMU_STACK_TRACE(hmeblkp, hmenum)

	flush_tte.tte_intlo = (tteold.tte_intlo ^ ttep->tte_intlo) &
	    hw_tte.tte_intlo;
	flush_tte.tte_inthi = (tteold.tte_inthi ^ ttep->tte_inthi) &
	    hw_tte.tte_inthi;

	if (remap && (flush_tte.tte_inthi || flush_tte.tte_intlo)) {
		/*
		 * If remap and new tte differs from old tte we need
		 * to sync the mod bit and flush tlb/tsb.  We don't
		 * need to sync ref bit because we currently always set
		 * ref bit in tteload.
		 */
		ASSERT(TTE_IS_REF(ttep));
		if (TTE_IS_MOD(&tteold)) {
			sfmmu_ttesync(sfmmup, vaddr, &tteold, pp);
		}
		sfmmu_tlb_demap(vaddr, sfmmup, hmeblkp, 0);
		if (sfmmup == ksfmmup) {
			xt_sync(hmeblkp->hblk_cpuset);
		} else {
			xt_sync(sfmmup->sfmmu_cpusran);
		}
	}

	if ((size == TTE8K) && !(flags & SFMMU_NO_TSBLOAD)) {

		/*
		 * Make sure that we have a valid ctx and
		 * it doesn't get stolen after this point.
		 */
		if (sfmmup != ksfmmup)
			sfmmu_disallow_ctx_steal(sfmmup);

		/*
		 * We need to disable premption because we need to make
		 * sure the cpuset that we are updating corresponds with
		 * the actual cpu where we are loading the tsb/tlb.
		 * If we decide to have users use the hblk_cpuset then
		 * we should make the sfmmu_load_tsb routine do it
		 * all it one.
		 */
		kpreempt_disable();

		if ((sfmmup == ksfmmup) &&
		    !CPU_IN_SET(hmeblkp->hblk_cpuset, CPU->cpu_id)) {
			/* update the hmeblkp cpuset field */

			CPUSET_CAS(hmeblkp->hblk_cpuset, CPU->cpu_id);
		}
		sfmmu_load_tsb(vaddr, sfmmup->sfmmu_cnum, ttep);

		kpreempt_enable();

		/*
		 * Now we can allow our ctx to be stolen.
		 */
		if (sfmmup != ksfmmup)
			sfmmu_allow_ctx_steal(sfmmup);
	}
	if (pp) {
		if (!remap) {
			hme_add(sfhme, pp);
			atomic_add_hword(&hmeblkp->hblk_hmecnt, 1, NULL);
			ASSERT(hmeblkp->hblk_hmecnt > 0);

			/*
			 * Cannot ASSERT(hmeblkp->hblk_hmecnt <= NHMENTS)
			 * see pageunload() for comment.
			 */
		}
		sfmmu_mlist_exit(pml);
	}

	return (0);
}

/*
 * Function unlocks hash bucket.
 */
static void
sfmmu_tteload_release_hashbucket(struct hmehash_bucket *hmebp)
{
	ASSERT(SFMMU_HASH_LOCK_ISHELD(hmebp));
	SFMMU_HASH_UNLOCK(hmebp);
}

/*
 * function which checks and sets up page array for a large
 * translation.  Will set p_vcolor, p_index, p_ro fields.
 * Assumes addr and pfnum of first page are properly aligned.
 * Will check for physical contiguity. If check fails it return
 * non null.
 */
static int
sfmmu_pagearray_setup(caddr_t addr, machpage_t **pps, tte_t *ttep, int remap)
{
	int i, npgs, pfnum, index, ttesz;
	int cflags = 0;
	machpage_t *pp;

	ttesz = ttep->tte_size;

	ASSERT(ttesz > TTE8K);

	npgs = TTEPAGES(ttesz);
	index = PAGESZ_TO_INDEX(ttesz);

	pfnum = (*pps)->p_pagenum;
	ASSERT(IsAligned(pfnum, npgs));

	for (i = 0; i < npgs; i++, pps++) {
		pp = *pps;

		/*
		 * XXX is it possible to maintain P_RO on the root only?
		 */
		if (TTE_IS_WRITABLE(ttep) && PP_ISRO(pp)) {
			sfmmu_page_enter(pp);
			PP_CLRRO(pp);
			sfmmu_page_exit(pp);
		} else if (!PP_ISMAPPED(pp) && !TTE_IS_WRITABLE(ttep) &&
		    !PP_ISMOD(pp)) {
			sfmmu_page_enter(pp);
			if (!(PP_ISMOD(pp))) {
				PP_SETRO(pp);
			}
			sfmmu_page_exit(pp);
		}

		/*
		 * If this is a remap we skip vac & contiguity checks.
		 */
		if (remap)
			continue;

		/*
		 * set P_TNC, p_vcolor and detect any vac conflicts.
		 */
		if (sfmmu_vacconflict_array(addr, pp, &cflags)) {
			int j;
			/*
			 * detected a conflict, resolve conflict for
			 * previous and current pages.
			 */
			for (j = 0; j <= i; j++) {
				sfmmu_vacconflict_array(addr, pps[j], &cflags);
			}
		}

		/*
		 * save current index in case we need to undo it.
		 */
		pp->p_index = ((pp->p_index << SFMMU_INDEX_SHIFT) | index |
		    PP_MAPINDEX(pp));

		/*
		 * contiguity check
		 */
		if (pp->p_pagenum != pfnum) {
			/*
			 * If we fail the contiguity test then
			 * the only thing we need to fix is the p_index field.
			 * We might get a few extra flushes but since this
			 * path is rare that is ok.  The p_ro field will
			 * get automatically fixed on the next tteload to
			 * the page.  Same applies to TMP_NC.
			 */
			while (i >= 0) {
				pp = *pps;
				pp->p_index = pp->p_index >> SFMMU_INDEX_SHIFT;
				pps--;
				i--;
			}
			return (1);
		}
		pfnum++;
		addr += MMU_PAGESIZE;
	}

	return (0);
}

/*
 * Routine that manages (detects & resolves) vac consistency
 * for a large page. If we detect a vac conflict all mappings
 * to the large page will be marked uncacheable.
 */
static int
sfmmu_vacconflict_array(caddr_t addr, machpage_t *pp, int *cflags)
{
	int vcolor;

	if (*cflags & CACHE_UNCACHE) {
		/*
		 * A previous vac conflict is forcing all mappins to this
		 * large page to be marked uncacheable.
		 */
		vcolor = PP_GET_VCOLOR(pp);
		if (!CacheColor_IsFlushed(*cflags, vcolor)) {
			CacheColor_SetFlushed(*cflags, vcolor);
			sfmmu_cache_flushcolor(vcolor);
		}
		sfmmu_page_cache(pp, HAT_TMPNC, CACHE_NO_FLUSH);
		return (0);
	}

	if (PP_ISNC(pp)) {
		/*
		 * If any page is marked uncacheble then the entire large
		 * page has to be uncacheable.
		 */
		*cflags |= CACHE_UNCACHE;
		return (1);
	}

	vcolor = addr_to_vcolor(addr);
	if (PP_NEWPAGE(pp)) {
		PP_SET_VCOLOR(pp, vcolor);
		return (0);
	}
	if (PP_GET_VCOLOR(pp) == vcolor) {
		return (0);
	}
	if (!PP_ISMAPPED(pp)) {
		/*
		 * Previous user of page had a differnet color
		 * but since there are no current users
		 * we just flush the cache and change the color.
		 * As an optimization for large pages we
		 * flush the entire cache and set a flag.
		 * RFE if this routine supports small pages then
		 * make sure we only flush that color for small pages.
		 *
		 */
		if (!CacheColor_IsFlushed(*cflags, PP_GET_VCOLOR(pp))) {
			CacheColor_SetFlushed(*cflags, PP_GET_VCOLOR(pp));
			sfmmu_cache_flush(pp->p_pagenum, PP_GET_VCOLOR(pp));
		}
		PP_SET_VCOLOR(pp, vcolor);
		return (0);
	}
	/*
	 * We got a real conflict with a current mapping.
	 * set flags to start unencaching all mappings
	 * and return failure so we restart looping
	 * the pp array from the beginning.
	 */
	*cflags |= CACHE_UNCACHE;
	return (1);
}

/*
 * creates a large page shadow hmeblk for a tte.
 * The purpose of this routine is to allow us to do quick unloads because
 * the vm layer can easily pass a very large but sparsely populated range.
 */
static struct hme_blk *
sfmmu_shadow_hcreate(sfmmu_t *sfmmup, caddr_t vaddr, int ttesz)
{
	struct hmehash_bucket *hmebp;
	hmeblk_tag hblktag;
	int hmeshift, size, vshift;
	uint shw_mask, newshw_mask;
	struct hme_blk *hmeblkp;
	extern int cas();

	ASSERT(sfmmup != KHATID);
	ASSERT(ttesz < TTE4M);

	size = (ttesz == TTE8K)? TTE512K : ++ttesz;

	hblktag.htag_id = sfmmup;
	hmeshift = HME_HASH_SHIFT(size);
	hblktag.htag_bspage = HME_HASH_BSPAGE(vaddr, hmeshift);
	hblktag.htag_rehash = HME_HASH_REHASH(size);
	hmebp = HME_HASH_FUNCTION(sfmmup, vaddr, hmeshift);

	SFMMU_HASH_LOCK(hmebp);

	HME_HASH_FAST_SEARCH(hmebp, hblktag, hmeblkp);
	if (hmeblkp == NULL) {
		hmeblkp = sfmmu_hblk_alloc(sfmmup, vaddr, hmebp, size, hblktag);
	}
	ASSERT(hmeblkp);
	if (!hmeblkp->hblk_shw_mask) {
		/*
		 * if this is a unused hblk it was just allocated or could
		 * potentially be a previous large page hblk so we need to
		 * set the shadow bit.
		 */
		hmeblkp->hblk_shw_bit = 1;
	}
	ASSERT(hmeblkp->hblk_shw_bit == 1);
	vshift = vaddr_to_vshift(hblktag, vaddr, size);
	ASSERT(vshift < 8);
	/*
	 * Atomically set shw mask bit
	 */
	do {
		shw_mask = hmeblkp->hblk_shw_mask;
		newshw_mask = shw_mask | (1 << vshift);
		newshw_mask = cas(&hmeblkp->hblk_shw_mask, shw_mask,
			newshw_mask);
	} while (newshw_mask != shw_mask);

	SFMMU_HASH_UNLOCK(hmebp);

	return (hmeblkp);
}

/*
 * This routine cleanup a previous shadow hmeblk and changes it to
 * a regular hblk.  This happens rarely but it is possible
 * when a process wants to use large pages and there are hblks still
 * lying around from the previous as that used these hmeblks.
 * The alternative was to cleanup the shadow hblks at unload time
 * but since so few user processes actually use large pages, it is
 * better to be lazy and cleanup at this time.
 */
static void
sfmmu_shadow_hcleanup(sfmmu_t *sfmmup, struct hme_blk *hmeblkp,
	struct hmehash_bucket *hmebp)
{
	caddr_t addr, endaddr;
	int hashno, size;

	ASSERT(hmeblkp->hblk_shw_bit);

	ASSERT(SFMMU_HASH_LOCK_ISHELD(hmebp));

	if (!hmeblkp->hblk_shw_mask) {
		hmeblkp->hblk_shw_bit = 0;
		return;
	}
	addr = (caddr_t)get_hblk_base(hmeblkp);
	endaddr = get_hblk_endaddr(hmeblkp);
	size = get_hblk_ttesz(hmeblkp);
	hashno = size - 1;
	ASSERT(hashno > 0);
	SFMMU_HASH_UNLOCK(hmebp);

	sfmmu_free_hblks(sfmmup, addr, endaddr, hashno);

	SFMMU_HASH_LOCK(hmebp);
}

static void
sfmmu_free_hblks(sfmmu_t *sfmmup, caddr_t addr, caddr_t endaddr,
	int hashno)
{
	int hmeshift, shadow = 0;
	hmeblk_tag hblktag;
	struct hmehash_bucket *hmebp;
	struct hme_blk *hmeblkp;
	struct hme_blk *nx_hblk, *pr_hblk;
	u_longlong_t hblkpa, prevpa, nx_pa;

	ASSERT(hashno > 0);
	hblktag.htag_id = sfmmup;
	hblktag.htag_rehash = hashno;

	hmeshift = HME_HASH_SHIFT(hashno);

	while (addr < endaddr) {
		hblktag.htag_bspage = HME_HASH_BSPAGE(addr, hmeshift);
		hmebp = HME_HASH_FUNCTION(sfmmup, addr, hmeshift);

		SFMMU_HASH_LOCK(hmebp);

		/* inline HME_HASH_SEARCH */
		hmeblkp = hmebp->hmeblkp;
		hblkpa = hmebp->hmeh_nextpa;
		prevpa = 0;
		pr_hblk = NULL;
		while (hmeblkp) {
			ASSERT(hblkpa == va_to_pa((caddr_t)hmeblkp));
			if (hmeblkp->hblk_tag.htag_tag == hblktag.htag_tag) {
				/* found hme_blk */
				if (hmeblkp->hblk_shw_bit) {
					if (hmeblkp->hblk_shw_mask) {
						shadow = 1;
						sfmmu_shadow_hcleanup(sfmmup,
							hmeblkp, hmebp);
						break;
					} else {
						hmeblkp->hblk_shw_bit = 0;
					}
				}

				/*
				 * Hblk_hmecnt and hblk_vcnt could be non zero
				 * since hblk_unload() does not gurantee that.
				 *
				 * XXX - this could cause tteload() to spin
				 * where sfmmu_shadow_hcleanup() is called.
				 */
			}

			nx_hblk = hmeblkp->hblk_next;
			nx_pa = hmeblkp->hblk_nextpa;
			if (!hmeblkp->hblk_vcnt && !hmeblkp->hblk_hmecnt) {
				sfmmu_hblk_hash_rm(hmebp, hmeblkp, prevpa,
					pr_hblk);
				sfmmu_hblk_free(hmebp, hmeblkp, hblkpa);
			} else {
				pr_hblk = hmeblkp;
				prevpa = hblkpa;
			}
			hmeblkp = nx_hblk;
			hblkpa = nx_pa;
		}

		SFMMU_HASH_UNLOCK(hmebp);

		if (shadow) {
			/*
			 * We found another shadow hblk so cleaned its
			 * children.  We need to go back and cleanup
			 * the original hblk so we don't change the
			 * addr.
			 */
			shadow = 0;
		} else {
			addr = (caddr_t)roundup((u_int)addr + 1,
				(1 << hmeshift));
		}
	}
}

/*
 * Release one hardware address translation lock on the given address range.
 */
void
hat_unlock(struct hat *sfmmup, caddr_t addr, size_t len)
{
	struct hmehash_bucket *hmebp;
	hmeblk_tag hblktag;
	int hmeshift, hashno = 1;
	struct hme_blk *hmeblkp;
	caddr_t endaddr;

	ASSERT(sfmmup != NULL);
	ASSERT((sfmmup == ksfmmup) ||
		AS_LOCK_HELD(sfmmup->sfmmu_as, &sfmmup->sfmmu_as->a_lock));
	ASSERT((len & MMU_PAGEOFFSET) == 0);
	endaddr = addr + len;
	hblktag.htag_id = sfmmup;

	/*
	 * Spitfire supports 4 page sizes.
	 * Most pages are expected to be of the smallest page size (8K) and
	 * these will not need to be rehashed. 64K pages also don't need to be
	 * rehashed because an hmeblk spans 64K of address space. 512K pages
	 * might need 1 rehash and and 4M pages might need 2 rehashes.
	 */
	while (addr < endaddr) {
		hmeshift = HME_HASH_SHIFT(hashno);
		hblktag.htag_bspage = HME_HASH_BSPAGE(addr, hmeshift);
		hblktag.htag_rehash = hashno;
		hmebp = HME_HASH_FUNCTION(sfmmup, addr, hmeshift);

		SFMMU_HASH_LOCK(hmebp);

		HME_HASH_SEARCH(hmebp, hblktag, hmeblkp);
		if (hmeblkp != NULL) {
			addr = sfmmu_hblk_unlock(hmeblkp, addr, endaddr);
			SFMMU_HASH_UNLOCK(hmebp);
			hashno = 1;
			continue;
		}
		SFMMU_HASH_UNLOCK(hmebp);

		if (!sfmmup->sfmmu_lttecnt || (hashno >= MAX_HASHCNT)) {
			/*
			 * We have traversed the whole list and rehashed
			 * if necessary without finding the address to unlock
			 * which should never happen.
			 */
			cmn_err(CE_PANIC,
				"sfmmu_unlock: addr not found."
				"addr = 0x%x hat = 0x%x\n", addr, sfmmup);
			addr += MMU_PAGESIZE;
		} else {
			hashno++;
		}
	}
}

/*
 * Function to unlock a range of addresses in an hmeblk.  It returns the
 * next address that needs to be unlocked.
 * Should be called with the hash lock held.
 */
static caddr_t
sfmmu_hblk_unlock(struct hme_blk *hmeblkp, caddr_t addr, caddr_t endaddr)
{
	struct sf_hment *sfhme;
	tte_t tteold;
	int ttesz;

	ASSERT(in_hblk_range(hmeblkp, addr));

	endaddr = MIN(endaddr, get_hblk_endaddr(hmeblkp));
	ttesz = get_hblk_ttesz(hmeblkp);

	sfhme = sfmmu_hblktohme(hmeblkp, addr, NULL);
	while (addr < endaddr) {
		sfmmu_copytte(&sfhme->hme_tte, &tteold);
		if (TTE_IS_VALID(&tteold)) {
			if (hmeblkp->hblk_lckcnt <= 0) {
				cmn_err(CE_PANIC, "negative tte lckcnt\n");
			}
			if (((uint)addr + TTEBYTES(ttesz)) > (uint)endaddr) {
				cmn_err(CE_PANIC, "can't unlock large tte\n");
			}
			atomic_add_hword(&hmeblkp->hblk_lckcnt, -1, NULL);
			ASSERT(hmeblkp->hblk_lckcnt >= 0);
		} else {
			cmn_err(CE_PANIC, "sfmmu_hblk_unlock: invalid tte");
		}
		addr += TTEBYTES(ttesz);
		sfhme++;
		SFMMU_STACK_TRACE(hmeblkp, tteold.tte_hmenum)
	}
	return (addr);
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
 */
int
hat_probe(struct hat *sfmmup, caddr_t addr)
{
	ASSERT(sfmmup != NULL);
	ASSERT((sfmmup == ksfmmup) ||
		AS_LOCK_HELD(sfmmup->sfmmu_as, &sfmmup->sfmmu_as->a_lock));

	if (sfmmu_vatopfn(addr, sfmmup) != -1)
		return (1);
	else
		return (0);
}

size_t
hat_getpagesize(struct hat *sfmmup, caddr_t addr)
{
	tte_t tte;

	tte = sfmmu_gettte(sfmmup, addr);
	if (TTE_IS_VALID(&tte)) {
		return (TTEBYTES(tte.tte_size));
	}
	return ((size_t)-1);
}

static tte_t
sfmmu_gettte(struct hat *sfmmup, caddr_t addr)
{
	struct hmehash_bucket *hmebp;
	hmeblk_tag hblktag;
	int hmeshift, hashno = 1;
	struct hme_blk *hmeblkp;
	struct sf_hment *sfhmep;
	tte_t tte;


	ASSERT(!((u_int)addr & MMU_PAGEOFFSET));

	hblktag.htag_id = sfmmup;
	tte.ll = 0;

	do {
		hmeshift = HME_HASH_SHIFT(hashno);
		hblktag.htag_bspage = HME_HASH_BSPAGE(addr, hmeshift);
		hblktag.htag_rehash = hashno;
		hmebp = HME_HASH_FUNCTION(sfmmup, addr, hmeshift);

		SFMMU_HASH_LOCK(hmebp);

		HME_HASH_SEARCH(hmebp, hblktag, hmeblkp);
		if (hmeblkp != NULL) {
			sfhmep = sfmmu_hblktohme(hmeblkp, addr, 0);
			sfmmu_copytte(&sfhmep->hme_tte, &tte);
			SFMMU_HASH_UNLOCK(hmebp);
			return (tte);
		}
		SFMMU_HASH_UNLOCK(hmebp);
		hashno++;
	} while (sfmmup->sfmmu_lttecnt && (hashno <= MAX_HASHCNT));
	/*
	 * We have traversed the entire hmeblk list and
	 * rehashed if necessary without finding the addr.
	 */
	return (tte);
}

u_int
hat_getattr(struct hat *sfmmup, caddr_t addr, u_int *attr)
{
	tte_t tte;

	tte = sfmmu_gettte(sfmmup, addr);
	if (TTE_IS_VALID(&tte)) {
		*attr = sfmmu_ptov_attr(&tte);
		return (0);
	}
	*attr = 0;
	return ((u_int)0xffffffff);
}

/*
 * Enables more attributes on specified address range (ie. logical OR)
 */
void
hat_setattr(struct hat *hat, caddr_t addr, size_t len, u_int attr)
{
	sfmmu_chgattr(hat, addr, len, attr, SFMMU_SETATTR);
}

/*
 * Assigns attributes to the specified address range.  All the attributes
 * are specified.
 */
void
hat_chgattr(struct hat *hat, caddr_t addr, size_t len, u_int attr)
{
	sfmmu_chgattr(hat, addr, len, attr, SFMMU_CHGATTR);
}

/*
 * Remove attributes on the specified address range (ie. loginal NAND)
 */
void
hat_clrattr(struct hat *hat, caddr_t addr, size_t len, u_int attr)
{
	sfmmu_chgattr(hat, addr, len, attr, SFMMU_CLRATTR);
}

/*
 * Change attributes on an address range to that specified by attr and mode.
 */
static void
sfmmu_chgattr(struct hat *sfmmup, caddr_t addr, size_t len, u_int attr,
	int mode)
{
	struct hmehash_bucket *hmebp;
	hmeblk_tag hblktag;
	int hmeshift, hashno = 1;
	struct hme_blk *hmeblkp;
	caddr_t endaddr;
	cpuset_t cpuset;

	CPUSET_ZERO(cpuset);

	ASSERT((sfmmup == ksfmmup) ||
		AS_LOCK_HELD(sfmmup->sfmmu_as, &sfmmup->sfmmu_as->a_lock));
	ASSERT((len & MMU_PAGEOFFSET) == 0);
	ASSERT(((u_int)addr & MMU_PAGEOFFSET) == 0);

	if ((attr & PROT_USER) && (mode != SFMMU_CLRATTR) &&
	    ((addr + len) > (caddr_t)USERLIMIT)) {
		cmn_err(CE_PANIC, "user addr %x in kernel space\n", addr);
	}

	endaddr = addr + len;
	hblktag.htag_id = sfmmup;

	while (addr < endaddr) {
		hmeshift = HME_HASH_SHIFT(hashno);
		hblktag.htag_bspage = HME_HASH_BSPAGE(addr, hmeshift);
		hblktag.htag_rehash = hashno;
		hmebp = HME_HASH_FUNCTION(sfmmup, addr, hmeshift);

		SFMMU_HASH_LOCK(hmebp);

		HME_HASH_SEARCH(hmebp, hblktag, hmeblkp);
		if (hmeblkp != NULL) {
			CPUSET_OR(cpuset, hmeblkp->hblk_cpuset);
			addr = sfmmu_hblk_chgattr(hmeblkp, addr, endaddr,
				attr, mode);
			SFMMU_HASH_UNLOCK(hmebp);
			hashno = 1;
			continue;
		}
		SFMMU_HASH_UNLOCK(hmebp);

		if (!sfmmup->sfmmu_lttecnt || (hashno >= MAX_HASHCNT)) {
			/*
			 * We have traversed the whole list and rehashed
			 * if necessary without finding the address to chgattr.
			 * This is ok so we increment the address by the
			 * smallest page size and continue.
			 */
			addr += MMU_PAGESIZE;
		} else {
			hashno++;
		}
	}

	if (sfmmup != ksfmmup)
		cpuset = sfmmup->sfmmu_cpusran;
	xt_sync(cpuset);
}

/*
 * This function chgattr on a range of addresses in an hmeblk.  It returns the
 * next addres that needs to be chgattr.
 * It should be called with the hash lock held.
 * XXX It should be possible to optimize chgattr by not flushing every time but
 * on the other hand:
 * 1. do one flush crosscall.
 * 2. only flush if we are increasing permissions (make sure this will work)
 */
static caddr_t
sfmmu_hblk_chgattr(struct hme_blk *hmeblkp, caddr_t addr,
	caddr_t endaddr, u_int attr, int mode)
{
	tte_t tte, tteattr, tteflags, ttemod;
	struct sf_hment *sfhmep;
	sfmmu_t *sfmmup;
	int ttesz;
	struct machpage *pp = NULL;
	kmutex_t *pml;
	int ret;

	ASSERT(in_hblk_range(hmeblkp, addr));

	sfmmup = hblktosfmmu(hmeblkp);
	endaddr = MIN(endaddr, get_hblk_endaddr(hmeblkp));
	ttesz = get_hblk_ttesz(hmeblkp);

	tteattr.ll = sfmmu_vtop_attr(attr, mode, &tteflags);
	sfhmep = sfmmu_hblktohme(hmeblkp, addr, 0);
	while (addr < endaddr) {
		sfmmu_copytte(&sfhmep->hme_tte, &tte);
		if (TTE_IS_VALID(&tte)) {
			if ((tte.ll & tteflags.ll) == tteattr.ll) {
				/*
				 * if the new attr is the same as old
				 * continue
				 */
				addr += TTEBYTES(ttesz);
				sfhmep++;
				continue;
			}
			if (!TTE_IS_WRITABLE(&tteattr)) {
				/*
				 * make sure we clear hw modify bit if we
				 * removing write protections
				 */
				tteflags.tte_intlo |= TTE_HWWR_INT;
			}
			ttemod = tte;
			ttemod.ll = (ttemod.ll & ~tteflags.ll) | tteattr.ll;
			ASSERT(TTE_TO_TTEPFN(&ttemod) == TTE_TO_TTEPFN(&tte));
			ret = sfmmu_modifytte_try(&tte, &ttemod,
			    &sfhmep->hme_tte);

			if (ret < 0) {
				/* tte changed underneath us */
				continue;
			}

			pp = sfhmep->hme_page;
			if (tteflags.tte_intlo & TTE_HWWR_INT) {
				/*
				 * need to sync if we are clearing modify bit.
				 */
				if (pp) {
					pml = sfmmu_mlist_enter(pp);
				}
				sfmmu_ttesync(sfmmup, addr, &tte, pp);
				if (pp) {
					sfmmu_mlist_exit(pml);
				}
			}

			if (pp && PP_ISRO(pp)) {
				if (tteattr.tte_intlo & TTE_WRPRM_INT) {
					sfmmu_page_enter(pp);
					PP_CLRRO(pp);
					sfmmu_page_exit(pp);
				}
			}

			if (ret > 0) {
				sfmmu_tlb_demap(addr, sfmmup, hmeblkp, 0);
			}

			SFMMU_STACK_TRACE(hmeblkp, tte.tte_hmenum)
		}
		addr += TTEBYTES(ttesz);
		sfhmep++;
	}
	return (addr);
}

/*
 * This routine converts virtual attributes to physical ones.  It will
 * update the tteflags field with the tte mask corresponding to the attributes
 * affected and it returns the new attributes.  It will also clear the modify
 * bit if we are taking away write permission.  This is necessary since the
 * modify bit is the hardware permission bit and we need to clear it in order
 * to detect write faults.
 */
static u_longlong_t
sfmmu_vtop_attr(u_int attr, int mode, tte_t *ttemaskp)
{
	tte_t ttevalue;

	ASSERT(!(attr & ~SFMMU_LOAD_ALLATTR));

	switch (mode) {
	case (SFMMU_CHGATTR):
		/* all attributes specified */
		ttevalue.tte_inthi = MAKE_TTEATTR_INTHI(attr);
		ttevalue.tte_intlo = MAKE_TTEATTR_INTLO(attr);
		ttemaskp->tte_inthi = TTEINTHI_ATTR;
		ttemaskp->tte_intlo = TTEINTLO_ATTR;
		break;
	case (SFMMU_SETATTR):
		ASSERT(!(attr & ~HAT_PROT_MASK));
		ttemaskp->ll = 0;
		ttevalue.ll = 0;
		/*
		 * a valid tte implies exec and read for sfmmu
		 * so no need to do anything about them.
		 * since priviledged access implies user access
		 * PROT_USER doesn't make sense either.
		 */
		if (attr & PROT_WRITE) {
			ttemaskp->tte_intlo |= TTE_WRPRM_INT;
			ttevalue.tte_intlo |= TTE_WRPRM_INT;
		}
		break;
	case (SFMMU_CLRATTR):
		/* attributes will be nand with current ones */
		if (attr & ~(PROT_WRITE | PROT_USER)) {
			cmn_err(CE_PANIC, "sfmmu: attr %x not supported",
				attr);
		}
		ttemaskp->ll = 0;
		ttevalue.ll = 0;
		if (attr & PROT_WRITE) {
			/* clear both writable and modify bit */
			ttemaskp->tte_intlo |= TTE_WRPRM_INT | TTE_HWWR_INT;
		}
		if (attr & PROT_USER) {
			ttemaskp->tte_intlo |= TTE_PRIV_INT;
			ttevalue.tte_intlo |= TTE_PRIV_INT;
		}
		break;
	default:
		cmn_err(CE_PANIC, "sfmmu_vtop_attr: bad mode %x", mode);
	}
	ASSERT(TTE_TO_TTEPFN(&ttevalue) == 0);
	return (ttevalue.ll);
}

static u_int
sfmmu_ptov_attr(tte_t *ttep)
{
	u_int attr;

	ASSERT(TTE_IS_VALID(ttep));

	attr = PROT_READ | PROT_EXEC;

	if (TTE_IS_WRITABLE(ttep)) {
		attr |= PROT_WRITE;
	}
	if (!TTE_IS_PRIVILEGED(ttep)) {
		attr |= PROT_USER;
	}
	if (TTE_IS_NFO(ttep)) {
		attr |= HAT_NOFAULT;
	}
	if (TTE_IS_NOSYNC(ttep)) {
		attr |= HAT_NOSYNC;
	}
	if (TTE_IS_SIDEFFECT(ttep)) {
		attr |= SFMMU_SIDEFFECT;
	}
	if (!TTE_IS_VCACHEABLE(ttep)) {
		attr |= SFMMU_UNCACHEVTTE;
	}
	if (!TTE_IS_PCACHEABLE(ttep)) {
		attr |= SFMMU_UNCACHEPTTE;
	}
	return (attr);
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
hat_chgprot(struct hat *sfmmup, caddr_t addr, size_t len, u_int vprot)
{
	struct hmehash_bucket *hmebp;
	hmeblk_tag hblktag;
	int hmeshift, hashno = 1;
	struct hme_blk *hmeblkp;
	caddr_t endaddr;
	cpuset_t cpuset;

	ASSERT((len & MMU_PAGEOFFSET) == 0);
	ASSERT(((u_int)addr & MMU_PAGEOFFSET) == 0);

	CPUSET_ZERO(cpuset);

	if ((vprot != (uint)~PROT_WRITE) && (vprot & PROT_USER) &&
	    ((addr + len) > (caddr_t)USERLIMIT)) {
		cmn_err(CE_PANIC, "user addr %x vprot %x in kernel space\n",
			addr, vprot);
	}
	endaddr = addr + len;
	hblktag.htag_id = sfmmup;

	while (addr < endaddr) {
		hmeshift = HME_HASH_SHIFT(hashno);
		hblktag.htag_bspage = HME_HASH_BSPAGE(addr, hmeshift);
		hblktag.htag_rehash = hashno;
		hmebp = HME_HASH_FUNCTION(sfmmup, addr, hmeshift);

		SFMMU_HASH_LOCK(hmebp);

		HME_HASH_SEARCH(hmebp, hblktag, hmeblkp);
		if (hmeblkp != NULL) {
			CPUSET_OR(cpuset, hmeblkp->hblk_cpuset);
			addr = sfmmu_hblk_chgprot(hmeblkp, addr, endaddr,
				vprot);
			SFMMU_HASH_UNLOCK(hmebp);
			hashno = 1;
			continue;
		}
		SFMMU_HASH_UNLOCK(hmebp);

		if (!sfmmup->sfmmu_lttecnt || (hashno >= MAX_HASHCNT)) {
			/*
			 * We have traversed the whole list and rehashed
			 * if necessary without finding the address to chgprot.
			 * This is ok so we increment the address by the
			 * smallest page size and continue.
			 */
			addr += MMU_PAGESIZE;
		} else {
			hashno++;
		}
	}

	if (sfmmup != ksfmmup)
		cpuset = sfmmup->sfmmu_cpusran;
	xt_sync(cpuset);
}

/*
 * This function chgprots a range of addresses in an hmeblk.  It returns the
 * next addres that needs to be chgprot.
 * It should be called with the hash lock held.
 * XXX It shold be possible to optimize chgprot by not flushing every time but
 * on the other hand:
 * 1. do one flush crosscall.
 * 2. only flush if we are increasing permissions (make sure this will work)
 */
static caddr_t
sfmmu_hblk_chgprot(struct hme_blk *hmeblkp, caddr_t addr, caddr_t endaddr,
	u_int vprot)
{
	u_int pprot;
	tte_t tte, ttemod;
	sfmmu_t *sfmmup;
	struct sf_hment *sfhmep;
	u_int tteflags;
	int ttesz;
	struct machpage *pp = NULL;
	kmutex_t *pml;
	int ret;

	ASSERT(in_hblk_range(hmeblkp, addr));

#ifdef DEBUG
	if (get_hblk_ttesz(hmeblkp) != TTE8K &&
	    (endaddr < get_hblk_endaddr(hmeblkp))) {
		cmn_err(CE_PANIC,
		    "sfmmu_hblk_chgprot: partial chgprot of large page\n");
	}
#endif DEBUG

	sfmmup = hblktosfmmu(hmeblkp);
	endaddr = MIN(endaddr, get_hblk_endaddr(hmeblkp));
	ttesz = get_hblk_ttesz(hmeblkp);

	pprot = sfmmu_vtop_prot(vprot, &tteflags);
	sfhmep = sfmmu_hblktohme(hmeblkp, addr, 0);
	while (addr < endaddr) {
		sfmmu_copytte(&sfhmep->hme_tte, &tte);
		if (TTE_IS_VALID(&tte)) {
			if (TTE_GET_LOFLAGS(&tte, tteflags) == pprot) {
				/*
				 * if the new protection is the same as old
				 * continue
				 */
				addr += TTEBYTES(ttesz);
				sfhmep++;
				continue;
			}
			ttemod = tte;
			TTE_SET_LOFLAGS(&ttemod, tteflags, pprot);
			ret = sfmmu_modifytte_try(&tte, &ttemod,
			    &sfhmep->hme_tte);

			if (ret < 0) {
				/* tte changed underneath us */
				continue;
			}

			pp = sfhmep->hme_page;
			if (tteflags & TTE_HWWR_INT) {
				/*
				 * need to sync if we are clearing modify bit.
				 */
				if (pp) {
					pml = sfmmu_mlist_enter(pp);
				}
				sfmmu_ttesync(sfmmup, addr, &tte, pp);
				if (pp) {
					sfmmu_mlist_exit(pml);
				}
			}

			if (pp && PP_ISRO(pp)) {
				if (pprot & TTE_WRPRM_INT) {
					sfmmu_page_enter(pp);
					PP_CLRRO(pp);
					sfmmu_page_exit(pp);
				}
			}

			if (ret > 0) {
				sfmmu_tlb_demap(addr, sfmmup, hmeblkp, 0);
			}

			SFMMU_STACK_TRACE(hmeblkp, tte.tte_hmenum)
		}
		addr += TTEBYTES(ttesz);
		sfhmep++;
	}
	return (addr);
}

/*
 * This routine is deprecated and should only be used by hat_chgprot.
 * The correct routine is sfmmu_vtop_attr.
 * This routine converts virtual page protections to physical ones.  It will
 * update the tteflags field with the tte mask corresponding to the protections
 * affected and it returns the new protections.  It will also clear the modify
 * bit if we are taking away write permission.  This is necessary since the
 * modify bit is the hardware permission bit and we need to clear it in order
 * to detect write faults.
 * It accepts the following special protections:
 * ~PROT_WRITE = remove write permissions.
 * ~PROT_USER = remove user permissions.
 */
static u_int
sfmmu_vtop_prot(u_int vprot, u_int *tteflagsp)
{
	if (vprot == (uint)~PROT_WRITE) {
		*tteflagsp = TTE_WRPRM_INT | TTE_HWWR_INT;
		return (0);		/* will cause wrprm to be cleared */
	}
	if (vprot == (uint)~PROT_USER) {
		*tteflagsp = TTE_PRIV_INT;
		return (0);		/* will cause privprm to be cleared */
	}
	if ((vprot == 0) || (vprot == PROT_USER) ||
		((vprot & PROT_ALL) != vprot)) {
		cmn_err(CE_PANIC, "sfmmu_vtop_prot -- bad prot %x\n", vprot);
	}

	switch (vprot) {

	case (PROT_READ):
	case (PROT_EXEC):
	case (PROT_EXEC | PROT_READ):
		*tteflagsp = TTE_PRIV_INT | TTE_WRPRM_INT | TTE_HWWR_INT;
		return (TTE_PRIV_INT); 		/* set prv and clr wrt */
	case (PROT_WRITE):
	case (PROT_WRITE | PROT_READ):
	case (PROT_EXEC | PROT_WRITE):
	case (PROT_EXEC | PROT_WRITE | PROT_READ):
		*tteflagsp = TTE_PRIV_INT | TTE_WRPRM_INT;
		return (TTE_PRIV_INT | TTE_WRPRM_INT); 	/* set prv and wrt */
	case (PROT_USER | PROT_READ):
	case (PROT_USER | PROT_EXEC):
	case (PROT_USER | PROT_EXEC | PROT_READ):
		*tteflagsp = TTE_PRIV_INT | TTE_WRPRM_INT | TTE_HWWR_INT;
		return (0); 			/* clr prv and wrt */
	case (PROT_USER | PROT_WRITE):
	case (PROT_USER | PROT_WRITE | PROT_READ):
	case (PROT_USER | PROT_EXEC | PROT_WRITE):
	case (PROT_USER | PROT_EXEC | PROT_WRITE | PROT_READ):
		*tteflagsp = TTE_PRIV_INT | TTE_WRPRM_INT;
		return (TTE_WRPRM_INT); 	/* clr prv and set wrt */
	default:
		cmn_err(CE_PANIC, "sfmmu_vtop_prot -- bad prot %x\n", vprot);
	}
	return (0);
}


/*
 * Unload all the mappings in the range [addr..addr+len). addr and len must
 * be MMU_PAGESIZE aligned.
 */
void
hat_unload(struct hat *sfmmup, caddr_t addr, size_t len, u_int flags)
{
	struct hmehash_bucket *hmebp;
	hmeblk_tag hblktag;
	int hmeshift, hashno, iskernel;
	struct hme_blk *hmeblkp, *pr_hblk;
	caddr_t endaddr;
	cpuset_t cpuset;
	u_longlong_t hblkpa, prevpa;

	ASSERT((sfmmup == ksfmmup) || (flags & HAT_UNLOAD_OTHER) || \
	    AS_LOCK_HELD(sfmmup->sfmmu_as, &sfmmup->sfmmu_as->a_lock));


	ASSERT(sfmmup != NULL);
	ASSERT((len & MMU_PAGEOFFSET) == 0);
	ASSERT(!((u_long)addr & MMU_PAGEOFFSET));

	CPUSET_ZERO(cpuset);

	endaddr = addr + len;
	hblktag.htag_id = sfmmup;

	/*
	 * It is likely for the vm to call unload over a wide range of
	 * addresses that are actually very sparsely populated by
	 * translations.  In order to speed this up the sfmmu hat supports
	 * the concept of shadow hmeblks. Dummy large page hmeblks that
	 * correspond to actual small translations are allocated at tteload
	 * time and are referred to as shadow hmeblks.  Now, during unload
	 * time, we first check if we have a shadow hmeblk for that
	 * translation.  The absence of one means the corresponding address
	 * range is empty and can be skipped.
	 *
	 * The kernel is an exception to above statement and that is why
	 * we don't use shadow hmeblks and hash starting from the smallest
	 * page size.
	 */
	if (sfmmup == KHATID) {
		iskernel = 1;
		hashno = TTE64K;
	} else {
		iskernel = 0;
		hashno = TTE4M;
	}
	while (addr < endaddr) {
		hmeshift = HME_HASH_SHIFT(hashno);
		hblktag.htag_bspage = HME_HASH_BSPAGE(addr, hmeshift);
		hblktag.htag_rehash = hashno;
		hmebp = HME_HASH_FUNCTION(sfmmup, addr, hmeshift);

		SFMMU_HASH_LOCK(hmebp);

		HME_HASH_SEARCH_PREV(hmebp, hblktag, hmeblkp, hblkpa, pr_hblk,
			prevpa);
		if (hmeblkp == NULL) {
			/*
			 * didn't find an hmeblk. skip the appropiate
			 * address range.
			 */
			SFMMU_HASH_UNLOCK(hmebp);
			if (iskernel) {
				if (hashno < MAX_HASHCNT) {
					hashno++;
					continue;
				} else {
					hashno = TTE64K;
					addr = (caddr_t)roundup((u_int)addr
						+ 1, MMU_PAGESIZE64K);
					continue;
				}
			}
			addr = (caddr_t)roundup((u_int)addr + 1,
				(1 << hmeshift));
			if ((u_long)addr & MMU_PAGEOFFSET512K) {
				ASSERT(hashno == TTE64K);
				continue;
			}
			if ((u_long)addr & MMU_PAGEOFFSET4M) {
				hashno = TTE512K;
				continue;
			}
			hashno = TTE4M;
			continue;
		}
		ASSERT(hmeblkp);
		if (!hmeblkp->hblk_vcnt && !hmeblkp->hblk_hmecnt) {
			/*
			 * If the valid count is zero we can skip the range
			 * mapped by this hmeblk.
			 * We free hblks in the case of HAT_UNMAP.  HAT_UNMAP
			 * is used by segment drivers as a hint
			 * that the mapping resource won't be used any longer.
			 * The best example of this is during exit().
			 */
			addr = (caddr_t)roundup((u_int)addr + 1,
				get_hblk_span(hmeblkp));
			if (flags & HAT_UNLOAD_UNMAP) {
				sfmmu_hblk_hash_rm(hmebp, hmeblkp, prevpa,
				    pr_hblk);
				sfmmu_hblk_free(hmebp, hmeblkp, hblkpa);
			}
			SFMMU_HASH_UNLOCK(hmebp);
			if (iskernel) {
				hashno = TTE64K;
				continue;
			}
			if ((u_long)addr & MMU_PAGEOFFSET512K) {
				ASSERT(hashno == TTE64K);
				continue;
			}
			if ((u_long)addr & MMU_PAGEOFFSET4M) {
				hashno = TTE512K;
				continue;
			}
			hashno = TTE4M;
			continue;
		}
		if (hmeblkp->hblk_shw_bit) {
			/*
			 * If we encounter a shadow hmeblk we know there is
			 * smaller sized hmeblks mapping the same address space.
			 * Decrement the hash size and rehash.
			 */
			ASSERT(sfmmup != KHATID);
			hashno--;
			SFMMU_HASH_UNLOCK(hmebp);
			continue;
		}
		CPUSET_OR(cpuset, hmeblkp->hblk_cpuset);
		addr = sfmmu_hblk_unload(sfmmup, hmeblkp, addr, endaddr, flags);
		SFMMU_HASH_UNLOCK(hmebp);
		if (iskernel) {
			hashno = TTE64K;
			continue;
		}
		if ((u_long)addr & MMU_PAGEOFFSET512K) {
			ASSERT(hashno == TTE64K);
			continue;
		}
		if ((u_long)addr & MMU_PAGEOFFSET4M) {
			hashno = TTE512K;
			continue;
		}
		hashno = TTE4M;
	}
	if (sfmmup != ksfmmup) {
		cpuset = sfmmup->sfmmu_cpusran;
	}
	xt_sync(cpuset);
}

/*
 * This function unloads a range of addresses for an hmeblk.
 * It returns the next addres to be unloaded.
 * It should be called with the hash lock held.
 */
static caddr_t
sfmmu_hblk_unload(struct hat *sfmmup, struct hme_blk *hmeblkp, caddr_t addr,
	caddr_t endaddr, u_int flags)
{
	tte_t	tte, ttemod;
	struct	sf_hment *sfhmep;
	int	ttesz, cnt;
	struct	machpage *pp;
	kmutex_t *pml;
	int ret;

	ASSERT(in_hblk_range(hmeblkp, addr));
	ASSERT(!hmeblkp->hblk_shw_bit);
#ifdef DEBUG
	if (get_hblk_ttesz(hmeblkp) != TTE8K &&
	    (endaddr < get_hblk_endaddr(hmeblkp))) {
		cmn_err(CE_PANIC,
		    "sfmmu_hblk_unload: partial unload of large page\n");
	}
#endif DEBUG

	endaddr = MIN(endaddr, get_hblk_endaddr(hmeblkp));

	ttesz = get_hblk_ttesz(hmeblkp);
	cnt = -TTEPAGES(ttesz);
	sfhmep = sfmmu_hblktohme(hmeblkp, addr, NULL);

	while (addr < endaddr) {
		pml = NULL;
again:
		sfmmu_copytte(&sfhmep->hme_tte, &tte);
		if (TTE_IS_VALID(&tte)) {
			pp = sfhmep->hme_page;
			if (pp && pml == NULL) {
				pml = sfmmu_mlist_enter(pp);
			}

			/*
			 * Verify if hme still points to 'pp' now that
			 * we have p_mapping lock.
			 */
			if (sfhmep->hme_page != pp) {
				ASSERT((pp != NULL) && (sfhmep->hme_page
				    == NULL));
				goto tte_unloaded;
			}

			/*
			 * This point on we have both HASH and p_mapping
			 * lock.
			 */
			ASSERT(pp == sfhmep->hme_page);
			ASSERT(pp == NULL || sfmmu_mlist_held(pp));

			/*
			 * We need to loop on modify tte because it is
			 * possible for pagesync to come along and
			 * change the software bits beneath us.
			 *
			 * Page_unload can also invalidate the tte after
			 * we read tte outside of p_mapping lock.
			 */
			ttemod = tte;
			TTE_SET_INVALID(&ttemod);
			ret = sfmmu_modifytte_try(&tte, &ttemod,
			    &sfhmep->hme_tte);

			if (ret <= 0) {
				if (TTE_IS_VALID(&tte)) {
					goto again;
				} else {
					/*
					 * We read in a valid pte, but it
					 * is unloaded by page_unload.
					 * hme_page has become NULL and
					 * we hold no p_mapping lock.
					 */
					ASSERT(pp == NULL && pml == NULL);
					goto tte_unloaded;
				}
			}

			/*
			 * Ok- we invalidate the tte. Do the rest of the job.
			 */
			if (pp) {
				/*
				 * Remove the hment from the mapping list
				 */
				ASSERT(hmeblkp->hblk_hmecnt > 0);

				/*
				 * Again, we cannot
				 * ASSERT(hmeblkp->hblk_hmecnt <= NHMENTS);
				 */
				hme_sub(sfhmep, pp);
				atomic_add_hword(&hmeblkp->hblk_hmecnt, -1,
						NULL);
			}

			if (!(flags & HAT_UNLOAD_NOSYNC)) {
				sfmmu_ttesync(sfmmup, addr, &tte, pp);
			}

			atomic_add_hword(&hmeblkp->hblk_vcnt, -1, NULL);
			if (ttesz != TTE8K) {
				atomic_add_hword(&sfmmup->sfmmu_lttecnt, -1,
						NULL);
			}
			atomic_add_word((u_int *)&sfmmup->sfmmu_rss, cnt, NULL);

			if (flags & HAT_UNLOAD_UNLOCK) {
				atomic_add_hword(&hmeblkp->hblk_lckcnt, -1,
						NULL);
				ASSERT(hmeblkp->hblk_lckcnt >= 0);
			}

			/*
			 * Normally we would need to flush the page
			 * from the virtual cache at this point in
			 * order to prevent a potential cache alias
			 * inconsistency.
			 * The particular scenario we need to worry
			 * about is:
			 * Given:  va1 and va2 are two virtual address
			 * that alias and map the same physical
			 * address.
			 * 1.	mapping exists from va1 to pa and data
			 * has been read into the cache.
			 * 2.	unload va1.
			 * 3.	load va2 and modify data using va2.
			 * 4	unload va2.
			 * 5.	load va1 and reference data.  Unless we
			 * flush the data cache when we unload we will
			 * get stale data.
			 * Fortunately, page coloring eliminates the
			 * above scenario by remembering the color a
			 * physical page was last or is currently
			 * mapped to.  Now, we delay the flush until
			 * the loading of translations.  Only when the
			 * new translation is of a different color
			 * are we forced to flush.
			 */
			if (do_virtual_coloring) {
				sfmmu_tlb_demap(addr, sfmmup, hmeblkp,
				    sfmmup->sfmmu_free);
			} else {
				int pfnum;

				pfnum = TTE_TO_PFN(addr, &tte);
				sfmmu_tlbcache_demap(addr, sfmmup,
				    hmeblkp, pfnum, sfmmup->sfmmu_free,
				    FLUSH_NECESSARY_CPUS);
			}

			if (pp && PP_ISTNC(pp)) {
				/*
				 * if page was temporary
				 * unencached, try to recache
				 * it.
				 */
				sfmmu_page_cache(pp, HAT_CACHE, CACHE_FLUSH);
			}
		} else {
			if ((pp = sfhmep->hme_page) != NULL) {
				/*
				 * Tte is invalid but the hme
				 * still exists. let pageunload
				 * complete its job.
				 */
				ASSERT(pml == NULL);
				pml = sfmmu_mlist_enter(pp);
				/*
				 * Pageunload should be done now,
				 * fall through.
				 */
			}
		}

tte_unloaded:
		/*
		 * At this point, the tte we are looking at
		 * should be unloaded, and hme has been unlinked
		 * from page too. This is important because in
		 * pageunload, it does ttesync() then hme_sub.
		 * We need to make sure hme_sub has been completed
		 * so we know ttesync() has been completed. Otherwise,
		 * at exit time, after return from hat layer, VM will
		 * release as structure which hat_setstat() (called
		 * by ttesync()) needs.
		 */
#ifdef DEBUG
		{
			tte_t	dtte;

			ASSERT(sfhmep->hme_page == NULL);

			sfmmu_copytte(&sfhmep->hme_tte, &dtte);
			ASSERT(!TTE_IS_VALID(&dtte));
		}
#endif

		if (pml) {
			sfmmu_mlist_exit(pml);
		}

		SFMMU_STACK_TRACE(hmeblkp, tte.tte_hmenum)
		addr += TTEBYTES(ttesz);
		sfhmep++;
	}

	/*
	 * If pageunload is invalidating ptes at the same time,
	 * then the following test may fail (rarely) since
	 * we could be here before pageunload clears the counters.
	 * This would cause more xcalls, but no harm.
	 */
	if (!hmeblkp->hblk_vcnt && !hmeblkp->hblk_hmecnt) {
		CPUSET_ZERO(hmeblkp->hblk_cpuset);
	}
	return (addr);
}

/*
 * Synchronize all the mappings in the range [addr..addr+len).
 * Can be called with clearflag having two states:
 * HAT_SYNC_DONTZERO means just return the rm stats
 * HAT_SYNC_ZERORM means zero rm bits in the tte and return the stats
 */
void
hat_sync(struct hat *sfmmup, caddr_t addr, size_t len, u_int clearflag)
{
	struct hmehash_bucket *hmebp;
	hmeblk_tag hblktag;
	int hmeshift, hashno = 1;
	struct hme_blk *hmeblkp;
	caddr_t endaddr;
	cpuset_t cpuset;

	ASSERT((sfmmup == ksfmmup) ||
		AS_LOCK_HELD(sfmmup->sfmmu_as, &sfmmup->sfmmu_as->a_lock));
	ASSERT((len & MMU_PAGEOFFSET) == 0);
	ASSERT((clearflag == HAT_SYNC_DONTZERO) ||
		(clearflag == HAT_SYNC_ZERORM));

	CPUSET_ZERO(cpuset);

	endaddr = addr + len;
	hblktag.htag_id = sfmmup;
	/*
	 * Spitfire supports 4 page sizes.
	 * Most pages are expected to be of the smallest page
	 * size (8K) and these will not need to be rehashed. 64K
	 * pages also don't need to be rehashed because the an hmeblk
	 * spans 64K of address space. 512K pages might need 1 rehash and
	 * and 4M pages 2 rehashes.
	 */
	while (addr < endaddr) {
		hmeshift = HME_HASH_SHIFT(hashno);
		hblktag.htag_bspage = HME_HASH_BSPAGE(addr, hmeshift);
		hblktag.htag_rehash = hashno;
		hmebp = HME_HASH_FUNCTION(sfmmup, addr, hmeshift);

		SFMMU_HASH_LOCK(hmebp);

		HME_HASH_SEARCH(hmebp, hblktag, hmeblkp);
		if (hmeblkp != NULL) {
			CPUSET_OR(cpuset, hmeblkp->hblk_cpuset);
			addr = sfmmu_hblk_sync(sfmmup, hmeblkp, addr, endaddr,
			    clearflag);
			SFMMU_HASH_UNLOCK(hmebp);
			hashno = 1;
			continue;
		}
		SFMMU_HASH_UNLOCK(hmebp);

		if (!sfmmup->sfmmu_lttecnt || (hashno >= MAX_HASHCNT)) {
			/*
			 * We have traversed the whole list and rehashed
			 * if necessary without unloading so we assume it
			 * has already been unloaded
			 */
			addr += MMU_PAGESIZE;
		} else {
			hashno++;
		}
	}
	if (sfmmup != ksfmmup) {
		cpuset = sfmmup->sfmmu_cpusran;
	}
	xt_sync(cpuset);
}

static caddr_t
sfmmu_hblk_sync(struct hat *sfmmup, struct hme_blk *hmeblkp, caddr_t addr,
	caddr_t endaddr, int clearflag)
{
	tte_t	tte, ttemod;
	struct sf_hment *sfhmep;
	int ttesz;
	struct machpage *pp;
	kmutex_t *pml;
	int ret;

	endaddr = MIN(endaddr, get_hblk_endaddr(hmeblkp));

	ttesz = get_hblk_ttesz(hmeblkp);
	sfhmep = sfmmu_hblktohme(hmeblkp, addr, 0);

	while (addr < endaddr) {
		sfmmu_copytte(&sfhmep->hme_tte, &tte);
		if (TTE_IS_VALID(&tte)) {
			if (clearflag == HAT_SYNC_ZERORM) {
				ttemod = tte;
				TTE_CLR_RM(&ttemod);
				ret = sfmmu_modifytte_try(&tte, &ttemod,
				    &sfhmep->hme_tte);
				if (ret < 0) {
					continue;
				}

				if (ret > 0) {
					sfmmu_tlb_demap(addr, sfmmup,
						hmeblkp, 0);
				}
			}
			pp = sfhmep->hme_page;
			if (pp) {
				pml = sfmmu_mlist_enter(pp);
			}
			sfmmu_ttesync(sfmmup, addr, &tte, pp);
			if (pp) {
				sfmmu_mlist_exit(pml);
			}
		}
		addr += TTEBYTES(ttesz);
		sfhmep++;
	}
	return (addr);
}

/*
 * This function will sync a tte to the page struct and it will
 * update the hat stats. Currently it allows us to pass a NULL pp
 * and we will simply update the stats.  We may want to change this
 * so we only keep stats for pages backed by pp's.
 */
static void
sfmmu_ttesync(struct hat *sfmmup, caddr_t addr, tte_t *ttep, machpage_t *pp)
{
	u_int rm = 0;
	int   sz, npgs;

	ASSERT(TTE_IS_VALID(ttep));

	if (TTE_IS_NOSYNC(ttep)) {
		return;
	}

	if (TTE_IS_REF(ttep))  {
		rm = P_REF;
	}
	if (TTE_IS_MOD(ttep))  {
		rm |= P_MOD;
	}
	if (rm && sfmmup->sfmmu_rmstat) {
		hat_setstat(sfmmup->sfmmu_as, addr, MMU_PAGESIZE, rm);
	}
	/*
	 * XXX I want to use cas to update nrm bits but they
	 * currently belong in common/vm and not in hat where
	 * they should be.
	 * The nrm bits are protected by the same mutex as
	 * the one that protects the page's mapping list.
	 */
	if (!pp)
		return;
	/*
	 * If the tte is for a large page, we need to sync all the
	 * pages covered by the tte.
	 */
	sz = ttep->tte_size;
	if (sz != TTE8K) {
		ASSERT(pp->p_cons > 0);
		pp = PP_GROUPLEADER(pp, sz);
	}

	/* Get number of pages from tte size. */
	npgs = TTEPAGES(sz);

	do {
		ASSERT(pp);
		ASSERT(sfmmu_mlist_held(pp));
		sfmmu_page_enter(pp);
		if ((rm == P_REF) && !PP_ISREF(pp)) {
			PP_SETREF(pp);
		} else if ((rm == P_MOD) && !PP_ISMOD(pp)) {
			PP_SETMOD(pp);
		} else if ((rm == (P_REF | P_MOD)) &&
		    (!PP_ISREF(pp) || !PP_ISMOD(pp))) {
			PP_SETREFMOD(pp);
		}
		sfmmu_page_exit(pp);

		/*
		 * Are we done? If not, we must have a large mapping.
		 * For large mappings we need to sync the rest of the pages
		 * covered by this tte; goto the next page.
		 */
	} while (--npgs > 0 && (pp = PP_PAGENEXT(pp)));
}

/*
 * Remove all mappings to page 'pp'.
 * XXXmh support forceflag
 */
/* ARGSUSED */
int
hat_pageunload(struct page *gen_pp, u_int forceflag)
{
	struct machpage *pp = PP2MACHPP(gen_pp);
	struct sf_hment *sfhme;
	kmutex_t *pml;
	cpuset_t cpuset, tset;

	ASSERT(se_assert(&gen_pp->p_selock));

	CPUSET_ZERO(cpuset);

	pml = sfmmu_mlist_enter(pp);
	while ((sfhme = pp->p_mapping) != NULL) {
		tset = sfmmu_pageunload(pp, sfhme);
		CPUSET_OR(cpuset, tset);
	}
	xt_sync(cpuset);
	ASSERT(pp->p_mapping == NULL);

	if (PP_ISTNC(pp)) {
		sfmmu_page_enter(pp);
		PP_CLRTNC(pp);
		sfmmu_page_exit(pp);
	}

	sfmmu_mlist_exit(pml);
	return (0);
}

static cpuset_t
sfmmu_pageunload(machpage_t *pp, struct sf_hment *sfhme)
{
	struct hme_blk *hmeblkp;
	sfmmu_t *sfmmup;
	tte_t tte, ttemod;
#ifdef DEBUG
	tte_t orig_old;
#endif DEBUG
	caddr_t addr;
	int ttesz;
	int ret;
	cpuset_t cpuset;

	ASSERT(pp != NULL);
	ASSERT(sfmmu_mlist_held(pp));

	CPUSET_ZERO(cpuset);

	hmeblkp = sfmmu_hmetohblk(sfhme);

readtte:
	sfmmu_copytte(&sfhme->hme_tte, &tte);
	if (TTE_IS_VALID(&tte)) {
		sfmmup = hblktosfmmu(hmeblkp);
		ttesz = get_hblk_ttesz(hmeblkp);
		if (ttesz != TTE8K) {
			cmn_err(CE_PANIC, "sfmmu_pageunload - large page");
		}

		/*
		 * Note that we have p_mapping lock, but no hash lock here.
		 * hblk_unload() has to have both hash lock AND p_mapping
		 * lock before it tries to modify tte. So, the tte could
		 * not become invalid in the sfmmu_modifytte_try() below.
		 */
		ttemod = tte;
#ifdef DEBUG
		orig_old = tte;
#endif DEBUG
		TTE_SET_INVALID(&ttemod);
		ret = sfmmu_modifytte_try(&tte, &ttemod, &sfhme->hme_tte);
		if (ret < 0) {
#ifdef DEBUG
			/* only R/M bits can change. */
			chk_tte(&orig_old, &tte, &ttemod, hmeblkp);
#endif DEBUG
			goto readtte;
		}

		if (ret == 0) {
			cmn_err(CE_PANIC, "pageunload: cas failed?");
		}

		addr = tte_to_vaddr(hmeblkp, tte);

		SFMMU_STACK_TRACE(hmeblkp, tte.tte_hmenum)
		atomic_add_word((u_int *)&sfmmup->sfmmu_rss, -1, NULL);
		atomic_add_hword(&hmeblkp->hblk_vcnt, -1, NULL);
		/*
		 * since we don't support pageunload of a large page we
		 * don't need to atomically decrement lttecnt.
		 */
		sfmmu_ttesync(sfmmup, addr, &tte, pp);

		/*
		 * We need to flush the page from the virtual cache
		 * in order to prevent a virtual cache alias
		 * inconsistency. The particular scenario we need
		 * to worry about is:
		 * Given:  va1 and va2 are two virtual address that
		 * alias and will map the same physical address.
		 * 1.	mapping exists from va1 to pa and data has
		 *	been read into the cache.
		 * 2.	unload va1.
		 * 3.	load va2 and modify data using va2.
		 * 4	unload va2.
		 * 5.	load va1 and reference data.  Unless we flush
		 *	the data cache when we unload we will get
		 *	stale data.
		 * This scenario is taken care of by using virtual
		 * page coloring.
		 */
		if (do_virtual_coloring) {
			sfmmu_tlb_demap(addr, sfmmup, hmeblkp, 0);
		} else {
			sfmmu_tlbcache_demap(addr, sfmmup, hmeblkp,
				pp->p_pagenum, 0, FLUSH_NECESSARY_CPUS);
		}

		if (sfmmup == ksfmmup) {
			cpuset = hmeblkp->hblk_cpuset;
		} else {
			cpuset = sfmmup->sfmmu_cpusran;
		}

		/*
		 * Hme_sub has to run after ttesync() and a_rss update.
		 * See hblk_unload().
		 */
		hme_sub(sfhme, pp);
		membar_stst();

		/*
		 * We can not make ASSERT(hmeblkp->hblk_hmecnt <= NHMENTS)
		 * since pteload may have done a hme_add() right after
		 * we did the hme_sub() above. Hmecnt is now maintained
		 * by cas only. no lock guranteed its value. The only
		 * gurantee we have is the hmecnt should not be less than
		 * what it should be so the hblk will not be taken away.
		 *
		 * It's also important that we decremented the hmecnt after
		 * we are done with hmeblkp so that this hmeblk won't be
		 * stolen.
		 */
		ASSERT(hmeblkp->hblk_hmecnt > 0);
		atomic_add_hword(&hmeblkp->hblk_hmecnt, -1, NULL);
	} else {
		cmn_err(CE_PANIC, "invalid tte? pp = 0x%x, &tte = 0x%x\n",
			pp, &tte);
	}

	return (cpuset);
}

u_int	share_trigger = 8;

u_int
hat_pagesync(struct page *gen_pp, u_int clearflag)
{
	machpage_t *pp = PP2MACHPP(gen_pp);
	struct sf_hment *sfhme, *tmphme = NULL;
	kmutex_t *pml;
	cpuset_t cpuset, tset;
	int	index, cons;

	CPUSET_ZERO(cpuset);

	if (PP_ISRO(pp) && (clearflag & HAT_SYNC_STOPON_MOD)) {
		return (PP_GENERIC_ATTR(pp));
	}

	if ((clearflag == (HAT_SYNC_STOPON_REF | HAT_SYNC_DONTZERO)) &&
	    PP_ISREF(pp)) {
		return (PP_GENERIC_ATTR(pp));
	}

	if ((clearflag == (HAT_SYNC_STOPON_MOD | HAT_SYNC_DONTZERO)) &&
	    PP_ISMOD(pp)) {
		return (PP_GENERIC_ATTR(pp));
	}

	if ((pp->p_share > share_trigger) &&
	    !(clearflag & HAT_SYNC_ZERORM)) {
		if (PP_ISRO(pp)) {
			sfmmu_page_enter(pp);
			PP_SETREF(pp);
			sfmmu_page_exit(pp);
		}
		return (PP_GENERIC_ATTR(pp));
	}

	pml = sfmmu_mlist_enter(pp);
	index = PP_MAPINDEX(pp);
	cons = TTE8K;
retry:
	for (sfhme = pp->p_mapping; sfhme;
	    sfhme = tmphme) {
		/*
		 * We need to save the next hment on the list since
		 * it is possible for pagesync to remove an invalid hment
		 * from the list.
		 */
		tmphme = sfhme->hme_next;
		/*
		 * If we are looking for large mappings and this hme doesn't
		 * reach the range we are seeking, just ignore its.
		 */
		if (hme_size(sfhme) < cons)
			continue;
		tset = sfmmu_pagesync(pp, sfhme,
			clearflag & ~HAT_SYNC_STOPON_RM);
		CPUSET_OR(cpuset, tset);
		/*
		 * If clearflag is HAT_SYNC_DONTZERO, break out as soon
		 * as the "ref" or "mod" is set.
		 */
		if ((clearflag & ~HAT_SYNC_STOPON_RM) == HAT_SYNC_DONTZERO &&
		    ((clearflag & HAT_SYNC_STOPON_MOD) && PP_ISMOD(pp)) ||
		    ((clearflag & HAT_SYNC_STOPON_REF) && PP_ISREF(pp))) {
			index = 0;
			break;
		}
	}

	while (index) {
		index = index >> 1;
		cons++;
		if (index & 0x1) {
			/* Go to leading page */
			pp = PP_GROUPLEADER(pp, cons);
			goto retry;
		}
	}

	xt_sync(cpuset);
	sfmmu_mlist_exit(pml);
	return (PP_GENERIC_ATTR(pp));
}

/*
 * Get all the hardware dependent attributes for a page struct
 */
static cpuset_t
sfmmu_pagesync(struct machpage *pp, struct sf_hment *sfhme,
	u_int clearflag)
{
	caddr_t addr;
	tte_t tte, ttemod;
	struct hme_blk *hmeblkp;
	int ret;
	sfmmu_t *sfmmup;
	cpuset_t cpuset;

	ASSERT(pp != NULL);
	ASSERT(sfmmu_mlist_held(pp));
	ASSERT((clearflag == HAT_SYNC_DONTZERO) ||
		(clearflag == HAT_SYNC_ZERORM));

	SFMMU_STAT(sf_pagesync);

	CPUSET_ZERO(cpuset);

sfmmu_pagesync_retry:

	sfmmu_copytte(&sfhme->hme_tte, &tte);
	if (TTE_IS_VALID(&tte)) {
		hmeblkp = sfmmu_hmetohblk(sfhme);
		sfmmup = hblktosfmmu(hmeblkp);
		addr = tte_to_vaddr(hmeblkp, tte);
		if (clearflag == HAT_SYNC_ZERORM) {
			ttemod = tte;
			TTE_CLR_RM(&ttemod);
			ret = sfmmu_modifytte_try(&tte, &ttemod,
				&sfhme->hme_tte);
			if (ret < 0) {
				/*
				 * cas failed and the new value is not what
				 * we want.
				 */
				goto sfmmu_pagesync_retry;
			}

			if (ret > 0) {
				/* we win the cas */
				sfmmu_tlb_demap(addr, sfmmup, hmeblkp, 0);
			}
		}

		sfmmu_ttesync(sfmmup, addr, &tte, pp);
		if (sfmmup == ksfmmup) {
			cpuset = hmeblkp->hblk_cpuset;
		} else {
			cpuset = sfmmup->sfmmu_cpusran;
		}
	}
	return (cpuset);
}

void
hat_page_setattr(page_t *gen_pp, u_int flag)
{
	machpage_t *pp = PP2MACHPP(gen_pp);

	ASSERT(!(flag & ~(P_MOD | P_REF | P_RO)));

	if ((pp->p_nrm & flag) == flag) {
		/* attribute already set */
		return;
	}
	sfmmu_page_enter(pp);
	pp->p_nrm |= flag;
	sfmmu_page_exit(pp);
}
void
hat_page_clrattr(page_t *gen_pp, u_int flag)
{
	machpage_t *pp = PP2MACHPP(gen_pp);

	ASSERT(!(flag & ~(P_MOD | P_REF | P_RO)));

	sfmmu_page_enter(pp);
	pp->p_nrm &= ~flag;
	sfmmu_page_exit(pp);
}

u_int
hat_page_getattr(page_t *gen_pp, u_int flag)
{
	machpage_t *pp = PP2MACHPP(gen_pp);

	ASSERT(!(flag & ~(P_MOD | P_REF | P_RO)));
	return ((u_int)(pp->p_nrm & flag));
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
	return (sfmmu_vatopfn(addr, hat));
}

/*
 * For compatability with AT&T and later optimizations
 */
/* ARGSUSED */
void
hat_map(struct hat *hat, caddr_t addr, size_t len, u_int flags)
{
	ASSERT(hat != NULL);
}

/*
 * Return the number of mappings to a particular page.
 * This number is an approximation of the number of
 * number of people sharing the page.
 */
u_long
hat_page_getshare(page_t *gen_pp)
{
	int cnt, sz, index;
	machpage_t *pp = PP2MACHPP(gen_pp);

	if (!PP_ISMAPPED_LARGE(pp))
		return (pp->p_share);

	/*
	 * If we have a large mapping, we count the number of
	 * mappings that this large page is part.
	 */
	ASSERT(pp->p_cons > 0);
	sz = TTE8K;
	cnt = pp->p_share;
	index = PP_MAPINDEX(pp);
	while (index) {
		index >>= 1;
		sz++;
		if (index & 0x1) {
			pp = PP_GROUPLEADER(pp, sz);
			cnt += pp->p_share;
		}
	}
	return (cnt);
}

/*
 * Yield the memory claim requirement for an address space.
 *
 * This is currently implemented as the number of bytes that have active
 * hardware translations that have page structures.  Therefore, it can
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
		return ((size_t)ptob(hat->sfmmu_rss));
	else
		return (0);
}

int
hat_stats_enable(struct hat *hat)
{
	mutex_enter(&hat->sfmmu_mutex);
	hat->sfmmu_rmstat++;
	mutex_exit(&hat->sfmmu_mutex);
	return (1);
}

void
hat_stats_disable(struct hat *hat)
{
	mutex_enter(&hat->sfmmu_mutex);
	hat->sfmmu_rmstat--;
	mutex_exit(&hat->sfmmu_mutex);
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
int
hat_share(struct hat *sfmmup, caddr_t addr,
	struct hat *ism_hatid, caddr_t sptaddr, size_t size)
{
	struct ctx	*ctx;
	ism_map_blk_t	*blkp, *prevp;
	ism_map_t 	*map, m;
	u_short		sh_size = ISM_SHIFT(size);
	u_short		sh_vbase = ISM_SHIFT(addr);
	int		i;

	ASSERT(ism_hatid != NULL && sfmmup != NULL);
	ASSERT(sptaddr == ISMID_STARTADDR);
	/*
	 * Check the alignment.
	 */
	if (! ISM_ALIGNED(addr) || ! ISM_ALIGNED(sptaddr))
		return (EINVAL);

	/*
	 * Check size alignment.
	 */
	if (! ISM_ALIGNED(size))
		return (EINVAL);


	mutex_enter(&sfmmup->sfmmu_mutex);
	/*
	 * Make sure that during the time ism-mappings are setup, this
	 * process doesn't allow it's context to be stolen.
	 */
	sfmmu_disallow_ctx_steal(sfmmup);
	ctx	  = sfmmutoctx(sfmmup);

	/*
	 * Allocate an ism map blk if necessary.
	 * process doesn't allow it's context to be stolen.
	 */
	if (sfmmup->sfmmu_ismblk == NULL) {
		ASSERT(ctx->c_ismblkpa == (u_longlong_t)-1);
		sfmmup->sfmmu_ismblk = ism_map_blk_alloc();
		ctx->c_ismblkpa = va_to_pa((caddr_t)sfmmup->sfmmu_ismblk);
	}

	/*
	 * Make sure mapping does not already exist.
	 */
	blkp = sfmmup->sfmmu_ismblk;
	while (blkp) {
		map = blkp->maps;
		for (i = 0; i < ISM_MAP_SLOTS && map[i].ism_sfmmu; i++) {
			if ((map[i].ism_sfmmu == ism_hatid &&
			    sh_vbase == map[i].ism_vbase)) {
				cmn_err(CE_PANIC,
					"sfmmu_share: Already mapped!");
			}
		}
		blkp = blkp->next;
	}

	/*
	 * Add mapping to first available mapping slot.
	 *
	 * NOTE: We used 64 bit ld's/st's to prevent MT race
	 *	condition between here and both tsb miss handlers
	 *	and sfmmu_vatopfn/sfmmu_user_vatopfn().
	 */
	m.ism_map = (u_longlong_t)0;
	blkp = prevp = sfmmup->sfmmu_ismblk;
	while (blkp) {
		map = blkp->maps;
		for (i = 0; i < ISM_MAP_SLOTS; i++)  {
			if (map[i].ism_sfmmu == NULL) {
				m.ism_sfmmu = ism_hatid;
				m.ism_vbase = sh_vbase;
				m.ism_size  = sh_size;
				map[i].ism_map = m.ism_map;
				goto out;
			}
		}
		prevp = blkp;
		blkp = blkp->next;
	}

	/*
	 * We did not find an empty slot so we must add a
	 * new map blk and allocate the first mapping.
	 */
	blkp = prevp->next = ism_map_blk_alloc();
	map = blkp->maps;

	m.ism_sfmmu = ism_hatid;
	m.ism_vbase = sh_vbase;
	m.ism_size  = sh_size;
	map[0].ism_map = m.ism_map;

out:
	atomic_add_word((u_int *)&sfmmup->sfmmu_rss, ism_hatid->sfmmu_rss,
		NULL);

	atomic_add_hword((u_short *)&sfmmup->sfmmu_lttecnt,
			ism_hatid->sfmmu_lttecnt, NULL);

	/*
	 * XXX Since the tl=1 tsb miss handler will only
	 * BLINDLY hash for 4mb entries we need to set this.
	 * This is so we won't go to pagefault and blow up.
	 * FIX ME. XXX
	 */
	ctx->c_flags |= LTTES_FLAG;

	/*
	 * Now the ctx can be stolen.
	 */
	sfmmu_allow_ctx_steal(sfmmup);

	mutex_exit(&sfmmup->sfmmu_mutex);
	return (0);
}

/*
 * Invalidate top level mapping elements in as
 * starting from addr to (addr + size).
 */
void
hat_unshare(struct hat *sfmmup, caddr_t addr, u_int size)
{
	ism_map_t 	*map, m;
	ism_map_blk_t	*blkp;
	u_short		sh_size = ISM_SHIFT(size);
	u_short		sh_vbase = ISM_SHIFT(addr);
	int 		found, i, spt_rss = 0;
	int		spt_lttecnt = 0;
	struct hat	*ism_hatid;

	ASSERT(ISM_ALIGNED(addr));
	ASSERT(ISM_ALIGNED(size));
	ASSERT(sfmmup != NULL);
	ASSERT(sfmmup != ksfmmup);

	mutex_enter(&sfmmup->sfmmu_mutex);
	/*
	 * Make sure that during the time ism-mappings are setup, this
	 * process doesn't allow it's context to be stolen.
	 */
	sfmmu_disallow_ctx_steal(sfmmup);

	/*
	 * Remove the mapping.
	 *
	 * We can't have any holes in the ism map.
	 * The tsb miss code while searching the ism map will
	 * stop on an empty map slot.  So we must move
	 * everyone past the hole up 1 if any.
	 *
	 * Also empty ism map blks are not freed until the
	 * process exits. This is to prevent a MT race condition
	 * between sfmmu_unshare() and sfmmu_tsb_miss() and
	 * sfmmu_user_vatopfn(). Both the tsb miss handlers,
	 * sfmmu_vatopfn/sfmmu_user_vatopfn() and
	 * sfmmu_share()/sfmmu_unshare() access the mappings
	 * using 64 bit access to prevent MT race conditions.
	 */
	found = 0;
	blkp = sfmmup->sfmmu_ismblk;
	while (blkp) {
		map = blkp->maps;
		for (i = 0; i < ISM_MAP_SLOTS; i++) {
			m.ism_map = (u_longlong_t)0;
			if (!found && (sh_vbase == map[i].ism_vbase &&
					sh_size == map[i].ism_size)) {
				spt_rss = map[i].ism_sfmmu->sfmmu_rss;
				ASSERT(spt_rss);
				spt_lttecnt =
				    map[i].ism_sfmmu->sfmmu_lttecnt;
				ism_hatid = map[i].ism_sfmmu;
				found = 1;
			}
			if (found) {
				/*
				 * We delete the ism map by copying
				 * the next map over the current one.
				 * We will take the next one in the maps
				 * array or from the next ism_map_blk.
				 */
				if (map[i].ism_sfmmu &&
					i < (ISM_MAP_SLOTS - 1)) {

					m.ism_sfmmu = map[i + 1].ism_sfmmu;
					m.ism_vbase = map[i + 1].ism_vbase;
					m.ism_size  = map[i + 1].ism_size;
					map[i].ism_map = m.ism_map;
				} else {
					if (blkp->next) {
						ism_map_t *nmap;
						nmap = blkp->next->maps;

						m.ism_sfmmu = nmap[0].ism_sfmmu;
						m.ism_vbase = nmap[0].ism_vbase;
						m.ism_size  = nmap[0].ism_size;
						map[i].ism_map = m.ism_map;
						break;
					} else {
						map[i].ism_map =
							(u_longlong_t)0;
						break;
					}
				}
			}
		}
		blkp = blkp->next;
	}
	mutex_exit(&sfmmup->sfmmu_mutex);

	/*
	 * Now de-map the tsb and tlb's.
	 */
	if (found) {
		sfmmu_tte_unshare(ism_hatid, sfmmup, addr, (size_t)size);
		atomic_add_word((u_int *)&sfmmup->sfmmu_rss, -spt_rss, NULL);
		atomic_add_hword(&sfmmup->sfmmu_lttecnt, -spt_lttecnt, NULL);
	}
	sfmmu_allow_ctx_steal(sfmmup);
}

/*
 * This function will invalidate the tsb/tlb for <addr, len> in sfmmup
 * according to the mappings found in ism_hatid.
 */
void
sfmmu_tte_unshare(struct hat *ism_hatid, struct hat *sfmmup, caddr_t realaddr,
	size_t len)
{
	struct hmehash_bucket *hmebp;
	hmeblk_tag hblktag;
	int hmeshift, hashno;
	struct hme_blk *hmeblkp;
	caddr_t endaddr, hblkend, addr = ISMID_STARTADDR;
	int ttesz;

	/*
	 * We need to invalidate the tsb and the tlb for above address
	 * range.  In order to do this efficiently we traverse the dummy as
	 * hmeblks and only invalidate those entries required.  Remember
	 * that the start of the dummy as shm area is always 0.
	 */
	ASSERT(sfmmup != ksfmmup);
	ASSERT(ism_hatid != ksfmmup);

	endaddr = addr + len;
	hblktag.htag_id = ism_hatid;
	hashno = TTE4M;

	while (addr < endaddr) {
		hmeshift = HME_HASH_SHIFT(hashno);
		hblktag.htag_bspage = HME_HASH_BSPAGE(addr, hmeshift);
		hblktag.htag_rehash = hashno;
		hmebp = HME_HASH_FUNCTION(ism_hatid, addr, hmeshift);

		SFMMU_HASH_LOCK(hmebp);

		HME_HASH_SEARCH(hmebp, hblktag, hmeblkp);
		if (hmeblkp == NULL) {
			/*
			 * didn't find an hmeblk. skip the appropiate
			 * address range.
			 */
			SFMMU_HASH_UNLOCK(hmebp);
			addr = (caddr_t)roundup((u_int)addr + 1,
				(1 << hmeshift));
			if ((u_long)addr & MMU_PAGEOFFSET512K) {
				ASSERT(hashno == TTE64K);
				continue;
			}
			if ((u_long)addr & MMU_PAGEOFFSET4M) {
				hashno = TTE512K;
				continue;
			}
			hashno = TTE4M;
			continue;
		}
		ASSERT(hmeblkp);
		if (!hmeblkp->hblk_vcnt && !hmeblkp->hblk_hmecnt) {
			/*
			 * If the valid count is zero we can skip the range
			 * mapped by this hmeblk.
			 */
			addr = (caddr_t)roundup((u_int)addr + 1,
				get_hblk_span(hmeblkp));
			SFMMU_HASH_UNLOCK(hmebp);
			if ((u_long)addr & MMU_PAGEOFFSET512K) {
				ASSERT(hashno == TTE64K);
				continue;
			}
			if ((u_long)addr & MMU_PAGEOFFSET4M) {
				hashno = TTE512K;
				continue;
			}
			hashno = TTE4M;
			continue;
		}
		if (hmeblkp->hblk_shw_bit) {
			/*
			 * If we encounter a shadow hmeblk we know there is
			 * smaller sized hmeblks mapping the same address space.
			 * Decrement the hash size and rehash.
			 */
			hashno--;
			SFMMU_HASH_UNLOCK(hmebp);
			continue;
		}

		/*
		 * We now invalidate the tsb/tlb for all entries in this
		 * hmeblk
		 */
#ifdef DEBUG
		if (get_hblk_ttesz(hmeblkp) != TTE8K &&
		    (endaddr < get_hblk_endaddr(hmeblkp))) {
			cmn_err(CE_PANIC,
			    "sfmmu_tte_unshare: partial unload of large pg\n");
		}
#endif DEBUG

		hblkend = MIN(endaddr, get_hblk_endaddr(hmeblkp));
		ttesz = get_hblk_ttesz(hmeblkp);

		while (addr < hblkend) {
			sfmmu_tlb_demap(realaddr + (size_t)addr, sfmmup, NULL,
				sfmmup->sfmmu_free);
			addr += TTEBYTES(ttesz);
		}

		addr = hblkend;

		SFMMU_HASH_UNLOCK(hmebp);
		if ((u_long)addr & MMU_PAGEOFFSET512K) {
			ASSERT(hashno == TTE64K);
			continue;
		}
		if ((u_long)addr & MMU_PAGEOFFSET4M) {
			hashno = TTE512K;
			continue;
		}
		hashno = TTE4M;
	}
	xt_sync(sfmmup->sfmmu_cpusran);
}

static struct kmem_cache *ism_map_blk_cache;

/* ARGSUSED */
static int
ism_cache_constructor(void *buf, void *cdrarg, int kmflags)
{
	bzero(buf, sizeof (ism_map_blk_t));
	return (0);
}

/*
 * Allocate an ISM block mapping structure. This
 * routine will align the allocation to a E$ line
 * and make sure it does not cross a page boundary.
 */
static ism_map_blk_t *
ism_map_blk_alloc(void)
{
	ism_map_blk_t	*blkp, *blklist, *blkp1;
#ifdef DEBUG
	ism_map_t 	*map;
	int		i;
#endif /* DEBUG */

	/*
	 * Initialize cache if first time thru.
	 */
	if (ism_map_blk_cache == NULL) {
		mutex_enter(&ism_lock);
		if (ism_map_blk_cache == NULL) {
			ism_map_blk_cache =
				kmem_cache_create("ism_map_blk_cache",
					sizeof (ism_map_blk_t), ecache_linesize,
					ism_cache_constructor, NULL, NULL,
					NULL, NULL, 0);
		}
		mutex_exit(&ism_lock);
	}
	blkp = kmem_cache_alloc(ism_map_blk_cache, KM_SLEEP);

#ifdef DEBUG
	map = blkp->maps;
	for (i = 0; i < ISM_MAP_SLOTS; i++)
		ASSERT(map[i].ism_map == (u_longlong_t)0);
	ASSERT(blkp->next == NULL);
#endif /* DEBUG */


	/*
	 * Make sure map blk doesn't cross a page boundary.
	 */
	if (!ISM_CROSS_PAGE(blkp, sizeof (ism_map_blk_t))) {
		return (blkp);
	}
	blklist = blkp;

	/*
	 * Our first attempt failed. Keep trying until
	 * we get one that fits while holding previous allocs.
	 * Then after finding a good one free the list.
	 */
	while (ISM_CROSS_PAGE(blkp, sizeof (ism_map_blk_t))) {
		blkp = kmem_cache_alloc(ism_map_blk_cache, KM_SLEEP);
		if (!ISM_CROSS_PAGE(blkp, sizeof (ism_map_blk_t)))
			break;

		blkp->next = blklist;
		blklist = blkp;
	}

	/*
	 * Free the duds before returning the good one.
	 */
	while (blklist) {
		blkp1 = blklist;
		blklist = blklist->next;
		blkp1->next = NULL;
		kmem_cache_free(ism_map_blk_cache, blkp1);
	}
#ifdef DEBUG
	map = blkp->maps;
	for (i = 0; i < ISM_MAP_SLOTS; i++)
		ASSERT(map[i].ism_map == (u_longlong_t)0);
	ASSERT(blkp->next == NULL);
#endif /* DEBUG */
	return (blkp);
}

static void
ism_map_blk_free(ism_map_blk_t *blkp)
{
#ifdef DEBUG
	ism_map_t	*map = blkp->maps;
	int 		i;

	ASSERT(ism_map_blk_cache != NULL);

	for (i = 0; i < ISM_MAP_SLOTS; i++)
		ASSERT(map[i].ism_map == (u_longlong_t)0);
	ASSERT(blkp->next == NULL);
#endif /* DEBUG */

	kmem_cache_free(ism_map_blk_cache, blkp);
}

/* ARGSUSED */
static int
sfmmu_idcache_constructor(void *buf, void *cdrarg, int kmflags)
{
	sfmmu_t *sfmmup = (sfmmu_t *)buf;

	mutex_init(&sfmmup->sfmmu_mutex, "sfmmu_mutex", MUTEX_DEFAULT, NULL);
	return (0);
}

/* ARGSUSED */
static void
sfmmu_idcache_destructor(void *buf, void *cdrarg)
{
	sfmmu_t *sfmmup = (sfmmu_t *)buf;

	mutex_destroy(&sfmmup->sfmmu_mutex);
}

/*
 * setup kmem hmeblks by bzeroing all members and initializing the nextpa
 * field to be the pa of this hmeblk
 */
/* ARGSUSED */
static int
sfmmu_hblkcache_constructor(void *buf, void *cdrarg, int kmflags)
{
	struct hme_blk *hmeblkp;

	bzero(buf, (size_t)cdrarg);
	hmeblkp = (struct hme_blk *)buf;
	hmeblkp->hblk_nextpa = va_to_pa((caddr_t)hmeblkp);
	return (0);
}

#define	SFMMU_CACHE_RECLAIM_SCAN_RATIO 8
static int sfmmu_cache_reclaim_scan_ratio = SFMMU_CACHE_RECLAIM_SCAN_RATIO;
/*
 * The kmem allocator will callback into our reclaim routine when the system
 * is running low in memory.  We traverse the hash and free up all unused but
 * still cached hme_blks.  We also traverse the free list and free them up
 * as well.
 */
/*ARGSUSED*/
static void
sfmmu_hblkcache_reclaim(void *cdrarg)
{
	int i;
	u_longlong_t hblkpa, prevpa, nx_pa;
	struct hmehash_bucket *hmebp;
	struct hme_blk *hmeblkp, *nx_hblk, *pr_hblk = NULL;
	struct hme_blk *head = NULL;
	static struct hmehash_bucket *uhmehash_reclaim_hand;
	static struct hmehash_bucket *khmehash_reclaim_hand;
	int nhblks;
	extern struct hme_blk   *hblk1_flist;
	extern struct hme_blk   *hblk8_flist;
	extern int		hblk1_avail;
	extern int		hblk8_avail;

	hmebp = uhmehash_reclaim_hand;
	if (hmebp == NULL || hmebp > &uhme_hash[UHMEHASH_SZ])
		uhmehash_reclaim_hand = hmebp = uhme_hash;
	uhmehash_reclaim_hand += UHMEHASH_SZ / sfmmu_cache_reclaim_scan_ratio;

	for (i = UHMEHASH_SZ / sfmmu_cache_reclaim_scan_ratio; i; i--) {
		if (SFMMU_HASH_LOCK_TRYENTER(hmebp) != 0) {
			hmeblkp = hmebp->hmeblkp;
			hblkpa = hmebp->hmeh_nextpa;
			prevpa = 0;
			pr_hblk = NULL;
			while (hmeblkp) {
				nx_hblk = hmeblkp->hblk_next;
				nx_pa = hmeblkp->hblk_nextpa;
				if (!hmeblkp->hblk_vcnt &&
				    !hmeblkp->hblk_hmecnt) {
					sfmmu_hblk_hash_rm(hmebp, hmeblkp,
						prevpa, pr_hblk);
					sfmmu_hblk_free(hmebp, hmeblkp, hblkpa);
				} else {
					pr_hblk = hmeblkp;
					prevpa = hblkpa;
				}
				hmeblkp = nx_hblk;
				hblkpa = nx_pa;
			}
			SFMMU_HASH_UNLOCK(hmebp);
		}
		if (hmebp++ == &uhme_hash[UHMEHASH_SZ])
			hmebp = uhme_hash;
	}

	hmebp = khmehash_reclaim_hand;
	if (hmebp == NULL || hmebp > &khme_hash[KHMEHASH_SZ])
		khmehash_reclaim_hand = hmebp = khme_hash;
	khmehash_reclaim_hand += KHMEHASH_SZ / sfmmu_cache_reclaim_scan_ratio;

	for (i = KHMEHASH_SZ / sfmmu_cache_reclaim_scan_ratio; i; i--) {
		if (SFMMU_HASH_LOCK_TRYENTER(hmebp) != 0) {
			hmeblkp = hmebp->hmeblkp;
			hblkpa = hmebp->hmeh_nextpa;
			prevpa = 0;
			pr_hblk = NULL;
			while (hmeblkp) {
				nx_hblk = hmeblkp->hblk_next;
				nx_pa = hmeblkp->hblk_nextpa;
				if (!hmeblkp->hblk_vcnt &&
				    !hmeblkp->hblk_hmecnt) {
					sfmmu_hblk_hash_rm(hmebp, hmeblkp,
						prevpa, pr_hblk);
					sfmmu_hblk_free(hmebp, hmeblkp, hblkpa);
				} else {
					pr_hblk = hmeblkp;
					prevpa = hblkpa;
				}
				hmeblkp = nx_hblk;
				hblkpa = nx_pa;
			}
			SFMMU_HASH_UNLOCK(hmebp);
		}
		if (hmebp++ == &khme_hash[KHMEHASH_SZ])
			hmebp = khme_hash;
	}

	/*
	 * kmem free all dynamically allocated hme_blks beyond threshold
	 */
	if (hblk8_avail > HME8_TRHOLD &&
		hblk8_allocated > hblk8_prealloc_count) {
		HBLK8_FLIST_LOCK();
		/*
		 * subtract the hblks used in hash lists to find out how many
		 * we want to keep in the freelist. keep atleast HME8_TRHOLD
		 */
		nhblks = hblk8_prealloc_count - (hblk8_allocated - hblk8_avail);
		if (nhblks < HME8_TRHOLD)
			nhblks = HME8_TRHOLD;
		if (hblk8_avail > nhblks) {
			/*
			 * keep all the nucleus hmeblks, they are queued up
			 * at the front of the freelist. Also keep atleast
			 * nhblks number of hmeblks.
			 */
			for (i = 1, hmeblkp = hblk8_flist; (hmeblkp &&
			    hmeblkp->hblk_nuc_bit) || (i < nhblks);
				hmeblkp = hmeblkp->hblk_next, i++);
			if (hmeblkp) {
				head = hmeblkp->hblk_next;
				hmeblkp->hblk_next = NULL;
				hblk8_flist_t = hmeblkp;
				hblk8_allocated -= (hblk8_avail - i);
				hblk8_avail = i;
			}
		}
		HBLK8_FLIST_UNLOCK();
		while (head) {
			hmeblkp = head;
			head = head->hblk_next;
			kmem_cache_free(sfmmu8_cache, hmeblkp);
			SFMMU_STAT(sf_hblk8_dfree);
		}
	}

	/*
	 * follow the same method given above
	 */
	if (hblk1_avail > HME1_TRHOLD) {
		HBLK1_FLIST_LOCK();
		if (hblk1_avail > HME1_TRHOLD) {
			for (i = 1, hmeblkp = hblk1_flist; (hmeblkp &&
			    hmeblkp->hblk_nuc_bit) || (i < HME1_TRHOLD);
				hmeblkp = hmeblkp->hblk_next, i++);
			if (hmeblkp) {
				head = hmeblkp->hblk_next;
				hmeblkp->hblk_next = NULL;
				hblk1_flist_t = hmeblkp;
				hblk1_allocated -= (hblk1_avail - i);
				hblk1_avail = i;
			}
		}
		HBLK1_FLIST_UNLOCK();
		while (head) {
			hmeblkp = head;
			head = head->hblk_next;
			kmem_cache_free(sfmmu1_cache, hmeblkp);
			SFMMU_STAT(sf_hblk1_dfree);
		}
	}
#ifdef DEBUG
	sfmmu_check_hblk_flist();
#endif
}

/*
 * sfmmu_get_ppvcolor should become a vm_machdep or hatop interface.
 * same goes for sfmmu_get_addrvcolor().
 *
 * This function will return the virtual color for the specified page. The
 * virtual color corresponds to this page current mapping or its last mapping.
 * It is used by memory allocators to choose addresses with the correct
 * alignment so vac consistency is automatically maintained.  If the page
 * has no color it returns -1.
 */
int
sfmmu_get_ppvcolor(struct machpage *pp)
{
	int color;
	extern u_int shm_alignment;

	if (!(cache & CACHE_VAC) || PP_NEWPAGE(pp)) {
		return (-1);
	}
	color = PP_GET_VCOLOR(pp);
	ASSERT(color < mmu_btop(shm_alignment));
	return (color);
}

/*
 * This function will return the desired alignment for vac consistency
 * (vac color) given a virtual address.  If no vac is present it returns -1.
 */
int
sfmmu_get_addrvcolor(caddr_t vaddr)
{
	extern uint shm_alignment;
	extern int cache;

	if (cache & CACHE_VAC) {
		return (addr_to_vcolor(vaddr));
	} else {
		return (-1);
	}

}

/*
 * Check for conflicts.
 * A conflict exists if the new and existant mappings do not match in
 * their "shm_alignment fields. If conflicts exist, the existant mappings
 * are flushed unless one of them is locked. If one of them is locked, then
 * the mappings are flushed and converted to non-cacheable mappings.
 */
static void
sfmmu_vac_conflict(struct hat *hat, caddr_t addr, machpage_t *pp)
{
	struct hat *tmphat;
	struct sf_hment *sfhmep, *tmphme = NULL;
	struct hme_blk *hmeblkp;
	int vcolor;
	tte_t tte;

	ASSERT(sfmmu_mlist_held(pp));
	ASSERT(!PP_ISNC(pp));		/* page better be cacheable */

	vcolor = addr_to_vcolor(addr);
	if (PP_NEWPAGE(pp)) {
		PP_SET_VCOLOR(pp, vcolor);
		return;
	}

	if (PP_GET_VCOLOR(pp) == vcolor) {
		return;
	}

	if (!PP_ISMAPPED(pp)) {
		/*
		 * Previous user of page had a differnet color
		 * but since there are no current users
		 * we just flush the cache and change the color.
		 */
		SFMMU_STAT(sf_pgcolor_conflict);
		sfmmu_cache_flush(pp->p_pagenum, PP_GET_VCOLOR(pp));
		PP_SET_VCOLOR(pp, vcolor);
		return;
	}
	/*
	 * If we get here we have a vac conflict wit a current
	 * mapping.  VAC conflict policy is as follows.
	 * - The default is to unload the other mappings unless:
	 * - If we have a large mapping we uncache the page.
	 * We need to uncache the rest of the large page too.
	 * - If any of the mappings are locked we uncache the page.
	 * - If the requested mapping is inconsistent
	 * with another mapping and that mapping
	 * is in the same address space we have to
	 * make it non-cached.  The default thing
	 * to do is unload the inconsistent mapping
	 * but if they are in the same address space
	 * we run the risk of unmapping the pc or the
	 * stack which we will use as we return to the user,
	 * in which case we can then fault on the thing
	 * we just unloaded and get into an infinite loop.
	 */
	if (PP_ISMAPPED_LARGE(pp)) {
		int sz;
		int index;

		/* Find largest mapping this page is in */
		sz = 0;
		index = PP_MAPINDEX(pp);
		index = index >> 1; /* Don't care about 8K bit */
		for (; index; index >>= 1)
			sz++;

		/* Get number of pages largest mapping covers */
		index = CACHE_UNCACHE;
		pp = PP_GROUPLEADER(pp, sz);
		for (sz = TTEPAGES(sz); sz; sz--) {
			(void) sfmmu_vacconflict_array(addr, pp, &index);
			pp = PP_PAGENEXT(pp);
		}
		return;
	}

	/*
	 * check if any mapping is in same as or if it is locked
	 * since in that case we need to uncache.
	 */
	for (sfhmep = pp->p_mapping; sfhmep; sfhmep = tmphme) {
		tmphme = sfhmep->hme_next;
		hmeblkp = sfmmu_hmetohblk(sfhmep);
		tmphat = hblktosfmmu(hmeblkp);
		sfmmu_copytte(&sfhmep->hme_tte, &tte);
		ASSERT(TTE_IS_VALID(&tte));
		if ((tmphat == hat) || hmeblkp->hblk_lckcnt) {
			/*
			 * We have an uncache conflict
			 */
			SFMMU_STAT(sf_uncache_conflict);
			sfmmu_page_cache(pp, HAT_TMPNC, CACHE_FLUSH);
			return;
		}
	}
	/*
	 * We have an unload conflict
	 */
	SFMMU_STAT(sf_unload_conflict);
	while ((sfhmep = pp->p_mapping) != NULL) {
		(void) sfmmu_pageunload(pp, sfhmep);
	}
	ASSERT(pp->p_mapping == NULL);
	/*
	 * unloads only does tlb flushes so we need to flush the
	 * cache here.
	 */
	sfmmu_cache_flush(pp->p_pagenum, PP_GET_VCOLOR(pp));
	PP_SET_VCOLOR(pp, vcolor);
}

/*
 * This function changes the virtual cacheability of all mappings to a
 * particular page.  When changing from uncache to cacheable the mappings will
 * only be changed if all of them have the same virtual color.
 * We need to flush the cache in all cpus.  It is possible that
 * a process referenced a page as cacheable but has sinced exited
 * and cleared the mapping list.  We still to flush it but have no
 * state so all cpus is the only alternative.
 * RFE: In the HAT_CACHE case it would be nice if the routine could
 * somehow make sure we can recache the page before issuing all those
 * xcalls. See bug 1227522 for more info.
 */
static void
sfmmu_page_cache(machpage_t *pp, int flags, int cache_flush_flag)
{
	struct	sf_hment *sfhme, *tmphme = NULL;
	struct	hme_blk *hmeblkp;
	sfmmu_t *sfmmup;
	tte_t	tte, ttemod;
	caddr_t	vaddr;
	int	color = NO_VCOLOR;
	int	clr_valid = 0;
	int	ret;

	ASSERT(pp != NULL);
	ASSERT(sfmmu_mlist_held(pp));
	ASSERT(!(cache & CACHE_WRITEBACK));

	if ((flags == HAT_CACHE) && PP_ISPNC(pp)) {
		ASSERT(PP_GET_VCOLOR(pp) == NO_VCOLOR);
		return;
	}

	sfmmu_page_enter(pp);
	kpreempt_disable();

	/*
	 * We need to capture all cpus in order to change cacheability
	 * because we can't allow one cpu to access the same physical
	 * page using a cacheable and a non-cachebale mapping at the same
	 * time.
	 * A cache and tlb flush on all cpus who has referenced this page
	 * is necessary when going from cacheble to uncacheable.  A tlbflush
	 * is optional from going to uncacheable to cacheable and probably
	 * desireable because it allows the page to be cached sooner than
	 * waiting for the tlb entry to be invalidated randomly.
	 */
	xc_attention(cpu_ready_set);

redo:
	for (sfhme = pp->p_mapping; sfhme; sfhme = tmphme) {
		tmphme = sfhme->hme_next;
		hmeblkp = sfmmu_hmetohblk(sfhme);
		sfmmu_copytte(&sfhme->hme_tte, &tte);
		ASSERT(TTE_IS_VALID(&tte));
		vaddr = tte_to_vaddr(hmeblkp, tte);

		if (flags == HAT_CACHE) {
			if (!clr_valid) {
				color = addr_to_vcolor(vaddr);
				clr_valid = 1;
			} else if (color != addr_to_vcolor(vaddr)) {
				/*
				 * if we detect two mappings that disagree
				 * on the virtual color we abort the caching
				 * and redo all mappings to be uncached.
				 */
				flags = HAT_TMPNC;
				goto redo;
			}
		}


		ttemod = tte;
		if (flags & (HAT_UNCACHE | HAT_TMPNC)) {
			TTE_CLR_VCACHEABLE(&ttemod);
		} else {	/* flags & HAT_CACHE */
			TTE_SET_VCACHEABLE(&ttemod);
		}
		ret = sfmmu_modifytte_try(&tte, &ttemod, &sfhme->hme_tte);
		if (ret < 0) {
			/*
			 * Since all cpus are captured modifytte should not
			 * fail.
			 */
			cmn_err(CE_PANIC,
				"sfmmu_page_cache: write to tte failed");
		}

		sfmmup = hblktosfmmu(hmeblkp);
		if (cache_flush_flag == CACHE_FLUSH &&
		    (flags & (HAT_UNCACHE | HAT_TMPNC))) {
			/* flush tlb and caches */
			cache_flush_flag = CACHE_NO_FLUSH;
			sfmmu_tlbcache_demap(vaddr, sfmmup, hmeblkp,
				pp->p_pagenum, 0, FLUSH_ALL_CPUS);
		} else {	/* flags & HAT_CACHE */
			/* flush tlb only */
			sfmmu_tlb_demap(vaddr, sfmmup, hmeblkp, 0);
		}
		sfhme = sfhme->hme_next;
	}
	switch (flags) {

		default:
			cmn_err(CE_PANIC, "sfmmu_pagecache: unknown flags\n");
			break;

		case HAT_CACHE:
			PP_CLRTNC(pp);
			PP_CLRPNC(pp);
			PP_SET_VCOLOR(pp, color);
			break;

		case HAT_TMPNC:
			PP_SETTNC(pp);
			PP_SET_VCOLOR(pp, NO_VCOLOR);
			break;

		case HAT_UNCACHE:
			PP_SETPNC(pp);
			PP_CLRTNC(pp);
			PP_SET_VCOLOR(pp, NO_VCOLOR);
			break;
	}
	xt_sync(cpu_ready_set);
	xc_dismissed(cpu_ready_set);
	sfmmu_page_exit(pp);
	kpreempt_enable();
}

/*
 * This routine gets called when the system has run out of free contexts.
 * This will simply choose context passed to it to be stolen and reused.
*/
static void
sfmmu_reuse_ctx(struct ctx *ctx, sfmmu_t *sfmmup)
{
	sfmmu_t *stolen_sfmmup;
	cpuset_t cpuset;
	u_short	cnum = ctxtoctxnum(ctx);

	ASSERT(MUTEX_HELD(&ctx_lock));
	ASSERT(ctx->c_refcnt == HWORD_WLOCK);

	/*
	 * simply steal and reuse the ctx passed to us.
	 */
	stolen_sfmmup = ctx->c_sfmmu;
	ASSERT(stolen_sfmmup->sfmmu_cnum == cnum);

	TRACE_CTXS(ctx_trace_ptr, cnum, stolen_sfmmup, sfmmup, CTX_STEAL);
	SFMMU_STAT(sf_ctxsteal);

	/*
	 * Disable preemption. Capture other CPUS since TLB flush and
	 * TSB unload should be atomic. When we have a per-CPU TSB, we
	 * can do both atomically in one x-call and then we wouldn't need
	 * to do capture/release.
	 */
	kpreempt_disable();

	/*
	 * Update sfmmu and ctx structs. After this point all threads
	 * belonging to this hat/proc will fault and not use the ctx
	 * being stolen.
	 */
	stolen_sfmmup->sfmmu_cnum = INVALID_CONTEXT;
	ctx->c_sfmmu = NULL;
	membar_stld();

	cpuset = stolen_sfmmup->sfmmu_cpusran;
	CPUSET_DEL(cpuset, CPU->cpu_id);
	CPUSET_AND(cpuset, cpu_ready_set);
	xc_attention(cpuset);

	/*
	 * 1. flush TLB and TSB in all CPUs that ran the process whose ctx
	 * we are stealing.
	 * 2. change context for all other CPUs to INVALID_CONTEXT,
	 * if they are running in the context that we are going to steal.
	 */
	xt_some(cpuset, (u_int)sfmmu_ctx_steal_tl1, (u_int)cnum,
	    INVALID_CONTEXT, 0, 0);
	xt_sync(cpuset);

	/*
	 * flush TSB of local processor
	 */
	sfmmu_unload_tsbctx(cnum);

	/*
	 * flush tlb of local processor
	 */
	vtag_flushctx(cnum);

	xc_dismissed(cpuset);
	kpreempt_enable();

}

/*
 * Returns with context reader lock.
 * We maintain 2 different list of contexts.  The first list
 * is the free list and it is headed by ctxfree.  These contexts
 * are ready to use.  The second list is the dirty list and is
 * headed by ctxdirty. These contexts have been freed but haven't
 * been flushed from the tsb.
 */
static struct ctx *
sfmmu_get_ctx(sfmmu_t *sfmmup)
{
	struct ctx *ctx;
	u_short	cnum;
	struct ctx *lastctx = &ctxs[nctxs-1];
	struct ctx *firstctx = &ctxs[NUM_LOCKED_CTXS];
	u_int	found_stealable_ctx;
	u_int	retry_count = 0;

#define	NEXT_CTX(ctx)   (((ctx) >= lastctx) ? firstctx : ((ctx) + 1))

retry:
	mutex_enter(&ctx_lock);

	/*
	 * Check to see if this process has already got a ctx.
	 * In that case just set the sec-ctx, release ctx_lock and return.
	 */
	if (sfmmup->sfmmu_cnum >= NUM_LOCKED_CTXS) {
		ctx = sfmmutoctx(sfmmup);
		rwlock_hword_enter(&ctx->c_refcnt, READER_LOCK);
		mutex_exit(&ctx_lock);
		return (ctx);
	}

	found_stealable_ctx = 0;
	if ((ctx = ctxfree) != NULL) {
		/*
		 * Found a ctx in free list. Delete it from the list and
		 * use it.
		 */
		SFMMU_STAT(sf_ctxfree);
		ctxfree = ctx->c_free;
	} else if ((ctx = ctxdirty) != NULL) {
		/*
		 * No free contexts.  If we have at least one dirty ctx
		 * then flush tsb on all cpus and move dirty list to
		 * free list.
		 */
		SFMMU_STAT(sf_ctxdirty);
		kpreempt_disable();
		xc_all(sfmmu_unload_tsball, 0, 0);
		kpreempt_enable();
		ctxfree = ctx->c_free;
		ctxdirty = NULL;
	} else {
		/*
		 * no free context available, steal approp ctx.
		 * The policy to choose the aprop context is very simple.
		 * Just sweep all the ctxs using ctxhand. This will steal
		 * the LRU ctx.
		 *
		 * We however only steal a context whose c_refcnt rlock can
		 * be grabbed. Keep searching till we find a stealable ctx.
		 */
		ctx = ctxhand;
		do {
			/*
			 * If you get the writers lock, you can steal this
			 * ctx.
			 */
			if (rwlock_hword_enter(&ctx->c_refcnt, WRITER_LOCK)
				== 0) {
				found_stealable_ctx = 1;
				break;
			}
			ctx = NEXT_CTX(ctx);
		} while (ctx != ctxhand);

		if (found_stealable_ctx) {
			/*
			 * Try and reuse the ctx.
			 */
			sfmmu_reuse_ctx(ctx, sfmmup);

		} else if (retry_count++ < GET_CTX_RETRY_CNT) {
			mutex_exit(&ctx_lock);
			goto retry;

		} else {
			cmn_err(CE_PANIC, "Can't find any stealable context\n");
		}
	}

	ctx->c_sfmmu = sfmmup;		/* clears c_freep at the same time */
	ctx->c_flags = 0;
	cnum = ctxtoctxnum(ctx);
	sfmmup->sfmmu_cnum = cnum;

	/*
	 * If this sfmmu has an ism-map, setup the ctx struct.
	 */
	if (sfmmup->sfmmu_ismblk) {
		ctx->c_ismblkpa = va_to_pa((caddr_t)sfmmup->sfmmu_ismblk);
		/*
		 * XXX Since the tl=1 tsb miss handler will only
		 * BLINDLY hash for 4mb entries we need to set this.
		 * This is so we won't go to pagefault and blow up.
		 * FIX ME. XXX
		 */
		ctx->c_flags |= LTTES_FLAG;
	} else {
		ctx->c_ismblkpa = (u_longlong_t)-1;
	}

	/*
	 * Set up the c_flags field.
	 */
	if (sfmmup->sfmmu_lttecnt) {
		ctx->c_flags |= LTTES_FLAG;
	}

	/*
	 * If ctx stolen, release the writers lock.
	 */
	if (found_stealable_ctx)
		rwlock_hword_exit(&ctx->c_refcnt, WRITER_LOCK);

	/*
	 * Set the reader lock.
	 */
	rwlock_hword_enter(&ctx->c_refcnt, READER_LOCK);

	ctxhand = NEXT_CTX(ctx);

	ASSERT(sfmmup == sfmmutoctx(sfmmup)->c_sfmmu);
	mutex_exit(&ctx_lock);

	return (ctx);

#undef	NEXT_CTX
}

/*
 * Free up a ctx
 */
static void
sfmmu_free_ctx(sfmmu_t *sfmmup, struct ctx *ctx)
{
	int ctxnum;

	mutex_enter(&ctx_lock);

	TRACE_CTXS(ctx_trace_ptr, sfmmup->sfmmu_cnum, sfmmup,
	    0, CTX_FREE);

	if (sfmmup->sfmmu_cnum == INVALID_CONTEXT) {
		CPUSET_ZERO(sfmmup->sfmmu_cpusran);
		sfmmup->sfmmu_cnum = 0;
		mutex_exit(&ctx_lock);
		return;
	}

	ASSERT(sfmmup == ctx->c_sfmmu);

	ctx->c_ismblkpa = (u_longlong_t)-1;
	ctx->c_sfmmu = NULL;
	ctx->c_refcnt = 0;
	ctx->c_flags = 0;
	CPUSET_ZERO(sfmmup->sfmmu_cpusran);
	sfmmup->sfmmu_cnum = 0;
	ctxnum = sfmmu_getctx_sec();
	if (ctxnum == ctxtoctxnum(ctx)) {
		sfmmu_setctx_sec(INVALID_CONTEXT);
	}

	/*
	 * Put the freed ctx on the dirty list since tsb needs to be flushed.
	 */
	ctx->c_free = ctxdirty;
	ctxdirty = ctx;

	mutex_exit(&ctx_lock);
}

/*
 * Free up a sfmmu
 * Since the sfmmu is currently embedded in the hat struct we simply zero
 * out our fields and free up the ism map blk list if any.
 */
static void
sfmmu_free_sfmmu(sfmmu_t *sfmmup)
{
	ism_map_blk_t	*blkp, *nx_blkp;

	ASSERT(sfmmup->sfmmu_lttecnt == 0);
	sfmmup->sfmmu_cnum = 0;
	sfmmup->sfmmu_free = 0;

	blkp = sfmmup->sfmmu_ismblk;
	sfmmup->sfmmu_ismblk = NULL;
	while (blkp) {
		nx_blkp = blkp->next;
		blkp->next = NULL;
		ism_map_blk_free(blkp);
		blkp = nx_blkp;
	}
}

/*
 * Locking primitves accessed by HATLOCK macros
 */
static void
sfmmu_page_enter(struct machpage *pp)
{
	kmutex_t	*spl;

	ASSERT(pp != NULL);

	spl = SPL_HASH(pp);
	mutex_enter(spl);
}

static void
sfmmu_page_exit(struct machpage *pp)
{
	kmutex_t	*spl;

	ASSERT(pp != NULL);

	spl = SPL_HASH(pp);
	mutex_exit(spl);
}

/*
 * Sfmmu internal version of mlist enter/exit.
 */
static kmutex_t *
sfmmu_mlist_enter(struct machpage *pp)
{
	kmutex_t	*mml;

	ASSERT(pp != NULL);

	/* The lock lives in the root page */
	pp = PP_PAGEROOT(pp);
	ASSERT(pp != NULL);

	mml = MLIST_HASH(pp);
	mutex_enter(mml);

	return (mml);
}

static void
sfmmu_mlist_exit(kmutex_t *mml)
{
	mutex_exit(mml);
}


static int
sfmmu_mlist_held(struct machpage *pp)
{
	kmutex_t	*mml;

	ASSERT(pp != NULL);
	/* The lock lives in the root page */
	pp = PP_PAGEROOT(pp);
	ASSERT(pp != NULL);

	mml = MLIST_HASH(pp);
	return (MUTEX_HELD(mml));
}


/*
 * return a free hmeblk with 8 hments or with 1 hment depending on size.
 * Usually we take from freelist. if we are running low on hmeblks we
 * dynamically allocate more.  We finally call hmeblk stealer if none
 * available. Note that using nucleus hmeblks (placed in the front of
 * of the freelist) reduces the number of tlb misses while in the hat.
 * Of course, an even better rfe would be to modify kmem_alloc so it
 * understands nucleus memory and tries to allocate from it first.
 * This way the kernel tlb miss rate would drop further.
 */
static struct hme_blk *
sfmmu_hblk_alloc(sfmmu_t *sfmmup, caddr_t vaddr,
	struct hmehash_bucket *hmebp, int size, hmeblk_tag hblktag)
{
	struct hme_blk *hmeblkp = NULL;
	struct hme_blk *newhblkp;
	struct hme_blk *shw_hblkp = NULL;
	int hmelock_held = 1;
	u_longlong_t hblkpa;
	extern int tba_taken_over;

	ASSERT(SFMMU_HASH_LOCK_ISHELD(hmebp));

	if ((sfmmup != KHATID) && (size < TTE4M)) {
		SFMMU_HASH_UNLOCK(hmebp);
		hmelock_held = 0;
		shw_hblkp = sfmmu_shadow_hcreate(sfmmup, vaddr, size);
	}

	if (size == TTE8K) {
		/*
		 * Allocate more hmeblks if we are running low.
		 * check if the address is in kernel allocatable memory range
		 * so that we don't try to call kmem_alloc while we are called
		 * from kmem_alloc.
		 */
		if ((hblk8_avail <= HME8_TRHOLD) &&
		    (sfmmup != KHATID || hblkalloc_inprog == 0)) {
			if (hmelock_held) {
				SFMMU_HASH_UNLOCK(hmebp);
				hmelock_held = 0;
			}
			hmeblkp = sfmmu_hblk_grow(size,
			    (sfmmup == KHATID) ? KM_NOSLEEP : KM_SLEEP);
		}
		if (hmeblkp == NULL) {
			HBLK8_FLIST_LOCK();
			if (hblk8_avail) {
				hmeblkp = hblk8_flist;
				hblk8_flist = hmeblkp->hblk_next;
				if (--hblk8_avail == 0) {
					hblk8_flist_t = NULL;
				}
				HBLK_DEBUG_COUNTER_INCR(hblk8_inuse, 1);
			}
			HBLK8_FLIST_UNLOCK();
		}
	} else {
		if ((hblk1_avail <= HME1_TRHOLD) &&
		    (sfmmup != KHATID || hblkalloc_inprog == 0)) {
			if (hmelock_held) {
				SFMMU_HASH_UNLOCK(hmebp);
				hmelock_held = 0;
			}
			hmeblkp = sfmmu_hblk_grow(size,
			    (sfmmup == KHATID) ? KM_NOSLEEP : KM_SLEEP);
		}
		if (hmeblkp == NULL) {
			HBLK1_FLIST_LOCK();
			if (hblk1_avail) {
				hmeblkp = hblk1_flist;
				hblk1_flist = hmeblkp->hblk_next;
				if (--hblk1_avail == 0) {
					hblk1_flist_t = NULL;
				}
				HBLK_DEBUG_COUNTER_INCR(hblk1_inuse, 1);
			}
			HBLK1_FLIST_UNLOCK();
		}
	}

	if (hmeblkp == NULL) {
		/*
		 * Could not find a hmeblk in free list, and probably we
		 * could not allocate more. Call hmeblk stealer to get one.
		 */
		if (hmelock_held) {
			SFMMU_HASH_UNLOCK(hmebp);
			hmelock_held = 0;
		}

		hmeblkp = sfmmu_hblk_steal(size);
	}

	/*
	 * make sure hmeblk doesn't cross a page boundary
	 */
	ASSERT(hmeblkp != NULL);
	ASSERT(hmeblkp->hblk_nuc_bit || ((((uint)hmeblkp & MMU_PAGEOFFSET) +
		(size == TTE8K ? HME8BLK_SZ : HME1BLK_SZ)) <= MMU_PAGESIZE));

	set_hblk_sz(hmeblkp, size);

	if (!hmelock_held) {
		/*
		 * can only do this assert if hash lock is not held because
		 * we could deadlock otherwise.
		 */
		ASSERT(hmeblkp->hblk_nextpa == va_to_pa((caddr_t)hmeblkp));
		SFMMU_HASH_LOCK(hmebp);
		HME_HASH_FAST_SEARCH(hmebp, hblktag, newhblkp);
		if (newhblkp != NULL) {
			sfmmu_hblk_tofreelist(hmeblkp, hmeblkp->hblk_nextpa);
			return (newhblkp);
		}
	}

	ASSERT(SFMMU_HASH_LOCK_ISHELD(hmebp));
	hmeblkp->hblk_next = (struct hme_blk *)NULL;
	hmeblkp->hblk_tag = hblktag;
	hmeblkp->hblk_shadow = shw_hblkp;
	hblkpa = hmeblkp->hblk_nextpa;
	hmeblkp->hblk_nextpa = 0;
	CPUSET_ZERO(hmeblkp->hblk_cpuset);
	if (!tba_taken_over) {
		CPUSET_ADD(hmeblkp->hblk_cpuset, CPU->cpu_id);
	}
	ASSERT(get_hblk_ttesz(hmeblkp) == size);
	ASSERT(get_hblk_span(hmeblkp) == HMEBLK_SPAN(size));
	ASSERT(hmeblkp->hblk_hmecnt == 0);
	ASSERT(hmeblkp->hblk_vcnt == 0);
	ASSERT(hmeblkp->hblk_lckcnt == 0);
	ASSERT(hblkpa == va_to_pa((caddr_t)hmeblkp));
	sfmmu_hblk_hash_add(hmebp, hmeblkp, hblkpa);
	return (hmeblkp);
}

/*
 * This function performs any cleanup required on the hme_blk
 * and returns it to the free list.
 */
static void
sfmmu_hblk_free(struct hmehash_bucket *hmebp, struct hme_blk *hmeblkp,
	u_longlong_t hblkpa)
{
	int shw_size, vshift;
	struct hme_blk *shw_hblkp;
	uint	shw_mask, newshw_mask, vaddr;
	extern int cas();

	ASSERT(hmeblkp);
	ASSERT(!hmeblkp->hblk_hmecnt);
	ASSERT(!hmeblkp->hblk_vcnt);
	ASSERT(!hmeblkp->hblk_lckcnt);
	ASSERT(SFMMU_HASH_LOCK_ISHELD(hmebp));
	ASSERT(hblkpa == va_to_pa((caddr_t)hmeblkp));

	shw_hblkp = hmeblkp->hblk_shadow;
	if (shw_hblkp) {
		ASSERT(hblktosfmmu(hmeblkp) != KHATID);
		ASSERT(get_hblk_ttesz(hmeblkp) < TTE4M);

		shw_size = get_hblk_ttesz(shw_hblkp);
		vaddr = get_hblk_base(hmeblkp);
		vshift = vaddr_to_vshift(shw_hblkp->hblk_tag, vaddr, shw_size);
		ASSERT(vshift < 8);
		/*
		 * Atomically clear shadow mask bit
		 */
		do {
			shw_mask = shw_hblkp->hblk_shw_mask;
			ASSERT(shw_mask & (1 << vshift));
			newshw_mask = shw_mask & ~(1 << vshift);
			newshw_mask = cas(&shw_hblkp->hblk_shw_mask,
				shw_mask, newshw_mask);
		} while (newshw_mask != shw_mask);
		hmeblkp->hblk_shadow = NULL;
	}
	sfmmu_hblk_tofreelist(hmeblkp, hblkpa);
}

/*
 * This function puts an hmeblk back in the freelist, either in hblk8_flist
 * or hblk1_flist based on size. Note that the nucleus hmeblks go to the
 * front of the list and the dynamically allocated ones go to the rear,
 * the nucleus hmeblks are reused frequently (for better tlb hit rate).
 * Also note that we keep the hmeblk pa in the nextpa field in order
 * to save calls to vatopa.
 */
static void
sfmmu_hblk_tofreelist(struct hme_blk *hmeblkp, u_longlong_t hblkpa)
{
	extern struct hme_blk   *hblk1_flist;
	extern struct hme_blk   *hblk8_flist;
	extern int		hblk1_avail;
	extern int		hblk8_avail;

	ASSERT(hmeblkp);
	ASSERT(!hmeblkp->hblk_hmecnt);
	ASSERT(!hmeblkp->hblk_vcnt);
	ASSERT(hmeblkp->hblk_shadow == NULL);

	hmeblkp->hblk_next = NULL;
	hmeblkp->hblk_nextpa = hblkpa;	/* set nextpa field to this hblk pa */
	hmeblkp->hblk_shw_bit = 0;
	CPUSET_ZERO(hmeblkp->hblk_cpuset);

	if (get_hblk_ttesz(hmeblkp) == TTE8K) {
		ASSERT(hmeblkp->hblk_hme[1].hme_page == NULL);
		ASSERT(hmeblkp->hblk_hme[1].hme_next == NULL);
		ASSERT(!hmeblkp->hblk_hme[1].hme_tte.tte_inthi ||
			hmeblkp->hblk_hme[1].hme_tte.tte_hmenum == 1);

		HBLK8_FLIST_LOCK();
		if (hblk8_avail++ == 0) {
			hblk8_flist = hblk8_flist_t = hmeblkp;
		} else {
			if (hmeblkp->hblk_nuc_bit) {
				hmeblkp->hblk_next = hblk8_flist;
				hblk8_flist = hmeblkp;
			} else {
				hblk8_flist_t->hblk_next = hmeblkp;
				hblk8_flist_t = hmeblkp;
			}
		}
		HBLK_DEBUG_COUNTER_DECR(hblk8_inuse, 1);
		HBLK8_FLIST_UNLOCK();
	} else {
		HBLK1_FLIST_LOCK();
		if (hblk1_avail++ == 0) {
			hblk1_flist = hblk1_flist_t = hmeblkp;
		} else {
			if (hmeblkp->hblk_nuc_bit) {
				hmeblkp->hblk_next = hblk1_flist;
				hblk1_flist = hmeblkp;
			} else {
				hblk1_flist_t->hblk_next = hmeblkp;
				hblk1_flist_t = hmeblkp;
			}
		}
		HBLK_DEBUG_COUNTER_DECR(hblk1_inuse, 1);
		HBLK1_FLIST_UNLOCK();
	}
}

/*
 * dynamically allocate hmeblks. Allocate the first one (probably
 * with KM_SLEEP flag if user allocation), and then the rest and
 * stash them in the free list.
 */
static struct hme_blk *
sfmmu_hblk_grow(int size, int sleep)
{
	struct hme_blk *hmeblkp, *first, *head;
	int i;

	extern struct hme_blk *hblk1_flist;
	extern struct hme_blk *hblk8_flist;
	extern int hblk1_avail;
	extern int hblk8_avail;

	/*
	 * automically record that hmeblk allocation is in progress.
	 */
	atomic_add_word(&hblkalloc_inprog, 1, NULL);

	if (size == TTE8K) {
		if ((first = kmem_cache_alloc(sfmmu8_cache, sleep)) != NULL) {
			SFMMU_STAT(sf_hblk8_dalloc);
			hmeblkp = first;
			for (i = 0; i < HBLK_GROW_NUM &&
			    hblk8_avail <= HME8_TRHOLD; i++) {
				if ((hmeblkp->hblk_next = kmem_cache_alloc(
				    sfmmu8_cache, KM_NOSLEEP)) == NULL) {
					break;
				}
				SFMMU_STAT(sf_hblk8_dalloc);
				hmeblkp = hmeblkp->hblk_next;
			}

			HBLK8_FLIST_LOCK();
			if (i) {
				head = first->hblk_next;
				hmeblkp->hblk_next = NULL;
				if (hblk8_avail == 0) {
					hblk8_flist = head;
				} else {
					hblk8_flist_t->hblk_next = head;
				}
				hblk8_flist_t = hmeblkp;
				hblk8_avail += i;
			}
			/*
			 * we have allocated 'i+1' hmeblks, 'i' hmeblks added
			 * in freelist, one (first) will be used in hash list
			 */
			hblk8_allocated += (i + 1);
			HBLK_DEBUG_COUNTER_INCR(hblk8_inuse, 1);
			HBLK8_FLIST_UNLOCK();
			first->hblk_next = NULL;
		}
	} else {
		if ((first = kmem_cache_alloc(sfmmu1_cache, sleep)) != NULL) {
			SFMMU_STAT(sf_hblk1_dalloc);
			hmeblkp = first;
			for (i = 0; i < HBLK_GROW_NUM &&
			    hblk1_avail <= HME1_TRHOLD; i++) {
				if ((hmeblkp->hblk_next = kmem_cache_alloc(
				    sfmmu1_cache, KM_NOSLEEP)) == NULL) {
					break;
				}
				SFMMU_STAT(sf_hblk1_dalloc);
				hmeblkp = hmeblkp->hblk_next;
			}

			HBLK1_FLIST_LOCK();
			if (i) {
				head = first->hblk_next;
				hmeblkp->hblk_next = NULL;
				if (hblk1_avail == 0) {
					hblk1_flist = head;
				} else {
					hblk1_flist_t->hblk_next = head;
				}
				hblk1_flist_t = hmeblkp;
				hblk1_avail += i;
			}
			hblk1_allocated += (i + 1);
			HBLK_DEBUG_COUNTER_INCR(hblk1_inuse, 1);
			HBLK1_FLIST_UNLOCK();
			first->hblk_next = NULL;
		}
	}

	atomic_add_word(&hblkalloc_inprog, -1, NULL);

	return (first);
}

#define	BUCKETS_TO_SEARCH_BEFORE_UNLOAD	30

static u_int sfmmu_hblk_steal_twice;
static u_int sfmmu_hblk_steal_count, sfmmu_hblk_steal_unload_count;

/*
 * Steal a hmeblk
 * Enough hmeblks were allocated at startup (nucleus hmeblks) and also
 * hmeblks were added dynamically. We should never ever not be able to
 * find one. Look for an unused/unlocked hmeblk in user hash table.
 */
static struct hme_blk *
sfmmu_hblk_steal(int size)
{
	static struct hmehash_bucket *uhmehash_steal_hand = NULL;
	struct hmehash_bucket *hmebp;
	struct hme_blk *hmeblkp = NULL, *pr_hblk;
	u_longlong_t hblkpa, prevpa;
	int i;

	for (;;) {
		hmebp = (uhmehash_steal_hand == NULL) ? uhme_hash :
			uhmehash_steal_hand;
		ASSERT(hmebp >= uhme_hash && hmebp <= &uhme_hash[UHMEHASH_SZ]);

		for (i = 0; hmeblkp == NULL && i <= UHMEHASH_SZ +
		    BUCKETS_TO_SEARCH_BEFORE_UNLOAD; i++) {
			SFMMU_HASH_LOCK(hmebp);
			hmeblkp = hmebp->hmeblkp;
			hblkpa = hmebp->hmeh_nextpa;
			prevpa = 0;
			pr_hblk = NULL;
			while (hmeblkp) {
				/*
				 * check if it is a hmeblk that is not locked
				 * and not shared. skip shadow hmeblks with
				 * shadow_mask set i.e valid count non zero.
				 */
				if ((get_hblk_ttesz(hmeblkp) == size) &&
				    (hmeblkp->hblk_shw_bit == 0 ||
					hmeblkp->hblk_vcnt == 0) &&
				    (hmeblkp->hblk_lckcnt == 0) &&
				    !(hblktosfmmu(hmeblkp)->sfmmu_ismblk)) {
					/*
					 * there is a high probability that we
					 * will find a free one. search some
					 * buckets for a free hmeblk initially
					 * before unloading a valid hmeblk.
					 */
					if ((hmeblkp->hblk_vcnt == 0 &&
					    hmeblkp->hblk_hmecnt == 0) || (i >=
					    BUCKETS_TO_SEARCH_BEFORE_UNLOAD)) {
						if (sfmmu_steal_this_hblk(hmebp,
						    hmeblkp, hblkpa, prevpa,
						    pr_hblk)) {
							/*
							 * Hblk is unloaded
							 * successfully
							 */
							break;
						}
					}
				}
				pr_hblk = hmeblkp;
				prevpa = hblkpa;
				hblkpa = hmeblkp->hblk_nextpa;
				hmeblkp = hmeblkp->hblk_next;
			}

			SFMMU_HASH_UNLOCK(hmebp);
			if (hmebp++ == &uhme_hash[UHMEHASH_SZ])
				hmebp = uhme_hash;
		}
		uhmehash_steal_hand = hmebp;

		if (hmeblkp != NULL)
			break;

		/*
		 * we could not steal a hmeblk from user hash, check the
		 * freelist in case some hmeblk showed up there meanwhile.
		 */
		if (size == TTE8K) {
			HBLK8_FLIST_LOCK();
			if (hblk8_avail) {
				hmeblkp = hblk8_flist;
				hblk8_flist = hmeblkp->hblk_next;
				if (--hblk8_avail == 0) {
					hblk8_flist_t = NULL;
				}
				HBLK_DEBUG_COUNTER_INCR(hblk8_inuse, 1);
			}
			HBLK8_FLIST_UNLOCK();
		} else {
			HBLK1_FLIST_LOCK();
			if (hblk1_avail) {
				hmeblkp = hblk1_flist;
				hblk1_flist = hmeblkp->hblk_next;
				if (--hblk1_avail == 0) {
					hblk1_flist_t = NULL;
				}
				HBLK_DEBUG_COUNTER_INCR(hblk1_inuse, 1);
			}
			HBLK1_FLIST_UNLOCK();
		}

		if (hmeblkp != NULL)
			break;

		/*
		 * in the worst case, look for a free one in the kernel
		 * hash table.
		 */
		for (i = 0, hmebp = khme_hash; i <= KHMEHASH_SZ; i++) {
			SFMMU_HASH_LOCK(hmebp);
			hmeblkp = hmebp->hmeblkp;
			hblkpa = hmebp->hmeh_nextpa;
			prevpa = 0;
			pr_hblk = NULL;
			while (hmeblkp) {
				/*
				 * check if it is free hmeblk
				 */
				if ((get_hblk_ttesz(hmeblkp) == size) &&
				    (hmeblkp->hblk_lckcnt == 0) &&
				    (hmeblkp->hblk_vcnt == 0) &&
				    (hmeblkp->hblk_hmecnt == 0)) {
					if (sfmmu_steal_this_hblk(hmebp,
					    hmeblkp, hblkpa, prevpa, pr_hblk)) {
						break;
					} else {
						/*
						 * Cannot fail since we have
						 * hash lock.
						 */
						cmn_err(CE_PANIC,
						    "fail to steal?");
					}
				}

				pr_hblk = hmeblkp;
				prevpa = hblkpa;
				hblkpa = hmeblkp->hblk_nextpa;
				hmeblkp = hmeblkp->hblk_next;
			}

			SFMMU_HASH_UNLOCK(hmebp);
			if (hmebp++ == &khme_hash[KHMEHASH_SZ])
				hmebp = khme_hash;
		}

		if (hmeblkp != NULL)
			break;
		sfmmu_hblk_steal_twice++;
	}
	return (hmeblkp);
}

/*
 * This routine does real work to prepare a hblk to be "stolen" by
 * unloading the mappings, updating shadow counts ....
 * It returns 1 if the block is ready to be reused (stolen), or 0
 * means the block cannot be stolen yet- pageunload is still working
 * on this hblk.
 */
static int
sfmmu_steal_this_hblk(struct hmehash_bucket *hmebp, struct hme_blk *hmeblkp,
	u_longlong_t hblkpa, u_longlong_t prevpa, struct hme_blk *pr_hblk)
{
	int shw_size, vshift;
	struct hme_blk *shw_hblkp;
	uint	shw_mask, newshw_mask, vaddr;

	ASSERT(SFMMU_HASH_LOCK_ISHELD(hmebp));

	/*
	 * check if the hmeblk is free, unload if necessary
	 */
	if (hmeblkp->hblk_vcnt || hmeblkp->hblk_hmecnt) {
		sfmmu_hblk_unload(hblktosfmmu(hmeblkp), hmeblkp,
		    (caddr_t)get_hblk_base(hmeblkp),
			get_hblk_endaddr(hmeblkp), HAT_UNLOAD);

		if (hmeblkp->hblk_vcnt || hmeblkp->hblk_hmecnt) {
			/*
			 * Pageunload is working on the same hblk.
			 */
			return (0);
		}

		sfmmu_hblk_steal_unload_count++;
	}

	ASSERT(hmeblkp->hblk_lckcnt == 0);
	ASSERT(hmeblkp->hblk_vcnt == 0 && hmeblkp->hblk_hmecnt == 0);

	sfmmu_hblk_hash_rm(hmebp, hmeblkp, prevpa, pr_hblk);
	hmeblkp->hblk_nextpa = hblkpa;

	shw_hblkp = hmeblkp->hblk_shadow;
	if (shw_hblkp) {
		shw_size = get_hblk_ttesz(shw_hblkp);
		vaddr = get_hblk_base(hmeblkp);
		vshift = vaddr_to_vshift(shw_hblkp->hblk_tag, vaddr, shw_size);
		ASSERT(vshift < 8);
		/*
		 * Atomically clear shadow mask bit
		 */
		do {
			shw_mask = shw_hblkp->hblk_shw_mask;
			ASSERT(shw_mask & (1 << vshift));
			newshw_mask = shw_mask & ~(1 << vshift);
			newshw_mask = cas(&shw_hblkp->hblk_shw_mask,
				shw_mask, newshw_mask);
		} while (newshw_mask != shw_mask);
		hmeblkp->hblk_shadow = NULL;
	}

	/*
	 * remove shadow bit if we are stealing an unused shadow hmeblk.
	 * sfmmu_hblk_alloc needs it that way, will set shadow bit later if
	 * we are indeed allocating a shadow hmeblk.
	 */
	hmeblkp->hblk_shw_bit = 0;

	sfmmu_hblk_steal_count++;
	SFMMU_STAT(sf_steal_count);

	return (1);
}


/*
 * HME_BLK HASH PRIMITIVES
 */

/*
 * This function returns the hment given the hme_blk and a vaddr.
 * It assumes addr has already been checked to belong to hme_blk's
 * range.  If hmenump is passed then we update it with the index.
 */
static struct sf_hment *
sfmmu_hblktohme(struct hme_blk *hmeblkp, caddr_t addr, int *hmenump)
{
	int index = 0;

	ASSERT(in_hblk_range(hmeblkp, addr));

	if (get_hblk_ttesz(hmeblkp) == TTE8K) {
		index = (((u_int)addr >> MMU_PAGESHIFT) & (NHMENTS-1));
	}

	if (hmenump) {
		*hmenump = index;
	}

	return (&hmeblkp->hblk_hme[index]);
}

static struct hme_blk *
sfmmu_hmetohblk(struct sf_hment *sfhme)
{
	struct hme_blk *hmeblkp;
	struct sf_hment *sfhme0;
	struct hme_blk *hblk_dummy = 0;

	sfhme0 = sfhme - sfhme->hme_tte.tte_hmenum;
	hmeblkp = (struct hme_blk *)((u_int)sfhme0 -
		(u_int)&hblk_dummy->hblk_hme[0]);

	return (hmeblkp);
}

/*
 * XXX This will change for ISM 2
 * This code will only be called for a user process that has more than
 * ISM_MAP_SLOTS segments. Performance is not a priority in this case.
 *
 * This routine will look for a user vaddr and hatid in the hash
 * structure.  It returns a valid pfn or -1.
 */
u_int
sfmmu_user_vatopfn(caddr_t vaddr, sfmmu_t *sfmmup)
{
	struct hmehash_bucket *hmebp;
	hmeblk_tag hblktag;
	int hmeshift, hashno = 1;
	struct hme_blk *hmeblkp;
	struct sf_hment *sfhmep;
	u_int pfn;
	tte_t tte;

	u_short		sh_vaddr = 0;
	ism_map_t	*ism_map, m;
	ism_map_blk_t	*ismblkp;
	int		i;
	sfmmu_t *ism_hatid = NULL;

	ASSERT(sfmmup != KHATID);
	pfn = (u_int)-1;

	sh_vaddr = ISM_SHIFT(vaddr);
	ismblkp = sfmmup->sfmmu_ismblk;

	/*
	 * Set ism_hatid if vaddr falls in a ISM segment.
	 *
	 * NOTE: All accesses to ism mappings must use 64 bit
	 *	ld's/st's to prevent MT race condition between
	 *	here and sfmmu_share()/sfmmu_unshare().
	 */
	while (ismblkp) {
		ism_map = ismblkp->maps;
		for (i = 0; ism_map[i].ism_sfmmu && i < ISM_MAP_SLOTS; i++) {

			m.ism_map = ism_map[i].ism_map;
			if (sh_vaddr >= m.ism_vbase &&
			    sh_vaddr < (u_short)(m.ism_vbase + m.ism_size)) {
				ism_hatid = m.ism_sfmmu;
				goto ism_hit;
			}
		}
		ismblkp = ismblkp->next;
	}
ism_hit:

	/*
	 * If lookup into ISM segment then vaddr is converted to offset
	 * into owning ISM segment. All owning ISM segments
	 * start at 0x0.
	 */
	if (ism_hatid) {
		sfmmup = ism_hatid;
		vaddr = (caddr_t)((uint)vaddr -
			((uint)m.ism_vbase << ISM_AL_SHIFT));
	}

	hblktag.htag_id = sfmmup;
	do {
		hmeshift = HME_HASH_SHIFT(hashno);
		hblktag.htag_bspage = HME_HASH_BSPAGE(vaddr, hmeshift);
		hblktag.htag_rehash = hashno;
		hmebp = HME_HASH_FUNCTION(sfmmup, vaddr, hmeshift);

		SFMMU_HASH_LOCK(hmebp);

		HME_HASH_SEARCH(hmebp, hblktag, hmeblkp);
		if (hmeblkp != NULL) {
			sfhmep = sfmmu_hblktohme(hmeblkp, vaddr, 0);
			sfmmu_copytte(&sfhmep->hme_tte, &tte);
			if (TTE_IS_VALID(&tte)) {
				pfn = TTE_TO_PFN(vaddr, &tte);
			}
			SFMMU_HASH_UNLOCK(hmebp);
			return (pfn);
		}
		SFMMU_HASH_UNLOCK(hmebp);
		hashno++;
	} while (sfmmup->sfmmu_lttecnt && (hashno <= MAX_HASHCNT));
	return (pfn);
}

/*
 * Make sure that there is a valid ctx, if not get a ctx.
 * Also, get a readers lock on refcnt, so that the ctx cannot
 * be stolen underneath us.
 */
void
sfmmu_disallow_ctx_steal(sfmmu_t *sfmmup)
{
	struct	ctx *ctx;

	ASSERT(sfmmup != ksfmmup);
	/*
	 * If ctx has been stolen, get a ctx.
	 */
	if (sfmmup->sfmmu_cnum == INVALID_CONTEXT) {
		/*
		 * Our ctx was stolen. Get a ctx with rlock.
		 */
		ctx = sfmmu_get_ctx(sfmmup);
		return;
	} else {
		ctx = sfmmutoctx(sfmmup);
	}

	/*
	 * Try to get the reader lock.
	 */
	if (rwlock_hword_enter(&ctx->c_refcnt, READER_LOCK) == 0) {
		/*
		 * Successful in getting r-lock.
		 * Does ctx still point to sfmmu ?
		 * If NO, the ctx got stolen meanwhile.
		 * 	Release r-lock and try again.
		 * If YES, we are done - just exit
		 */
		if (ctx->c_sfmmu != sfmmup) {
			rwlock_hword_exit(&ctx->c_refcnt, READER_LOCK);
			/*
			 * Our ctx was stolen. Get a ctx with rlock.
			 */
			ctx = sfmmu_get_ctx(sfmmup);
		}
	} else {
		/*
		 * Our ctx was stolen. Get a ctx with rlock.
		 */
		ctx = sfmmu_get_ctx(sfmmup);
	}

	ASSERT(sfmmup->sfmmu_cnum >= NUM_LOCKED_CTXS);
	ASSERT(sfmmutoctx(sfmmup)->c_refcnt > 0);
}

/*
 * Decrement reference count for our ctx. If the reference count
 * becomes 0, our ctx can be stolen by someone.
 */
void
sfmmu_allow_ctx_steal(sfmmu_t *sfmmup)
{
	struct	ctx *ctx;

	ASSERT(sfmmup != ksfmmup);
	ctx = sfmmutoctx(sfmmup);

	ASSERT(ctx->c_refcnt > 0);
	ASSERT(sfmmup == ctx->c_sfmmu);
	ASSERT(sfmmup->sfmmu_cnum != INVALID_CONTEXT);
	rwlock_hword_exit(&ctx->c_refcnt, READER_LOCK);

}


/*
 * TLB Handling Routines
 * These routines get called from the trap vector table.
 * In some cases an optimized assembly handler has already been
 * executed.
 */

/*
 * We get here after we have missed in the TSB or taken a mod bit trap.
 * The TL1 assembly routine passes the contents of the tag access register.
 * Since we are only
 * supporting a 32 bit address space we manage this register as an uint.
 * This routine will try to find the hment for this address in the hment
 * hash and if found it will place the corresponding entry on the TSB and
 * If it fails then we will call trap which will call pagefault.
 * This routine is called via sys_trap and thus, executes at TL0
 */
void
sfmmu_tsb_miss(struct regs *rp, uint tagaccess, uint traptype)
{
	struct hmehash_bucket *hmebp;
	sfmmu_t *sfmmup, *sfmmup_orig;
	hmeblk_tag hblktag;
	int hmeshift, hashno = 1;
	struct hme_blk *hmeblkp;

	caddr_t vaddr;
	u_int ctxnum;
	struct sf_hment *sfhmep;
	struct ctx *ctx;
	tte_t tte, ttemod;

	u_short		sh_vaddr;
	caddr_t		tmp_vaddr;
	ism_map_t	*ism_map, m;
	ism_map_blk_t	*ismblkp;
	int		i;
	sfmmu_t *ism_hatid = NULL;

	SFMMU_STAT(sf_slow_tsbmiss);
	tmp_vaddr = vaddr = (caddr_t)(tagaccess & TAGACC_VADDR_MASK);
	sh_vaddr = ISM_SHIFT(vaddr);
	ctxnum = tagaccess & TAGACC_CTX_MASK;

	/*
	 * Make sure we have a valid ctx and that our context doesn't get
	 * stolen after this point.
	 */
	if (ctxnum == KCONTEXT) {
		sfmmup_orig = ksfmmup;
	} else {
		sfmmup_orig = astosfmmu(curthread->t_procp->p_as);
		sfmmu_disallow_ctx_steal(sfmmup_orig);
		ctxnum = sfmmup_orig->sfmmu_cnum;
		sfmmu_setctx_sec(ctxnum);
	}
	ASSERT(sfmmup_orig == ksfmmup || ctxnum >= NUM_LOCKED_CTXS);

	kpreempt_disable();

	ctx = ctxnumtoctx(ctxnum);
	sfmmup = ctx->c_sfmmu;
	ismblkp = sfmmup->sfmmu_ismblk;
	m.ism_map = (u_longlong_t)0;

	/*
	 * Set ism_hatid if vaddr falls in a ISM segment.
	 *
	 * NOTE: All accesses to ism mappings must use 64 bit
	 *	ld's/st's to prevent MT race condition between
	 *	here and sfmmu_share()/sfmmu_unshare().
	 */
	while (ismblkp) {
		ism_map = ismblkp->maps;
		for (i = 0; ism_map[i].ism_sfmmu && i < ISM_MAP_SLOTS; i++) {

			m.ism_map = ism_map[i].ism_map;
			if (sh_vaddr >= m.ism_vbase &&
			    sh_vaddr < (u_short)(m.ism_vbase + m.ism_size)) {
				ism_hatid = m.ism_sfmmu;
				goto ism_hit;
			}
		}
		ismblkp = ismblkp->next;
	}
ism_hit:

	/*
	 * If ism tlb miss then vaddr is converted into offset
	 * into owning ISM segment. All owning ISM segments
	 * start at 0x0.
	 */
	if (ism_hatid) {
		sfmmup = ism_hatid;
		vaddr = (caddr_t)((uint) vaddr -
			((uint)m.ism_vbase << ISM_AL_SHIFT));
	}

	hblktag.htag_id = sfmmup;
	do {
		hmeshift = HME_HASH_SHIFT(hashno);
		hblktag.htag_bspage = HME_HASH_BSPAGE(vaddr, hmeshift);
		hblktag.htag_rehash = hashno;
		hmebp = HME_HASH_FUNCTION(sfmmup, vaddr, hmeshift);

		SFMMU_HASH_LOCK(hmebp);

		HME_HASH_FAST_SEARCH(hmebp, hblktag, hmeblkp);
		if (hmeblkp != NULL) {
			if ((sfmmup == ksfmmup) &&
			    !CPU_IN_SET(hmeblkp->hblk_cpuset, CPU->cpu_id)) {
				/* update the hmeblkp cpuset field */

				CPUSET_CAS(hmeblkp->hblk_cpuset, CPU->cpu_id);
			}
			sfhmep = sfmmu_hblktohme(hmeblkp, vaddr, 0);
sfmmu_tsbmiss_retry:
			sfmmu_copytte(&sfhmep->hme_tte, &tte);
			if (TTE_IS_VALID(&tte) &&
			    (!TTE_IS_NFO(&tte) ||
			    traptype != T_INSTR_MMU_MISS)) {
				ttemod = tte;
				if (traptype == T_DATA_PROT) {
					/*
					 * We don't need to flush our tlb
					 * because we did it in our trap
					 * handler.  We also don't need to
					 * unload our tsb because the new entry
					 * will replace it.
					 */
					if (TTE_IS_WRITABLE(&tte)) {
						TTE_SET_MOD(&ttemod);
					} else {
						SFMMU_HASH_UNLOCK(hmebp);
						break;
					}
				}
				TTE_SET_REF(&ttemod);
				if (ism_hatid)
					vaddr = tmp_vaddr;
				if (get_hblk_ttesz(hmeblkp) == TTE8K)
					sfmmu_load_tsb(vaddr, ctxnum, &ttemod);
				if (traptype == T_INSTR_MMU_MISS) {
					sfmmu_itlb_ld(vaddr, ctxnum, &ttemod);
				} else {
					sfmmu_dtlb_ld(vaddr, ctxnum, &ttemod);
				}
				if (sfmmu_modifytte_try(&tte, &ttemod,
				    &sfhmep->hme_tte) < 0) {
					/*
					 * pageunload could have unloaded
					 * the tte for us.  In this case
					 * we might have loaded a stale tte
					 * inside the tlb/tte.  Flush both
					 * just in case and retry.
					 */
					vtag_flushpage(vaddr, ctxnum);
					goto sfmmu_tsbmiss_retry;
				}
				SFMMU_HASH_UNLOCK(hmebp);
				/*
				 * This assert can't be before loading the
				 * tsb/tlb or a recursive tlb miss is possible
				 * since the hats are kmemalloced.
				 */
				ASSERT(ism_hatid ||
					(ctxnum == sfmmup->sfmmu_cnum));

				/*
				 * Now we can allow context to be stolen.
				 */
				if (sfmmup_orig != ksfmmup)
					sfmmu_allow_ctx_steal(sfmmup_orig);
				kpreempt_enable();
				return;
			} else {
				SFMMU_HASH_UNLOCK(hmebp);
				hmeblkp = NULL;
				break;
			}
		}
		SFMMU_HASH_UNLOCK(hmebp);
		hashno++;
	} while (sfmmup->sfmmu_lttecnt && (hashno <= MAX_HASHCNT));
	kpreempt_enable();
	ASSERT(ism_hatid || (ctxnum == sfmmup->sfmmu_cnum));

	/*
	 * Now we can allow our context to be stolen.
	 */
	if (sfmmup_orig != ksfmmup)
		sfmmu_allow_ctx_steal(sfmmup_orig);

	/*
	 * If hment was not found in the hash on a protection fault
	 * or it was found but not valid, then it's really a data mmu
	 * miss; fix up the traptype before calling trap.
	 */
	if (hmeblkp == NULL && traptype == T_DATA_PROT)
		traptype = T_DATA_MMU_MISS;

	/* will call pagefault */
	trap(rp, traptype, 0, (caddr_t)tagaccess);
}

/*
 * Flushes caches and tlbs on all cpus for a particular virtual address
 * and ctx.  if noflush is set we do not flush the tlb.
 */
static void
sfmmu_tlbcache_demap(caddr_t addr, sfmmu_t *sfmmup,
	struct hme_blk *hmeblkp, int pfnum, int noflush, int flags)
{
	int ctxnum, vcolor;
	cpuset_t cpuset;

	ctxnum = (int)sfmmutoctxnum(sfmmup);
	vcolor = addr_to_vcolor(addr);

	/*
	 * There is no need to protect against ctx being stolen.  Even if the
	 * ctx is stolen, we need to flush the cache. Our ctx stealer only
	 * flushes the tlbs/tsb.
	 */
	kpreempt_disable();
	if (flags & FLUSH_ALL_CPUS) {
		cpuset = cpu_ready_set;
	} else {
		if (ctxnum == KCONTEXT) {
			cpuset = hmeblkp->hblk_cpuset;
		} else {
			cpuset = sfmmup->sfmmu_cpusran;
		}
		CPUSET_AND(cpuset, cpu_ready_set);
	}
	CPUSET_DEL(cpuset, CPU->cpu_id);
	SFMMU_XCALL_STATS(cpuset, ctxnum);
	xt_some(cpuset, (u_int)vac_flushpage_tl1, pfnum, vcolor, 0, 0);
	vac_flushpage(pfnum, vcolor);
	if (!noflush) {
		/* flush tsb and tlb */
		xt_some(cpuset, (u_int)vtag_flushpage_tl1,
			(u_int)addr, ctxnum, 0, 0);
		vtag_flushpage(addr, ctxnum);
	}
	kpreempt_enable();
}

/*
 * We need to flush the cache in all cpus.  It is possible that
 * a process referenced a page as cacheable but has sinced exited
 * and cleared the mapping list.  We still to flush it but have no
 * state so all cpus is the only alternative.
 */
void
sfmmu_cache_flush(int pfnum, int vcolor)
{
	cpuset_t cpuset;
	extern cpuset_t cpu_ready_set;

	kpreempt_disable();
	cpuset = cpu_ready_set;
	CPUSET_DEL(cpuset, CPU->cpu_id);
	xt_some(cpuset, (u_int)vac_flushpage_tl1, pfnum, vcolor, 0, 0);
	vac_flushpage(pfnum, vcolor);
	xt_sync(cpu_ready_set);
	kpreempt_enable();
}

void
sfmmu_cache_flushcolor(int vcolor)
{
	cpuset_t cpuset;
	extern cpuset_t cpu_ready_set;

	kpreempt_disable();
	cpuset = cpu_ready_set;
	CPUSET_DEL(cpuset, CPU->cpu_id);
	xt_some(cpuset, (u_int)vac_flushcolor_tl1, vcolor, 0, 0, 0);
	vac_flushcolor(vcolor);
	xt_sync(cpu_ready_set);
	kpreempt_enable();
}

/*
 * Demaps the tsb and flushes all tlbs on all cpus for a particular virtual
 * address and ctx. if noflush is set we do not flush the tlb.
 */
static void
sfmmu_tlb_demap(caddr_t addr, sfmmu_t *sfmmup, struct hme_blk *hmeblkp,
	int noflush)
{
	int ctxnum;
	cpuset_t cpuset;

	ctxnum = (int)sfmmutoctxnum(sfmmup);
	if (ctxnum == INVALID_CONTEXT) {
		/*
		 * if ctx was stolen then simply return
		 * whoever stole ctx is responsible for flush.
		 */
		return;
	}

	/*
	 * There is no need to protect against ctx being stolen.  If the
	 * ctx is stolen we will simply get an extra flush.
	 */
	if (!noflush) {
		/*
		 * if process is exiting then delay flush.
		 */
		kpreempt_disable();
		if (ctxnum == KCONTEXT) {
			cpuset = hmeblkp->hblk_cpuset;
		} else {
			cpuset = sfmmup->sfmmu_cpusran;
		}
		CPUSET_DEL(cpuset, CPU->cpu_id);
		CPUSET_AND(cpuset, cpu_ready_set);
		SFMMU_XCALL_STATS(cpuset, ctxnum);
		xt_some(cpuset, (u_int)vtag_flushpage_tl1,
			(u_int)addr, ctxnum, 0, 0);
		vtag_flushpage(addr, ctxnum);
		kpreempt_enable();
	}
}

/*
 * Flushes only TLB.
 */
static
void
sfmmu_tlb_ctx_demap(sfmmu_t *sfmmup)
{
	int ctxnum;
	cpuset_t cpuset;

	ctxnum = (int)sfmmutoctxnum(sfmmup);
	if (ctxnum == INVALID_CONTEXT) {
		/*
		 * if ctx was stolen then simply return
		 * whoever stole ctx is responsible for flush.
		 */
		return;
	}
	ASSERT(ctxnum != KCONTEXT);
	/*
	 * There is no need to protect against ctx being stolen.  If the
	 * ctx is stolen we will simply get an extra flush.
	 */
	kpreempt_disable();

	cpuset = sfmmup->sfmmu_cpusran;
	CPUSET_DEL(cpuset, CPU->cpu_id);
	CPUSET_AND(cpuset, cpu_ready_set);
	SFMMU_XCALL_STATS(cpuset, ctxnum);
	/*
	 * we only flush tlb.  tsbs are flushed lazily
	 * RFE: it might be worth delaying the tlb flush as well. In that
	 * case each cpu would have to traverse the dirty list and flush
	 * each one of those ctx from the tlb.
	 */
	vtag_flushctx(ctxnum);
	xt_some(cpuset, (u_int)vtag_flushctx_tl1, ctxnum, 0, 0, 0);

	kpreempt_enable();
}

void
sfmmu_inv_tsb(caddr_t tsb_bs, u_int tsb_bytes)
{
	struct tsbe *tsbaddr;

	for (tsbaddr = (struct tsbe *)tsb_bs;
	    (uint)tsbaddr < (uint)(tsb_bs + tsb_bytes);
	    tsbaddr++) {
		tsbaddr->tte_tag.tag_inthi = TSBTAG_INVALID;
	}
}

/*
 * Initialize per cpu tsb and per cpu tsbmiss_area
 */
void
sfmmu_init_tsbs()
{
	int i, tsbsz;
	caddr_t vaddr;
	struct tsbmiss *tsbmissp;
	extern int	uhmehash_num;
	extern int	khmehash_num;
	extern struct hmehash_bucket	*uhme_hash;
	extern struct hmehash_bucket	*khme_hash;
	extern caddr_t tsbmiss_area;
	extern int	dcache_line_mask;
	extern struct ctx *ctxs;

	tsbmissp = (struct tsbmiss *)&tsbmiss_area;
	tsbsz = TSB_BYTES(tsb_szcode);
	vaddr = tsballoc_base;
	for (i = 0; i < NCPU; tsbmissp++, i++) {
		if (cpunodes[i].nodeid == 0)
			continue;
		tsb_bases[i].tsb_vbase = vaddr;
		tsb_bases[i].tsb_pfnbase = va_to_pfn(vaddr);
		ASSERT(!((uint)vaddr & (tsbsz - 1)));
		sfmmu_inv_tsb(vaddr, tsbsz);
		/*
		 * initialize the tsbmiss area.
		 */
		tsbmissp->sfmmup = ksfmmup;
		tsbmissp->khashsz = khmehash_num;
		tsbmissp->khashstart = khme_hash;
		tsbmissp->uhashsz = uhmehash_num;
		tsbmissp->uhashstart = uhme_hash;
		tsbmissp->dcache_line_mask = dcache_line_mask;
		tsbmissp->ctxs = ctxs;
		vaddr += tsbsz;
	}
}

/*
 * this function creates nucleus hmeblks and adds them to the freelists.
 * It returns the approximate number of 8k hmeblks we could create with
 * the given segment of nucleus memory.
 */
int
sfmmu_add_nucleus_hblks(caddr_t addr, int size)
{
	int i, j = 0, k = 0;
	struct hme_blk *hmeblkp;
	int hme8blk_sz, hme1blk_sz;

	ASSERT(addr && size);
	hme8blk_sz = roundup(HME8BLK_SZ, sizeof (double));
	hme1blk_sz = roundup(HME1BLK_SZ, sizeof (double));

	HBLK8_FLIST_LOCK();
	/*
	 * create nucleus hmeblks and add to the freelist. Try to allocate
	 * 8 hme8blks for every hme1blk. Note that hme8blk is about three
	 * times the size of hme1blk.
	 */
	for (i = 0; i <= (size - size/24 - hme8blk_sz); i += hme8blk_sz, j++) {
		hmeblkp = (struct hme_blk *)addr;
		addr += hme8blk_sz;
		hmeblkp->hblk_nuc_bit = 1;
		hmeblkp->hblk_nextpa = va_to_pa((caddr_t)hmeblkp);
		if (hblk8_avail++ == 0) {
			hblk8_flist = hblk8_flist_t = hmeblkp;
			hmeblkp->hblk_next = NULL;
		} else {
			hmeblkp->hblk_next = hblk8_flist;
			hblk8_flist = hmeblkp;
		}
		SFMMU_STAT(sf_hblk8_nalloc);
	}
	hblk8_allocated += j;
	HBLK_DEBUG_COUNTER_INCR(nhblk8_allocated, j);
	HBLK8_FLIST_UNLOCK();

	HBLK1_FLIST_LOCK();
	for (; i <= (size - hme1blk_sz); i += hme1blk_sz, k++) {
		hmeblkp = (struct hme_blk *)addr;
		addr += hme1blk_sz;
		hmeblkp->hblk_nuc_bit = 1;
		hmeblkp->hblk_nextpa = va_to_pa((caddr_t)hmeblkp);
		if (hblk1_avail++ == 0) {
			hblk1_flist = hblk1_flist_t = hmeblkp;
			hmeblkp->hblk_next = NULL;
		} else {
			hmeblkp->hblk_next = hblk1_flist;
			hblk1_flist = hmeblkp;
		}
		SFMMU_STAT(sf_hblk1_nalloc);
	}
	hblk1_allocated += k;
	HBLK_DEBUG_COUNTER_INCR(nhblk1_allocated, k);
	HBLK1_FLIST_UNLOCK();

	PRM_DEBUG(hblk8_avail);
	PRM_DEBUG(hblk1_avail);

	return (j);
}

/*
 * This function is currently not supported on this platform. For what
 * it's supposed to do, see hat.c and hat_srmmu.c
 */
/* ARGSUSED */
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
/* ARGSUSED */
faultcode_t
hat_pageflip(hat, addr_to, kaddr, lenp, pp_to, pp_from)
	struct hat *hat;
	caddr_t	addr_to, kaddr;
	size_t	*lenp;
	page_t	**pp_to, **pp_from;
{
	return (FC_NOSUPPORT);
}

/*
 * Enter a hme on the mapping list for page pp.
 * When large pages are more prevalent in the system we might want to
 * keep the mapping list in ascending order by the hment size. For now,
 * small pages are more frequent, so don't slow it down.
 */
static void
hme_add(struct sf_hment *hme, machpage_t *pp)
{
	ASSERT(sfmmu_mlist_held(pp));

	hme->hme_prev = NULL;
	hme->hme_next = pp->p_mapping;
	hme->hme_page = pp;
	if (pp->p_mapping) {
		pp->p_mapping->hme_prev = hme;
		ASSERT(pp->p_share > 0);
	} else  {
		ASSERT(pp->p_share == 0);
	}
	pp->p_mapping = hme;
	/*
	 * Update number of mappings.
	 */
	pp->p_share++;
}

/*
 * Enter a hme on the mapping list for page pp.
 * If we are unmapping a large translation, we need to make sure that the
 * change is reflect in the corresponding bit of the p_index field.
 */
static void
hme_sub(struct sf_hment *hme, machpage_t *pp)
{
	ASSERT(sfmmu_mlist_held(pp));
	ASSERT(hme->hme_page == pp);

	if (pp->p_mapping == NULL) {
		cmn_err(CE_PANIC, "hme_remove - no mappings");
	}

	ASSERT(pp->p_share > 0);
	pp->p_share--;

	if (hme->hme_prev) {
		ASSERT(pp->p_mapping != hme);
		ASSERT(hme->hme_prev->hme_page == pp);
		hme->hme_prev->hme_next = hme->hme_next;
	} else {
		ASSERT(pp->p_mapping == hme);
		pp->p_mapping = hme->hme_next;
		ASSERT((pp->p_mapping == NULL) ?
			(pp->p_share == 0) : 1);
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
	hme->hme_page = (machpage_t *)NULL;

	if (hme_size(hme) > TTE8K) {
		/*
		 * remove mappings for the
		 * reset of the large page.
		 */
		sfmmu_rm_large_mappings(pp, hme_size(hme));
	}
}

/*
 * Searchs the mapping list of the page for a mapping of the same size. If not
 * found the corresponding bit is cleared in the p_index field. When large
 * pages are more prevalent in the system, we can maintain the mapping list
 * in order and we don't have to traverse the list each time. Just check the
 * next and prev entries, and if both are of different size, we clear the bit.
 */
static void
sfmmu_rm_large_mappings(machpage_t *pp, int ttesz)
{
	struct sf_hment *sfhmep;
	int npgs, index;

	ASSERT(ttesz > TTE8K);

	ASSERT(sfmmu_mlist_held(pp));

	ASSERT(PP_ISMAPPED_LARGE(pp));

	/*
	 * Traverse mapping list looking for another mapping of same size.
	 * since we only want to clear index field if all mappings of
	 * that size are gone.
	 */

	for (sfhmep = pp->p_mapping; sfhmep; sfhmep = sfhmep->hme_next) {
		if (hme_size(sfhmep) == ttesz) {
			/*
			 * another mapping of the same size. don't clear index.
			 */
			return;
		}
	}

	/*
	 * Clear the p_index bit for large page.
	 */
	index = PAGESZ_TO_INDEX(ttesz);
	npgs = TTEPAGES(ttesz);
	while (npgs-- > 0) {
		ASSERT(pp->p_index & index);
		pp->p_index &= ~index;
		pp = PP_PAGENEXT(pp);
	}
}

/*
 * return supported features
 */
/* ARGSUSED */
int
hat_supported(enum hat_features feature, void *arg)
{
	switch (feature) {
	case    HAT_SHARED_PT:
		return (1);
	default:
		return (0);
	}
}

void
hat_enter(struct hat *hat)
{
	mutex_enter(&hat->sfmmu_mutex);
}

void
hat_exit(struct hat *hat)
{
	mutex_exit(&hat->sfmmu_mutex);
}

u_long
hat_getkpfnum(caddr_t addr)
{
	return (hat_getpfnum((struct hat *)kas.a_hat, addr));
}

static void
hat_kstat_init(void)
{
	kstat_t *ksp;
	extern	struct sfmmu_global_stat sfmmu_global_stat;

	ksp = kstat_create("unix", 0, "sfmmu_global_stat", "hat",
		KSTAT_TYPE_RAW, sizeof (struct sfmmu_global_stat),
		KSTAT_FLAG_VIRTUAL);
	if (ksp) {
		ksp->ks_data = (void *) &sfmmu_global_stat;
		kstat_install(ksp);
	}
	ksp = kstat_create("unix", 0, "sfmmu_percpu_stat", "hat",
		KSTAT_TYPE_RAW, sizeof (struct sfmmu_percpu_stat) * NCPU,
		KSTAT_FLAG_WRITABLE);
	if (ksp) {
		ksp->ks_update = sfmmu_kstat_percpu_update;
		kstat_install(ksp);
	}
}

static int
sfmmu_kstat_percpu_update(kstat_t *ksp, int rw)
{
	struct tsbmiss *tsbm;
	int i;
	extern caddr_t tsbmiss_area;
	struct sfmmu_percpu_stat *cpu_kstat = ksp->ks_data;

	ASSERT(cpu_kstat);
	if (rw == KSTAT_READ) {
		tsbm = (struct tsbmiss *)&tsbmiss_area;
		for (i = 0; i < NCPU; cpu_kstat++, tsbm++, i++) {
			cpu_kstat->sf_itlb_misses = tsbm->itlb_misses;
			cpu_kstat->sf_dtlb_misses = tsbm->dtlb_misses;
			cpu_kstat->sf_utsb_misses = tsbm->utsb_misses;
			cpu_kstat->sf_ktsb_misses = tsbm->ktsb_misses;
			if (tsbm->itlb_misses > 0 && tsbm->dtlb_misses > 0) {
				cpu_kstat->sf_tsb_hits =
				(tsbm->itlb_misses + tsbm->dtlb_misses) -
				(tsbm->utsb_misses + tsbm->ktsb_misses);
			} else {
				cpu_kstat->sf_tsb_hits = 0;
			}
			cpu_kstat->sf_umod_faults = tsbm->uprot_traps;
			cpu_kstat->sf_kmod_faults = tsbm->kprot_traps;
		}
	} else {
		/* KSTAT_WRITE is used to clear stats */
		tsbm = (struct tsbmiss *)&tsbmiss_area;
		for (i = 0; i < NCPU; tsbm++, i++) {
			tsbm->itlb_misses = 0;
			tsbm->dtlb_misses = 0;
			tsbm->utsb_misses = 0;
			tsbm->ktsb_misses = 0;
			tsbm->uprot_traps = 0;
			tsbm->kprot_traps = 0;
		}
	}
	return (0);
}

#ifdef DEBUG
/*
 * Debug code that verifies hblk lists are correct
 */
static void
sfmmu_check_hblk_flist()
{
	int i;
	struct hme_blk *hmeblkp, *pr_hmeblkp;

	HBLK8_FLIST_LOCK();
	for (i = 0, pr_hmeblkp = NULL, hmeblkp = hblk8_flist; hmeblkp; i++) {
			pr_hmeblkp = hmeblkp;
			hmeblkp = hmeblkp->hblk_next;
	}
	if (i != hblk8_avail || pr_hmeblkp != hblk8_flist_t ||
		hblk8_allocated != (hblk8_avail + hblk8_inuse)) {
		cmn_err(CE_PANIC,
			"sfmmu_check_hblk_flist: inconsistent hblk8_flist");
	}
	HBLK8_FLIST_UNLOCK();

	HBLK1_FLIST_LOCK();
	for (i = 0, pr_hmeblkp = NULL, hmeblkp = hblk1_flist; hmeblkp; i++) {
			pr_hmeblkp = hmeblkp;
			hmeblkp = hmeblkp->hblk_next;
	}
	if (i != hblk1_avail || pr_hmeblkp != hblk1_flist_t ||
		hblk1_allocated != (hblk1_avail + hblk1_inuse)) {
		cmn_err(CE_PANIC,
			"sfmmu_check_hblk_flist: inconsistent hblk1_flist");
	}
	HBLK1_FLIST_UNLOCK();
}

tte_t  *gorig[NCPU], *gcur[NCPU], *gnew[NCPU];

/*
 * A tte checker. *orig_old is the value we read before cas.
 *	*cur is the value returned by cas.
 *	*new is the desired value when we do the cas.
 *
 *	*hmeblkp is currently unused.
 */

/* ARGSUSED */
void
chk_tte(tte_t *orig_old, tte_t *cur, tte_t *new, struct hme_blk *hmeblkp)
{
	u_int i, j, k;
	int cpuid = CPU->cpu_id;

	gorig[cpuid] = orig_old;
	gcur[cpuid] = cur;
	gnew[cpuid] = new;

#ifdef lint
	hmeblkp = hmeblkp;
#endif

	if (TTE_IS_VALID(orig_old)) {
		if (TTE_IS_VALID(cur)) {
			i = TTE_TO_TTEPFN(orig_old);
			j = TTE_TO_TTEPFN(cur);
			k = TTE_TO_TTEPFN(new);
			if (i != j) {
				/* remap error? */
				panic("chk_tte: bad pfn, 0x%x, 0x%x",
					i, j);
			}

			if (i != k) {
				/* remap error? */
				panic("chk_tte: bad pfn2, 0x%x, 0x%x",
					i, k);
			}
		} else {
			if (TTE_IS_VALID(new)) {
				panic("chk_tte: invalid cur? ");
			}

			i = TTE_TO_TTEPFN(orig_old);
			k = TTE_TO_TTEPFN(new);
			if (i != k) {
				panic("chk_tte: bad pfn3, 0x%x, 0x%x",
					i, k);
			}
		}
	} else {
		if (TTE_IS_VALID(cur)) {
			j = TTE_TO_TTEPFN(cur);
			if (TTE_IS_VALID(new)) {
				k = TTE_TO_TTEPFN(new);
				if (j != k) {
					panic("chk_tte: bad pfn4, 0x%x, 0x%x",
						j, k);
				}
			} else {
				panic("chk_tte: why here?");
			}
		} else {
			if (!TTE_IS_VALID(new)) {
				panic("chk_tte: why here2 ?");
			}
		}
	}
}

#endif /* DEBUG */
