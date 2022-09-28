/*
 * Copyright (c) 1993, 1994 by Sun Microsystems, Inc.
 */

#pragma	ident	"@(#)kmem.c	1.32	96/10/17 SMI"

/*
 * Kernel memory allocator, as described in:
 *
 * Jeff Bonwick,
 * The Slab Allocator: An Object-Caching Kernel Memory Allocator.
 * Proceedings of the Summer 1994 Usenix Conference.
 *
 * See /shared/sac/PSARC/1994/028 for copies of the paper and
 * related design documentation.
 */

#include <sys/kmem.h>
#include <sys/kmem_impl.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/vm.h>
#include <sys/proc.h>
#include <sys/tuneable.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/mutex.h>
#include <sys/bitmap.h>
#include <sys/vtrace.h>
#include <sys/kobj.h>
#include <vm/page.h>
#include <vm/seg_kmem.h>
#include <sys/map.h>

extern void prom_printf(char *fmt, ...);

struct kmem_cpu_kstat {
	kstat_named_t	kmcpu_alloc_from;
	kstat_named_t	kmcpu_free_to;
	kstat_named_t	kmcpu_buf_avail;
} kmem_cpu_kstat_template = {
	{ "alloc_from_cpu%d",		KSTAT_DATA_LONG },
	{ "free_to_cpu%d",		KSTAT_DATA_LONG },
	{ "buf_avail_cpu%d",		KSTAT_DATA_LONG },
};

struct kmem_cpu_kstat *kmem_cpu_kstat;

struct kmem_cache_kstat {
	kstat_named_t	kmc_buf_size;
	kstat_named_t	kmc_align;
	kstat_named_t	kmc_chunk_size;
	kstat_named_t	kmc_slab_size;
	kstat_named_t	kmc_alloc;
	kstat_named_t	kmc_alloc_fail;
	kstat_named_t	kmc_depot_alloc;
	kstat_named_t	kmc_depot_free;
	kstat_named_t	kmc_depot_contention;
	kstat_named_t	kmc_global_alloc;
	kstat_named_t	kmc_buf_constructed;
	kstat_named_t	kmc_buf_avail;
	kstat_named_t	kmc_buf_total;
	kstat_named_t	kmc_buf_max;
	kstat_named_t	kmc_slab_create;
	kstat_named_t	kmc_slab_destroy;
	kstat_named_t	kmc_memory_class;
	kstat_named_t	kmc_hash_size;
	kstat_named_t	kmc_hash_lookup_depth;
	kstat_named_t	kmc_hash_rescale;
	kstat_named_t	kmc_full_magazines;
	kstat_named_t	kmc_empty_magazines;
	kstat_named_t	kmc_magazine_size;
} kmem_cache_kstat_template = {
	{ "buf_size",		KSTAT_DATA_LONG },
	{ "align",		KSTAT_DATA_LONG },
	{ "chunk_size",		KSTAT_DATA_LONG },
	{ "slab_size",		KSTAT_DATA_LONG },
	{ "alloc",		KSTAT_DATA_LONG },
	{ "alloc_fail",		KSTAT_DATA_LONG },
	{ "depot_alloc",	KSTAT_DATA_LONG },
	{ "depot_free",		KSTAT_DATA_LONG },
	{ "depot_contention",	KSTAT_DATA_LONG },
	{ "global_alloc",	KSTAT_DATA_LONG },
	{ "buf_constructed",	KSTAT_DATA_LONG },
	{ "buf_avail",		KSTAT_DATA_LONG },
	{ "buf_total",		KSTAT_DATA_LONG },
	{ "buf_max",		KSTAT_DATA_LONG },
	{ "slab_create",	KSTAT_DATA_LONG },
	{ "slab_destroy",	KSTAT_DATA_LONG },
	{ "memory_class",	KSTAT_DATA_LONG },
	{ "hash_size",		KSTAT_DATA_LONG },
	{ "hash_lookup_depth",	KSTAT_DATA_LONG },
	{ "hash_rescale",	KSTAT_DATA_LONG },
	{ "full_magazines",	KSTAT_DATA_LONG },
	{ "empty_magazines",	KSTAT_DATA_LONG },
	{ "magazine_size",	KSTAT_DATA_LONG },
};

struct kmem_cache_kstat *kmem_cache_kstat;

struct {
	kstat_named_t	arena_size;
	kstat_named_t	huge_size;
	kstat_named_t	huge_alloc;
	kstat_named_t	huge_alloc_fail;
	kstat_named_t	perm_size;
	kstat_named_t	perm_alloc;
	kstat_named_t	perm_alloc_fail;
} kmem_misc_kstat = {
	{ "arena_size",		KSTAT_DATA_LONG },
	{ "huge_size",		KSTAT_DATA_LONG },
	{ "huge_alloc",		KSTAT_DATA_LONG },
	{ "huge_alloc_fail",	KSTAT_DATA_LONG },
	{ "perm_size",		KSTAT_DATA_LONG },
	{ "perm_alloc",		KSTAT_DATA_LONG },
	{ "perm_alloc_fail",	KSTAT_DATA_LONG },
};

/*
 * The default set of caches to back kmem_alloc().
 * These sizes should be reevaluated periodically.
 */
static int kmem_alloc_sizes[] = {
	8,
	16,	24,
	32,	40,	48,	56,
	64,	80,	96,	112,
	128,	144,	160,	176,	192,	208,	224,	240,
	256,	320,	384,	448,
	512,	576,	672,	800,
	1024,	1152,	1344,	1632,
	2048,	2720,
	4096,	5440,	6144,	6816,
	8192,	10240,	12288,
	16384
};

#define	KMEM_MAXBUF	16384

static kmem_cache_t *kmem_alloc_table[KMEM_MAXBUF >> KMEM_ALIGN_SHIFT];

/*
 * The magazine types for fast per-cpu allocation
 */
typedef struct kmem_magazine_type {
	int		mt_magsize;	/* magazine size (number of rounds) */
	int		mt_align;	/* magazine alignment */
	int		mt_minbuf;	/* all smaller buffers qualify */
	int		mt_maxbuf;	/* no larger buffers qualify */
	kmem_cache_t	*mt_cache;
} kmem_magazine_type_t;

kmem_magazine_type_t kmem_magazine_type[] = {
	{ 1,	8,	3200,	16384	},
	{ 3,	16,	256,	16384	},
	{ 7,	32,	64,	2048	},
	{ 15,	64,	0,	1024	},
	{ 31,	64,	0,	512	},
	{ 47,	64,	0,	256	},
	{ 63,	64,	0,	128	},
	{ 95,	64,	0,	64	},
	{ 143,	64,	0,	0	},
};

int kmem_page_alloc_fail;
u_int kmem_random;
static int kmem_reap_lasttime;	/* time of last reap */

/*
 * kmem tunables
 */
int kmem_reap_interval;		/* cache reaping rate [15 * HZ ticks] */
int kmem_depot_contention = 3;	/* max failed tryenters per real interval */
int kmem_reapahead = 0;		/* start reaping N pages before pageout */
int kmem_minhash = 512;		/* threshold for hashing (using bufctls) */
int kmem_align = KMEM_ALIGN;	/* minimum alignment for all caches */
int kmem_panic = 1;		/* whether to panic on error */
u_int kmem_mtbf = UINT_MAX;	/* mean time between injected failures */
int kmem_log_size;		/* KMF_AUDIT log size [2% of memory] */
int kmem_logging = 1;		/* kmem_log_enter() override */
int kmem_content_maxsave = 256;	/* KMF_CONTENTS max bytes to log */
int kmem_self_debug = KMC_NODEBUG; /* set to 0 to have allocator debug itself */

/*
 * On-the-fly kmem debugging
 *
 * The kernel memory allocator supports on-the-fly debugging for use
 * in the field.  There are two reasons you may want to do this:
 * (1) the system has somehow gotten into a strange state (e.g. leaking
 * memory at a tremendous rate) that a reboot would destroy; (2) to
 * diagnose problems at mission-critical (reboot-intolerant) sites.
 *
 * To enable debugging for a particular cache, set kmem_flags to the
 * desired value (using adb -kw), then set kmem_debug_enable to the
 * address of the cache you want to debug (or -1 if you want to debug
 * all caches).  Typically you'll want to select one specific cache
 * to track a memory leak, but you'll want to select all caches to
 * isolate a heap corruption bug.
 *
 * To disable on-the-fly debugging, set kmem_debug_disable to the
 * address of the cache (or -1 for all caches).
 *
 * On-the-fly debugging works by creating a new kmem cache with ".DEBUG"
 * appended to the original cache name, the cache_flags set according to
 * kmem_flags, and all other properties the same as the original.  All
 * allocations from the original cache are passed through to the .DEBUG
 * cache; all frees to the original cache are examined to determine where
 * the buffer really came from (original or .DEBUG) and freed accordingly.
 *
 * The main virtue of this implementation technique is that it requires
 * almost no additional code.  The main drawback is that it makes
 * on-the-fly debugging a one-shot deal: once you've created a .DEBUG
 * cache, you can toggle debugging on and off but you *cannot* create
 * a new .DEBUG cache with different flag values -- so choose carefully!
 */
kmem_cache_t *kmem_debug_enable;	/* enable cache debug (-1 means all) */
kmem_cache_t *kmem_debug_disable;	/* disable cache debug (-1 means all) */

#ifdef DEBUG
#define	KMEM_RANDOM_ALLOCATION_FAILURE(flags, beancounter)		\
	kmem_random = (kmem_random * 2416 + 374441) % 1771875;		\
	if ((flags & KM_NOSLEEP) && kmem_random < 1771875 / kmem_mtbf) { \
		beancounter++;						\
		return (NULL);						\
	}
int kmem_flags = KMF_AUDIT | KMF_DEADBEEF | KMF_REDZONE | KMF_CONTENTS;
#else
#define	KMEM_RANDOM_ALLOCATION_FAILURE(flags, beancounter)
int kmem_flags = 0;
#endif
int kmem_ready;

static kmem_backend_t	kmem_default_backend;

static kmem_cache_t	*kmem_slab_cache;
static kmem_cache_t	*kmem_bufctl_cache;
static kmem_cache_t	*kmem_bufctl_audit_cache;
static kmem_cache_t	*kmem_pagectl_cache;

static kmutex_t		kmem_cache_lock;	/* inter-cache linkage only */
static kmem_cache_t	*kmem_cache_freelist;
kmem_cache_t		kmem_null_cache;

static kmutex_t		kmem_async_lock;
static kmem_async_t	*kmem_async_freelist;
static kmem_async_t	kmem_async_queue;
static kcondvar_t	kmem_async_cv;
static kmutex_t		kmem_async_serialize;

static kmutex_t		kmem_backend_lock;
static kmutex_t		kmem_perm_lock;
static kmem_perm_t	*kmem_perm_freelist;

kmem_pagectl_t		*kmem_pagectl_hash[KMEM_PAGECTL_HASH_SIZE];

kmem_log_header_t	*kmem_transaction_log;
kmem_log_header_t	*kmem_content_log;

#define	KMEM_BZERO_INLINE	64	/* do inline bzero for smaller bufs */

#define	KMERR_MODIFIED	0	/* buffer modified while on freelist */
#define	KMERR_REDZONE	1	/* redzone violation (write past end of buf) */
#define	KMERR_BADADDR	2	/* freed a bad (unallocated) address */
#define	KMERR_DUPFREE	3	/* freed a buffer twice */
#define	KMERR_BADBUFTAG	4	/* buftag corrupted */
#define	KMERR_BADBUFCTL	5	/* bufctl corrupted */
#define	KMERR_BADCACHE	6	/* freed a buffer to the wrong cache */
#define	KMERR_WRONGSIZE	7	/* free size != alloc size */

#define	KMEM_CPU_CACHE(cp)	\
	(kmem_cpu_cache_t *)((char *)cp + CPU->cpu_cache_offset)

struct {
	hrtime_t	kmp_timestamp;	/* timestamp of panic */
	int		kmp_error;	/* type of kmem error */
	kmem_cache_t	*kmp_cache;	/* buffer's cache */
	void		*kmp_buffer;	/* buffer that induced panic */
	kmem_bufctl_t	*kmp_bufctl;	/* buffer's bufctl */
	kmem_bufctl_t	*kmp_realbcp;	/* bufctl according to kmem_locate() */
} kmem_panic_info;

static void
copy_pattern(uint32_t pattern, void *buf_arg, int size)
{
	uint32_t *bufend = (uint32_t *)((char *)buf_arg + size);
	uint32_t *buf = buf_arg;

	while (buf < bufend - 3) {
		buf[3] = buf[2] = buf[1] = buf[0] = pattern;
		buf += 4;
	}
	while (buf < bufend)
		*buf++ = pattern;
}

static void *
verify_pattern(uint32_t pattern, void *buf_arg, int size)
{
	uint32_t *bufend = (uint32_t *)((char *)buf_arg + size);
	uint32_t *buf;

	for (buf = buf_arg; buf < bufend; buf++)
		if (*buf != pattern)
			return (buf);
	return (NULL);
}

/*
 * Verifying that memory hasn't been modified while on the freelist (deadbeef)
 * and copying in a known pattern to detect uninitialized data use (baddcafe)
 * are among the most expensive operations in a DEBUG kernel -- typically
 * consuming 10-20% of overall system performance -- so they have to be as
 * fast as possible.  verify_and_copy_pattern() provides a high-performance
 * solution that combines the verify and copy operations and minimizes the
 * total number of branches, load stalls and cache misses.  Most of the
 * optimizations are familiar -- loop unrolling, using bitwise "or" rather
 * than logical "or" to collapse several compaisons down to one, etc.
 *
 * The one thing that's not obvious is the way the main loop terminates.
 * It does not check for buf < bufend because it can safely assume that
 * every buffer has a buftag -- that is, four or more words consisting
 * of a redzone, bufctl pointer, etc -- which can be used as a sentinel.
 * Therefore the main loop terminates when it encounters a pattern
 * match error *or* when it hits the buftag -- whichever comes first.
 * The cleanup loop (while buf < bufend) takes at most 3 iterations
 * to discriminate between clean termination and pattern mismatch.
 */
static void *
verify_and_copy_pattern(uint32_t old, uint32_t new, void *buf_arg, int size)
{
	uint32_t *bufend = (uint32_t *)((char *)buf_arg + size);
	uint32_t *buf = buf_arg;
	uint32_t tmp0, tmp1, tmp2, tmp3;

	tmp0 = buf[0];
	tmp1 = buf[1];
	tmp2 = buf[2];
	tmp3 = buf[3];
	while ((tmp0 - old | tmp1 - old | tmp2 - old | tmp3 - old) == 0) {
		tmp0 = buf[4];
		tmp1 = buf[5];
		tmp2 = buf[6];
		tmp3 = buf[7];
		buf[0] = new;
		buf[1] = new;
		buf[2] = new;
		buf[3] = new;
		buf += 4;
	}
	while (buf < bufend) {
		if (*buf != old) {
			copy_pattern(old, buf_arg,
				(char *)buf - (char *)buf_arg);
			return (buf);
		}
		*buf++ = new;
	}
	return (NULL);
}

/*
 * Debugging support.  Given any buffer address, find its bufctl
 * by searching every cache in the system.
 */
static kmem_bufctl_t *
kmem_locate(void *buf)
{
	kmem_cache_t *cp;
	kmem_bufctl_t *bcp = NULL;

	mutex_enter(&kmem_cache_lock);
	for (cp = kmem_null_cache.cache_next; cp != &kmem_null_cache;
	    cp = cp->cache_next) {
		if (!(cp->cache_flags & KMF_HASH))
			continue;
		mutex_enter(&cp->cache_lock);
		for (bcp = *KMEM_HASH(cp, buf); bcp != NULL; bcp = bcp->bc_next)
			if (bcp->bc_addr == buf)
				break;
		mutex_exit(&cp->cache_lock);
		if (bcp != NULL)
			break;
	}
	mutex_exit(&kmem_cache_lock);
	return (bcp);
}

static void
kmem_bufctl_display(kmem_bufctl_audit_t *bcp)
{
	int d;
	timestruc_t ts;

	hrt2ts(kmem_panic_info.kmp_timestamp - bcp->bc_timestamp, &ts);
	prom_printf("\nthread=%x  time=T-%d.%09d  slab=%x  cache: %s\n",
		bcp->bc_thread, ts.tv_sec, ts.tv_nsec,
		bcp->bc_slab, bcp->bc_cache->cache_name);
	for (d = 0; d < bcp->bc_depth; d++) {
		u_int off;
		char *sym = kobj_getsymname(bcp->bc_stack[d], &off);
		prom_printf("%s+%x\n", sym ? sym : "?", off);
	}
}

static void
kmem_error(int error, kmem_cache_t *cp, void *buf, kmem_bufctl_t *bcp)
{
	kmem_buftag_t *btp = KMEM_BUFTAG(cp, buf);
	uint32_t *off;

	kmem_logging = 0;	/* stop logging when a bad thing happens */

	kmem_panic_info.kmp_timestamp = gethrtime();
	kmem_panic_info.kmp_error = error;
	kmem_panic_info.kmp_cache = cp;
	kmem_panic_info.kmp_buffer = buf;
	kmem_panic_info.kmp_bufctl = bcp;

	prom_printf("kernel memory allocator: ");

	switch (error) {

	    case KMERR_MODIFIED:

		prom_printf("buffer modified after being freed\n");
		off = verify_pattern(KMEM_FREE_PATTERN, buf, cp->cache_offset);
		if (off == NULL)	/* shouldn't happen */
			off = buf;
		prom_printf("modification occurred at offset 0x%x "
			"(0x%x replaced by 0x%x)\n",
			(int)off - (int)buf, KMEM_FREE_PATTERN, *off);
		break;

	    case KMERR_REDZONE:

		prom_printf("redzone violation: write past end of buffer\n");
		break;

	    case KMERR_BADADDR:

		bcp = kmem_locate(buf);
		if (bcp && bcp->bc_slab->slab_cache != cp) {
			kmem_panic_info.kmp_error = KMERR_BADCACHE;
			prom_printf("buffer freed to wrong cache\n");
			prom_printf("buffer was allocated from cache %s,\n",
				bcp->bc_slab->slab_cache->cache_name);
			prom_printf("caller attempting free to cache %s.\n",
				cp->cache_name);
		} else {
			prom_printf("invalid free: buffer not in cache\n");
		}
		break;

	    case KMERR_DUPFREE:

		prom_printf("duplicate free: buffer freed twice\n");
		break;

	    case KMERR_BADBUFTAG:

		prom_printf("boundary tag corrupted\n");
		prom_printf("bcp ^ bxstat = %x, should be %x\n",
			(intptr_t)bcp ^ btp->bt_bxstat, KMEM_BUFTAG_FREE);
		bcp = kmem_locate(buf);
		break;

	    case KMERR_BADBUFCTL:

		prom_printf("bufctl corrupted\n");
		prom_printf("bcp->bc_addr = %x, should be %x\n",
			bcp->bc_addr, buf);
		bcp = kmem_locate(buf);
		break;

	    case KMERR_WRONGSIZE:

		prom_printf("bad free: free size (%d) != alloc size (%d)\n",
			btp->bt_redzone[-2], btp->bt_redzone[-1]);
		break;

	}

	kmem_panic_info.kmp_realbcp = bcp;

	prom_printf("buffer=%x  bufctl=%x  cache: %s\n",
		buf, bcp, cp->cache_name);

	if (cp->cache_flags & KMF_AUDIT) {
		prom_printf("previous transaction on buffer %x:\n", buf);
		if (bcp != NULL)
			kmem_bufctl_display((kmem_bufctl_audit_t *)bcp);
	}
	if (kmem_panic)
		cmn_err(CE_PANIC, "kernel heap corruption detected");
	debug_enter(NULL);
	kmem_logging = 1;	/* resume logging */
}

/*
 * Get pages from the back-end page allocator.
 */
static void *
kmem_page_alloc(kmem_backend_t *bep, int npages, int flags)
{
	void *pages;
	size_t size = npages << bep->be_pageshift;
	int bucket = highbit(size - 1);

	KMEM_RANDOM_ALLOCATION_FAILURE(flags, kmem_page_alloc_fail);

	if (bep->be_memclass == KMEM_CLASS_WIRED) {
		/*
		 * XXX -- this logic should be moved into segkmem
		 * as soon as tsreddy's segkmem rewrite is complete.
		 * The only reason for putting it here now is to
		 * avoid a bunch of code replication.  Note that
		 * this code implicitly assumes small pages.
		 */
		mutex_enter(&freemem_lock);
		while (availrmem < tune.t_minarmem + npages) {
			if (flags & KM_NOSLEEP) {
				mutex_exit(&freemem_lock);
				return (NULL);
			}
			/*
			 * We're out of memory.  It would be nice if there
			 * were something appropriate to cv_wait() for,
			 * but there are currently many ways for pages to
			 * come and go -- there's no reliable, centralized
			 * notification mechanism.  So, we just hang out
			 * for a moment, give pageout a chance to run,
			 * and try again.  It's lame, but this situation is
			 * rare in practice -- all we're really trying to do
			 * here is unwedge the system if it gets stuck.
			 */
			needfree += npages;
			mutex_exit(&freemem_lock);
			kmem_reap();
			delay(hz >> 2);
			mutex_enter(&freemem_lock);
			needfree -= npages;
		}
		availrmem -= npages;
		mutex_exit(&freemem_lock);
	}

	if ((pages = bep->be_page_alloc(npages, flags | KM_NOSLEEP)) == NULL) {
		kmem_reap();
		if ((pages = bep->be_page_alloc(npages, flags)) == NULL) {
			ASSERT(flags & KM_NOSLEEP);
			if (bep->be_memclass == KMEM_CLASS_WIRED) {
				mutex_enter(&freemem_lock);
				availrmem += npages;
				mutex_exit(&freemem_lock);
			}
			return (NULL);
		}
	}

	mutex_enter(&kmem_backend_lock);
	bep->be_alloc[bucket]++;
	bep->be_inuse[bucket] += size;
	bep->be_pages_inuse += npages;
	if (bep->be_memclass == KMEM_CLASS_WIRED)
		kmem_misc_kstat.arena_size.value.l += size;
	mutex_exit(&kmem_backend_lock);

	return (pages);
}

/*
 * Return pages to the back-end page allocator.
 */
static void
kmem_page_free(kmem_backend_t *bep, void *pages, int npages)
{
	size_t size = npages << bep->be_pageshift;
	int bucket = highbit(size - 1);

	bep->be_page_free(pages, npages);

	if (bep->be_memclass == KMEM_CLASS_WIRED) {
		/*
		 * XXX -- as above, this really belongs in segkmem
		 */
		mutex_enter(&freemem_lock);
		availrmem += npages;
		mutex_exit(&freemem_lock);
	}

	mutex_enter(&kmem_backend_lock);
	bep->be_inuse[bucket] -= size;
	bep->be_pages_inuse -= npages;
	if (bep->be_memclass == KMEM_CLASS_WIRED)
		kmem_misc_kstat.arena_size.value.l -= size;
	mutex_exit(&kmem_backend_lock);
}

u_long
kmem_avail(void)
{
	int rmem = (int)(availrmem - tune.t_minarmem);
	int fmem = (int)(freemem - minfree);
	int pages_avail = min(max(min(rmem, fmem), 0), 1 << (30 - PAGESHIFT));

	return (ptob(pages_avail));
}

/*
 * Return the maximum amount of memory that is (in theory) allocatable
 * from the heap. This may be used as an estimate only since there
 * is no guarentee this space will still be available when an allocation
 * request is made, nor that the space may be allocated in one big request
 * due to kernelmap fragmentation.
 */
u_longlong_t
kmem_maxavail(void)
{
	int max_phys = (int)(availrmem - tune.t_minarmem);
	int max_virt = (int)kmem_maxvirt();
	int pages_avail = max(min(max_phys, max_virt), 0);

	return ((u_longlong_t)pages_avail << PAGESHIFT);
}

/*
 * Allocate memory permanently.
 */
void *
kmem_perm_alloc(size_t size, int align, int flags)
{
	kmem_perm_t *pp, **prev_ppp, **best_prev_ppp;
	char *buf;
	int best_avail = INT_MAX;

	KMEM_RANDOM_ALLOCATION_FAILURE(flags,
		kmem_misc_kstat.perm_alloc_fail.value.l);

	if (align < KMEM_ALIGN)
		align = KMEM_ALIGN;
	if ((align & (align - 1)) || align > PAGESIZE)
		cmn_err(CE_PANIC, "kmem_perm_alloc: bad alignment %d", align);
	size = (size + align - 1) & -align;

	mutex_enter(&kmem_perm_lock);
	kmem_misc_kstat.perm_alloc.value.l++;
	best_prev_ppp = NULL;
	for (prev_ppp = &kmem_perm_freelist; (pp = *prev_ppp) != NULL;
	    prev_ppp = &pp->perm_next) {
		if (pp->perm_avail - (-(int)pp->perm_current & (align - 1)) >=
		    size && pp->perm_avail < best_avail) {
			best_prev_ppp = prev_ppp;
			best_avail = pp->perm_avail;
		}
	}
	if ((prev_ppp = best_prev_ppp) == NULL) {
		int npages = btopr(size + sizeof (kmem_perm_t));
		mutex_exit(&kmem_perm_lock);
		buf = kmem_page_alloc(&kmem_default_backend, npages, flags);
		if (buf == NULL) {
			kmem_misc_kstat.perm_alloc_fail.value.l++;
			return (NULL);
		}
		mutex_enter(&kmem_perm_lock);
		kmem_misc_kstat.perm_size.value.l += ptob(npages);
		pp = (kmem_perm_t *)buf;
		pp->perm_next = kmem_perm_freelist;
		pp->perm_current = buf + sizeof (kmem_perm_t);
		pp->perm_avail = ptob(npages) - sizeof (kmem_perm_t);
		kmem_perm_freelist = pp;
		prev_ppp = &kmem_perm_freelist;
	}
	pp = *prev_ppp;
	buf = (char *)(((intptr_t)pp->perm_current + align - 1) & -align);
	pp->perm_avail = pp->perm_avail - (buf + size - pp->perm_current);
	pp->perm_current = buf + size;
	if (pp->perm_avail < KMEM_PERM_MINFREE)
		*prev_ppp = pp->perm_next;
	mutex_exit(&kmem_perm_lock);
	return (buf);
}

static int
kmem_backend_kstat_snapshot(kstat_t *ksp, void *buf, int rw)
{
	int bucket;
	kstat_named_t *knp = buf;
	kmem_backend_t *bep = ksp->ks_private;
	static const char mem_units[5] = "BKMG";

	ksp->ks_snaptime = gethrtime();

	if (rw == KSTAT_WRITE)
		return (EACCES);

	kstat_named_init(&knp[0], "memory_class", KSTAT_DATA_ULONG);
	kstat_named_init(&knp[1], "active_clients", KSTAT_DATA_ULONG);
	kstat_named_init(&knp[2], "page_size", KSTAT_DATA_ULONG);
	kstat_named_init(&knp[3], "bytes_inuse", KSTAT_DATA_ULONG);
	knp[0].value.ul = bep->be_memclass;
	knp[1].value.ul = bep->be_clients;
	knp[2].value.ul = bep->be_pagesize;
	knp[3].value.ul = bep->be_pages_inuse << bep->be_pageshift;
	knp += 4;
	for (bucket = bep->be_pageshift; bucket < KMEM_PAGE_BUCKETS; bucket++) {
		sprintf(knp->name, "alloc_%d%c",
			1 << (bucket % 10), mem_units[bucket / 10]);
		knp->data_type = KSTAT_DATA_ULONG;
		knp->value.ul = bep->be_alloc[bucket];
		knp++;
		sprintf(knp->name, "inuse_%d%c",
			1 << (bucket % 10), mem_units[bucket / 10]);
		knp->data_type = KSTAT_DATA_ULONG;
		knp->value.ul = bep->be_inuse[bucket];
		knp++;
		if (bep->be_alloc[bucket] != 0 || bep->be_inuse[bucket] != 0)
			ksp->ks_ndata = knp - (kstat_named_t *)buf;
	}
	return (0);
}

static void
kmem_backend_kstat_create(kmem_backend_t *bep, char *name)
{
	if ((bep->be_kstat = kstat_create("unix", 0, name, "kmem_backend",
	    KSTAT_TYPE_NAMED,
	    (KMEM_PAGE_BUCKETS - bep->be_pageshift) * 2 + 4,
	    KSTAT_FLAG_VIRTUAL)) != NULL) {
		bep->be_kstat->ks_snapshot = kmem_backend_kstat_snapshot;
		bep->be_kstat->ks_private = bep;
		kstat_install(bep->be_kstat);
	}
}

kmem_backend_t *
kmem_backend_create(
	char *name,		/* descriptive name for this backend */
	void *(*page_alloc)(int, int),	/* page allocation routine */
	void (*page_free)(void *, int),	/* page free routine */
	int pagesize,		/* backend pagesize */
	int memclass)		/* backend memory class */
{
	kmem_backend_t *bep = kmem_zalloc(sizeof (kmem_backend_t), KM_SLEEP);

	bep->be_page_alloc = page_alloc;
	bep->be_page_free = page_free;
	bep->be_pagesize = pagesize;
	bep->be_pageshift = highbit(pagesize - 1);
	bep->be_memclass = memclass;

	kmem_backend_kstat_create(bep, name);

	return (bep);
}

void
kmem_backend_destroy(kmem_backend_t *bep)
{
	ASSERT(bep->be_clients == 0);
	ASSERT(bep->be_pages_inuse == 0);
	if (bep->be_kstat)
		kstat_delete(bep->be_kstat);
	kmem_free(bep, sizeof (kmem_backend_t));
}

void *
kmem_backend_alloc(kmem_backend_t *bep, size_t size, int flags)
{
	int npages = (size + bep->be_pagesize - 1) >> bep->be_pageshift;
	void *pages;
	kmem_pagectl_t *pcp, **hash_bucket;

	if (size == 0)
		return (NULL);

	kmem_misc_kstat.huge_alloc.value.l++;

	if ((pcp = kmem_cache_alloc(kmem_pagectl_cache, flags)) == NULL) {
		kmem_misc_kstat.huge_alloc_fail.value.l++;
		return (NULL);
	}

	if ((pages = kmem_page_alloc(bep, npages, flags)) == NULL) {
		kmem_cache_free(kmem_pagectl_cache, pcp);
		kmem_misc_kstat.huge_alloc_fail.value.l++;
		return (NULL);
	}

	pcp->pc_addr = pages;
	pcp->pc_size = size;
	pcp->pc_backend = bep;

	mutex_enter(&kmem_backend_lock);
	hash_bucket = KMEM_PAGECTL_HASH(bep, pages);
	pcp->pc_next = *hash_bucket;
	*hash_bucket = pcp;
	if (bep->be_memclass == KMEM_CLASS_WIRED)
		kmem_misc_kstat.huge_size.value.l +=
			npages << bep->be_pageshift;
	mutex_exit(&kmem_backend_lock);

	return (pages);
}

void
kmem_backend_free(kmem_backend_t *bep, void *pages, size_t size)
{
	int npages = (size + bep->be_pagesize - 1) >> bep->be_pageshift;
	kmem_pagectl_t *pcp, **prev_pcpp;

	if (pages == NULL && size == 0)
		return;

	mutex_enter(&kmem_backend_lock);
	prev_pcpp = KMEM_PAGECTL_HASH(bep, pages);
	while ((pcp = *prev_pcpp) != NULL) {
		if (pcp->pc_addr == pages && pcp->pc_backend == bep) {
			*prev_pcpp = pcp->pc_next;
			break;
		}
		prev_pcpp = &pcp->pc_next;
	}
	if (bep->be_memclass == KMEM_CLASS_WIRED)
		kmem_misc_kstat.huge_size.value.l -=
			npages << bep->be_pageshift;
	mutex_exit(&kmem_backend_lock);

	if (pcp == NULL)
		cmn_err(CE_PANIC, "kmem_backend_free('%s', %x, %d): "
		    "no such allocation", bep->be_kstat->ks_name, pages, size);

	if (pcp->pc_size != size)
		cmn_err(CE_PANIC, "kmem_backend_free('%s', %x, %d): "
		    "wrong size (alloc was %d)", bep->be_kstat->ks_name, pages,
		    size, pcp->pc_size);

	kmem_cache_free(kmem_pagectl_cache, pcp);
	kmem_page_free(bep, pages, npages);
}

static kmem_slab_t *
kmem_slab_create(kmem_cache_t *cp, int flags)
{
	int chunksize = cp->cache_chunksize;
	int cache_flags = cp->cache_flags;
	int color, chunks;
	char *buf, *base, *limit;
	kmem_slab_t *sp;
	kmem_bufctl_t *bcp;
	kmem_backend_t *bep = cp->cache_backend;

	ASSERT(MUTEX_HELD(&cp->cache_lock));

	TRACE_2(TR_FAC_KMEM, TR_KMEM_SLAB_CREATE_START,
		"kmem_slab_create_start:cache %S flags %x",
		cp->cache_name, flags);

	if ((color = cp->cache_color += cp->cache_align) > cp->cache_maxcolor)
		color = cp->cache_color = 0;

	mutex_exit(&cp->cache_lock);

	if ((base = kmem_page_alloc(bep,
	    cp->cache_slabsize >> bep->be_pageshift, flags)) == NULL)
		goto page_alloc_failure;
	ASSERT(((int)base & (bep->be_pagesize - 1)) == 0);

	if (cache_flags & KMF_DEADBEEF)
		copy_pattern(KMEM_FREE_PATTERN, base, cp->cache_slabsize);

	if (cache_flags & KMF_HASH) {
		limit = base + cp->cache_slabsize;
		if ((sp = kmem_cache_alloc(kmem_slab_cache, flags)) == NULL)
			goto slab_alloc_failure;
	} else {
		limit = base + cp->cache_slabsize - sizeof (kmem_slab_t);
		sp = (kmem_slab_t *)limit;
	}

	sp->slab_cache	= cp;
	sp->slab_head	= NULL;
	sp->slab_refcnt	= 0;
	sp->slab_base	= buf = base + color;

	chunks = 0;
	while (buf + chunksize <= limit) {
		if (cache_flags & KMF_HASH) {
			bcp = kmem_cache_alloc(cp->cache_bufctl_cache, flags);
			if (bcp == NULL)
				goto bufctl_alloc_failure;
			if (cache_flags & KMF_AUDIT)
				bzero(bcp, sizeof (kmem_bufctl_audit_t));
			bcp->bc_addr = buf;
			bcp->bc_slab = sp;
			bcp->bc_cache = cp;
		} else {
			bcp = (kmem_bufctl_t *)(buf + cp->cache_offset);
		}
		if (cache_flags & KMF_BUFTAG) {
			kmem_buftag_t *btp = KMEM_BUFTAG(cp, buf);
			copy_pattern(KMEM_REDZONE_PATTERN, btp->bt_redzone,
				sizeof (btp->bt_redzone));
			btp->bt_bufctl = bcp;
			btp->bt_bxstat = (intptr_t)bcp ^ KMEM_BUFTAG_FREE;
		}
		bcp->bc_next = sp->slab_head;
		sp->slab_head = bcp;
		if (chunks == 0)
			sp->slab_tail = bcp;
		buf += chunksize;
		chunks++;
	}
	sp->slab_chunks = chunks;

	mutex_enter(&cp->cache_lock);

	cp->cache_slab_create++;
	cp->cache_buftotal += sp->slab_chunks;
	if (cp->cache_buftotal > cp->cache_bufmax)
		cp->cache_bufmax = cp->cache_buftotal;

	TRACE_1(TR_FAC_KMEM, TR_KMEM_SLAB_CREATE_END,
		"kmem_slab_create_end:slab %x", sp);

	return (sp);

bufctl_alloc_failure:
	while ((bcp = sp->slab_head) != NULL) {
		sp->slab_head = bcp->bc_next;
		kmem_cache_free(cp->cache_bufctl_cache, bcp);
	}
	kmem_cache_free(kmem_slab_cache, sp);

slab_alloc_failure:
	kmem_page_free(bep, base, cp->cache_slabsize >> bep->be_pageshift);

page_alloc_failure:
	mutex_enter(&cp->cache_lock);

	TRACE_1(TR_FAC_KMEM, TR_KMEM_SLAB_CREATE_END,
		"kmem_slab_create_end:slab %x", sp);

	return (NULL);
}

static void
kmem_slab_destroy(kmem_cache_t *cp, kmem_slab_t *sp)
{
	kmem_backend_t *bep = cp->cache_backend;

	ASSERT(MUTEX_HELD(&cp->cache_lock));

	TRACE_2(TR_FAC_KMEM, TR_KMEM_SLAB_DESTROY_START,
		"kmem_slab_destroy_start:cache %S slab %x", cp->cache_name, sp);

	cp->cache_slab_destroy++;
	cp->cache_buftotal -= sp->slab_chunks;

	mutex_exit(&cp->cache_lock);

	kmem_page_free(bep,
		(void *)((intptr_t)sp->slab_base & -bep->be_pagesize),
		cp->cache_slabsize >> bep->be_pageshift);

	if (cp->cache_flags & KMF_HASH) {
		kmem_bufctl_t *bcp;
		sp->slab_tail->bc_next = NULL;	/* normally a garbage pointer */
		while ((bcp = sp->slab_head) != NULL) {
			sp->slab_head = bcp->bc_next;
			kmem_cache_free(cp->cache_bufctl_cache, bcp);
		}
		kmem_cache_free(kmem_slab_cache, sp);
	}

	mutex_enter(&cp->cache_lock);

	TRACE_0(TR_FAC_KMEM, TR_KMEM_SLAB_DESTROY_END, "kmem_slab_destroy_end");
}

static kmem_log_header_t *
kmem_log_init(size_t logsize)
{
	kmem_log_header_t *lhp;
	int nchunks = 4 * max_ncpus;
	size_t lhsize = (size_t)&((kmem_log_header_t *)0)->lh_cpu[max_ncpus];
	int i;

	/*
	 * Make sure that lhp->lh_cpu[] is nicely aligned
	 * to prevent false sharing of cache lines.
	 */
	lhp = kmem_perm_alloc(roundup(lhsize, 64), 64, KM_SLEEP);
	lhp = (kmem_log_header_t *)((char *)lhp + (-lhsize & 63));
	bzero(lhp, lhsize);

	mutex_init(&lhp->lh_lock, "kmem_log_%x", MUTEX_DEFAULT, NULL);
	lhp->lh_nchunks = nchunks;
	lhp->lh_chunksize = roundup(logsize / nchunks + 1, PAGESIZE);
	lhp->lh_base = kmem_zalloc(lhp->lh_chunksize * nchunks, KM_SLEEP);
	lhp->lh_free = kmem_alloc(nchunks * sizeof (int), KM_SLEEP);

	for (i = 0; i < max_ncpus; i++) {
		kmem_cpu_log_header_t *clhp = &lhp->lh_cpu[i];
		mutex_init(&clhp->clh_lock, "kmem_log_%x", MUTEX_DEFAULT, NULL);
		clhp->clh_chunk = i;
	}

	for (i = max_ncpus; i < nchunks; i++)
		lhp->lh_free[i] = i;

	lhp->lh_head = max_ncpus;
	lhp->lh_tail = 0;

	return (lhp);
}

static void *
kmem_log_enter(kmem_log_header_t *lhp, void *data, size_t size)
{
	void *logspace;
	kmem_cpu_log_header_t *clhp = &lhp->lh_cpu[CPU->cpu_seqid];

	if (lhp == NULL || kmem_logging == 0 || panicstr)
		return (NULL);

	mutex_enter(&clhp->clh_lock);
	clhp->clh_hits++;
	if (size > clhp->clh_avail) {
		mutex_enter(&lhp->lh_lock);
		lhp->lh_hits++;
		lhp->lh_free[lhp->lh_tail] = clhp->clh_chunk;
		lhp->lh_tail = (lhp->lh_tail + 1) % lhp->lh_nchunks;
		clhp->clh_chunk = lhp->lh_free[lhp->lh_head];
		lhp->lh_head = (lhp->lh_head + 1) % lhp->lh_nchunks;
		clhp->clh_current = lhp->lh_base +
			clhp->clh_chunk * lhp->lh_chunksize;
		clhp->clh_avail = lhp->lh_chunksize;
		if (size > lhp->lh_chunksize)
			size = lhp->lh_chunksize;
		mutex_exit(&lhp->lh_lock);
	}
	logspace = clhp->clh_current;
	clhp->clh_current += size;
	clhp->clh_avail -= size;
	bcopy(data, logspace, size);
	mutex_exit(&clhp->clh_lock);
	return (logspace);
}

static int
kmem_cache_alloc_debug(kmem_cache_t *cp, void *buf, int flags, int doconst)
{
	kmem_buftag_t *btp = KMEM_BUFTAG(cp, buf);
	kmem_bufctl_t *bcp = btp->bt_bufctl;
	int retval = 0;

	if (btp->bt_bxstat != ((intptr_t)bcp ^ KMEM_BUFTAG_FREE)) {
		kmem_error(KMERR_BADBUFTAG, cp, buf, bcp);
		return (retval);
	}
	btp->bt_bxstat = (intptr_t)bcp ^ KMEM_BUFTAG_ALLOC;

	if ((cp->cache_flags & KMF_HASH) != 0 && bcp->bc_addr != buf) {
		kmem_error(KMERR_BADBUFCTL, cp, buf, bcp);
		return (retval);
	}
	if ((cp->cache_flags & (KMF_HASH | KMF_REDZONE)) == KMF_REDZONE) {
		/*
		 * If the cache doesn't have external bufctls, freelist
		 * linkage overwrites the first word of the redzone; fix.
		 */
		btp->bt_redzone[1] = btp->bt_redzone[0] = KMEM_REDZONE_PATTERN;
	}
	if (cp->cache_flags & KMF_DEADBEEF) {
		if (verify_and_copy_pattern(KMEM_FREE_PATTERN,
		    KMEM_UNINITIALIZED_PATTERN, buf, cp->cache_offset) != NULL)
			kmem_error(KMERR_MODIFIED, cp, buf, bcp);
		if (doconst && cp->cache_constructor != NULL) {
			retval = cp->cache_constructor(buf,
			    cp->cache_private, flags);
		}
	}
	if (cp->cache_flags & KMF_AUDIT) {
		kmem_bufctl_audit_t *bcap = (kmem_bufctl_audit_t *)bcp;
		bcap->bc_timestamp = gethrtime();
		bcap->bc_thread = curthread;
		bcap->bc_depth = getpcstack(bcap->bc_stack, KMEM_STACK_DEPTH);
		bcap->bc_lastlog = kmem_log_enter(kmem_transaction_log,
			bcap, sizeof (kmem_bufctl_audit_t));
	}

	return (retval);
}

static int
kmem_cache_free_debug(kmem_cache_t *cp, void *buf, int dodest, size_t exactsize)
{
	kmem_buftag_t *btp = KMEM_BUFTAG(cp, buf);
	kmem_bufctl_t *bcp = btp->bt_bufctl;

	if (btp->bt_bxstat != ((intptr_t)bcp ^ KMEM_BUFTAG_ALLOC)) {
		if (btp->bt_bxstat == ((intptr_t)bcp ^ KMEM_BUFTAG_FREE)) {
			kmem_error(KMERR_DUPFREE, cp, buf, bcp);
			return (-1);
		}
		bcp = kmem_locate(buf);
		if (bcp == NULL || bcp->bc_slab->slab_cache != cp)
			kmem_error(KMERR_BADADDR, cp, buf, NULL);
		else
			kmem_error(KMERR_REDZONE, cp, buf, bcp);
		return (-1);
	}
	btp->bt_bxstat = (intptr_t)bcp ^ KMEM_BUFTAG_FREE;

	if ((cp->cache_flags & KMF_HASH) != 0 && bcp->bc_addr != buf) {
		kmem_error(KMERR_BADBUFCTL, cp, buf, bcp);
		return (-1);
	}
	if (cp->cache_flags & KMF_REDZONE) {
		if (exactsize != 0) {
			int rzsize = (exactsize + KMEM_ALIGN - 1) & -KMEM_ALIGN;
			if (rzsize != cp->cache_offset &&
			    btp->bt_redzone[-1] != exactsize) {
				btp->bt_redzone[-2] = exactsize;
				kmem_error(KMERR_WRONGSIZE, cp, buf, bcp);
				return (-1);
			}
			btp->bt_redzone[-1] = KMEM_UNINITIALIZED_PATTERN;
			if ((cp->cache_flags & KMF_DEADBEEF) &&
			    verify_pattern(KMEM_UNINITIALIZED_PATTERN,
			    (char *)buf + rzsize,
			    cp->cache_offset - rzsize) != NULL) {
				kmem_error(KMERR_REDZONE, cp, buf, bcp);
				return (-1);
			}
		}
		if (verify_pattern(KMEM_REDZONE_PATTERN, btp->bt_redzone,
		    sizeof (btp->bt_redzone)) != NULL) {
			kmem_error(KMERR_REDZONE, cp, buf, bcp);
			return (-1);
		}
	}
	if (cp->cache_flags & KMF_AUDIT) {
		kmem_bufctl_audit_t *bcap = (kmem_bufctl_audit_t *)bcp;
		bcap->bc_timestamp = gethrtime();
		bcap->bc_thread = curthread;
		bcap->bc_depth = getpcstack(bcap->bc_stack, KMEM_STACK_DEPTH);
		if (cp->cache_flags & KMF_CONTENTS)
			bcap->bc_contents = kmem_log_enter(kmem_content_log,
			    buf, min(cp->cache_offset, kmem_content_maxsave));
		bcap->bc_lastlog = kmem_log_enter(kmem_transaction_log,
			bcap, sizeof (kmem_bufctl_audit_t));
	}
	if (cp->cache_flags & KMF_DEADBEEF) {
		if (dodest && cp->cache_destructor != NULL)
			cp->cache_destructor(buf, cp->cache_private);
		copy_pattern(KMEM_FREE_PATTERN, buf, cp->cache_offset);
	}

	return (0);
}

/*
 * To make the magazine layer as fast as possible, we don't check for
 * kmem debugging flags in production (non-DEBUG) kernels.  We can get
 * equivalent debugging functionality in the field by setting the
 * KMF_NOMAGAZINE flag in addition to any others; that causes the
 * allocator to bypass the magazine layer entirely and go straight
 * to the global layer, which always checks the flags.  This is not
 * satisfactory for internal testing, however, because we also want
 * to stress the magazine code itself; the #defines below enable
 * magazine-layer debugging in DEBUG kernels.
 */
#ifdef DEBUG

#define	KMEM_CACHE_ALLOC_DEBUG(cp, buf, flags, doconst)	\
	if (cp->cache_flags & KMF_BUFTAG) \
		if (kmem_cache_alloc_debug(cp, buf, flags, doconst)) { \
			mutex_enter(&cp->cache_lock); \
			cp->cache_alloc_fail++; \
			mutex_exit(&cp->cache_lock); \
			kmem_cache_free_global(cp, buf); \
			buf = NULL; \
		}
#define	KMEM_CACHE_FREE_DEBUG(cp, buf, exactsize)	\
	if (cp->cache_flags & KMF_BUFTAG) \
		if (kmem_cache_free_debug(cp, buf, 1, exactsize)) \
			return;
#define	KMEM_CACHE_DESTRUCTOR(cp, buf)	\
	if (!(cp->cache_flags & KMF_DEADBEEF) && cp->cache_destructor != NULL) \
		cp->cache_destructor(buf, cp->cache_private);
#define	KMEM_PRECISE_REDZONE_SETUP(cp, buf, size)	\
	if ((cp->cache_flags & KMF_REDZONE) && \
	    cp->cache_offset - size >= KMEM_ALIGN && buf != NULL) \
		KMEM_BUFTAG(cp, buf)->bt_redzone[-1] = size;

#else

#define	KMEM_CACHE_ALLOC_DEBUG(cp, buf, flags, doconst)
#define	KMEM_CACHE_FREE_DEBUG(cp, buf, exactsize)
#define	KMEM_CACHE_DESTRUCTOR(cp, buf)	\
	if (cp->cache_destructor != NULL) \
		cp->cache_destructor(buf, cp->cache_private);
#define	KMEM_PRECISE_REDZONE_SETUP(cp, buf, size)

#endif

static void *
kmem_cache_alloc_global(kmem_cache_t *cp, int flags)
{
	void *buf;
	kmem_slab_t *sp, *snext, *sprev;
	kmem_slab_t *extra_slab = NULL;
	kmem_bufctl_t *bcp, **hash_bucket;

	KMEM_RANDOM_ALLOCATION_FAILURE(flags, cp->cache_alloc_fail);

	cp = cp->cache_active;

	TRACE_2(TR_FAC_KMEM, TR_KMEM_CACHE_ALLOC_START,
		"kmem_cache_alloc_start:cache %S flags %x",
		cp->cache_name, flags);

	mutex_enter(&cp->cache_lock);
	cp->cache_alloc++;
	sp = cp->cache_freelist;
	ASSERT(sp->slab_cache == cp);
	if ((bcp = sp->slab_head) == sp->slab_tail) {
		if (bcp == NULL) {
			/*
			 * The freelist is empty.  Create a new slab.
			 */
			if ((sp = kmem_slab_create(cp, flags)) == NULL) {
				cp->cache_alloc_fail++;
				mutex_exit(&cp->cache_lock);
				TRACE_1(TR_FAC_KMEM, TR_KMEM_CACHE_ALLOC_END,
					"kmem_cache_alloc_end:buf %x", NULL);
				return (NULL);
			}
			if (cp->cache_freelist == &cp->cache_nullslab) {
				/*
				 * Add slab to tail of freelist
				 */
				sp->slab_next = snext = &cp->cache_nullslab;
				sp->slab_prev = sprev = snext->slab_prev;
				snext->slab_prev = sp;
				sprev->slab_next = sp;
				cp->cache_freelist = sp;
			} else {
				extra_slab = sp;
				sp = cp->cache_freelist;
			}
		}
		/*
		 * If this is last buf in slab, remove slab from free list
		 */
		if ((bcp = sp->slab_head) == sp->slab_tail) {
			cp->cache_freelist = sp->slab_next;
			sp->slab_tail = NULL;
		}
	}

	sp->slab_head = bcp->bc_next;
	sp->slab_refcnt++;
	ASSERT(sp->slab_refcnt <= sp->slab_chunks);

	if (cp->cache_flags & KMF_HASH) {
		/*
		 * add buf to allocated-address hash table
		 */
		buf = bcp->bc_addr;
		hash_bucket = KMEM_HASH(cp, buf);
		bcp->bc_next = *hash_bucket;
		*hash_bucket = bcp;
	} else {
		buf = (void *)((char *)bcp - cp->cache_offset);
	}

	ASSERT((uintptr_t)buf - (uintptr_t)sp->slab_base < cp->cache_slabsize);

	if (extra_slab)
		kmem_slab_destroy(cp, extra_slab);

	mutex_exit(&cp->cache_lock);

	if (cp->cache_flags & KMF_BUFTAG)
		(void) kmem_cache_alloc_debug(cp, buf, flags, 0);

	TRACE_1(TR_FAC_KMEM, TR_KMEM_CACHE_ALLOC_END,
		"kmem_cache_alloc_end:buf %x", buf);

	return (buf);
}

static void
kmem_cache_free_global(kmem_cache_t *cp, void *buf)
{
	kmem_slab_t *sp, *snext, *sprev;
	kmem_bufctl_t *bcp, **prev_bcpp, *old_slab_tail;
	kmem_cache_t *dcp;

	TRACE_2(TR_FAC_KMEM, TR_KMEM_CACHE_FREE_START,
		"kmem_cache_free_start:cache %S buf %x", cp->cache_name, buf);

	ASSERT(buf != NULL);

	if ((dcp = cp->cache_debug) != NULL) {
		mutex_enter(&dcp->cache_lock);
		for (bcp = *KMEM_HASH(dcp, buf); bcp; bcp = bcp->bc_next)
			if (bcp->bc_addr == buf)
				break;
		mutex_exit(&dcp->cache_lock);
		if (bcp != NULL)	/* buffer came from debug cache */
			cp = dcp;
	}

	if (cp->cache_flags & KMF_BUFTAG)
		if (kmem_cache_free_debug(cp, buf, 0, 0))
			return;

	mutex_enter(&cp->cache_lock);

	if (cp->cache_flags & KMF_HASH) {
		/*
		 * look up buf in allocated-address hash table
		 */
		prev_bcpp = KMEM_HASH(cp, buf);
		while ((bcp = *prev_bcpp) != NULL) {
			if (bcp->bc_addr == buf) {
				*prev_bcpp = bcp->bc_next;
				sp = bcp->bc_slab;
				break;
			}
			cp->cache_lookup_depth++;
			prev_bcpp = &bcp->bc_next;
		}
	} else {
		bcp = (kmem_bufctl_t *)((char *)buf + cp->cache_offset);
		sp = (kmem_slab_t *)((((intptr_t)buf) &
		    -cp->cache_backend->be_pagesize) +
		    (cp->cache_backend->be_pagesize - sizeof (kmem_slab_t)));
	}

	if (bcp == NULL || sp->slab_cache != cp ||
	    (uintptr_t)buf - (uintptr_t)sp->slab_base >= cp->cache_slabsize) {
		mutex_exit(&cp->cache_lock);
		if (dcp && kmem_cache_free_debug(dcp, buf, 0, 0))
			return;
		kmem_error(KMERR_BADADDR, cp, buf, NULL);
		return;
	}

	old_slab_tail = sp->slab_tail;
	sp->slab_tail = bcp;
	if (old_slab_tail == NULL) {
		/*
		 * Return slab to head of free list
		 */
		sp->slab_head = bcp;
		if ((snext = sp->slab_next) != cp->cache_freelist) {
			snext->slab_prev = sprev = sp->slab_prev;
			sprev->slab_next = snext;
			sp->slab_next = snext = cp->cache_freelist;
			sp->slab_prev = sprev = snext->slab_prev;
			sprev->slab_next = sp;
			snext->slab_prev = sp;
		}
		cp->cache_freelist = sp;
	} else {
		old_slab_tail->bc_next = bcp;
	}
	ASSERT(sp->slab_refcnt >= 1);
	if (--sp->slab_refcnt == 0) {
		/*
		 * There are no outstanding allocations from this slab,
		 * so we can reclaim the memory.
		 */
		snext = sp->slab_next;
		sprev = sp->slab_prev;
		snext->slab_prev = sprev;
		sprev->slab_next = snext;
		if (sp == cp->cache_freelist)
			cp->cache_freelist = snext;
		kmem_slab_destroy(cp, sp);
	}
	mutex_exit(&cp->cache_lock);

	TRACE_0(TR_FAC_KMEM, TR_KMEM_CACHE_FREE_END, "kmem_cache_free_end");
}

void *
kmem_cache_alloc(kmem_cache_t *cp, int flags)
{
	void *buf;
	kmem_cpu_cache_t *ccp;
	kmem_magazine_t *mp, *fmp;
	int rounds;

	KMEM_RANDOM_ALLOCATION_FAILURE(flags, cp->cache_alloc_fail);

	TRACE_2(TR_FAC_KMEM, TR_KMEM_CACHE_ALLOC_START,
		"kmem_cache_alloc_start:cache %S flags %x",
		cp->cache_name, flags);

	ccp = KMEM_CPU_CACHE(cp);
	mutex_enter(&ccp->cc_lock);
	rounds = ccp->cc_rounds - 1;
	mp = ccp->cc_loaded_mag;
	if (rounds >= 0) {
		ccp->cc_rounds = rounds;
		ccp->cc_alloc++;
		buf = mp->mag_round[rounds];
		mutex_exit(&ccp->cc_lock);
		TRACE_1(TR_FAC_KMEM, TR_KMEM_CACHE_ALLOC_END,
			"kmem_cache_alloc_end:buf %x", buf);
		KMEM_CACHE_ALLOC_DEBUG(cp, buf, flags, 1);
		return (buf);
	}
	if ((fmp = ccp->cc_full_mag) != NULL) {
		ASSERT(ccp->cc_empty_mag == NULL);
		ccp->cc_alloc++;
		ccp->cc_empty_mag = mp;
		ccp->cc_loaded_mag = fmp;
		ccp->cc_full_mag = NULL;
		ccp->cc_rounds = ccp->cc_magsize - 1;
		buf = fmp->mag_round[ccp->cc_magsize - 1];
		mutex_exit(&ccp->cc_lock);
		TRACE_1(TR_FAC_KMEM, TR_KMEM_CACHE_ALLOC_END,
			"kmem_cache_alloc_end:buf %x", buf);
		KMEM_CACHE_ALLOC_DEBUG(cp, buf, flags, 1);
		return (buf);
	}
	if (ccp->cc_magsize > 0) {
		if (!mutex_tryenter(&cp->cache_depot_lock)) {
			mutex_enter(&cp->cache_depot_lock);
			cp->cache_depot_contention++;
		}
		if ((fmp = cp->cache_fmag_list) != NULL) {
			cp->cache_fmag_list = fmp->mag_next;
			if (--cp->cache_fmag_total < cp->cache_fmag_min)
				cp->cache_fmag_min = cp->cache_fmag_total;
			if (mp != NULL) {
				if (ccp->cc_empty_mag == NULL) {
					ccp->cc_empty_mag = mp;
				} else {
					mp->mag_next = cp->cache_emag_list;
					cp->cache_emag_list = mp;
					cp->cache_emag_total++;
				}
			}
			cp->cache_depot_alloc++;
			mutex_exit(&cp->cache_depot_lock);
			ccp->cc_loaded_mag = fmp;
			ccp->cc_rounds = ccp->cc_magsize - 1;
			buf = fmp->mag_round[ccp->cc_magsize - 1];
			mutex_exit(&ccp->cc_lock);
			TRACE_1(TR_FAC_KMEM, TR_KMEM_CACHE_ALLOC_END,
				"kmem_cache_alloc_end:buf %x", buf);
			KMEM_CACHE_ALLOC_DEBUG(cp, buf, flags, 1);
			return (buf);
		}
		mutex_exit(&cp->cache_depot_lock);
	}
	mutex_exit(&ccp->cc_lock);
	buf = kmem_cache_alloc_global(cp, flags);
	if (buf != NULL && cp->cache_constructor != NULL) {
		if (cp->cache_constructor(buf, cp->cache_private, flags)) {
			mutex_enter(&cp->cache_lock);
			cp->cache_alloc_fail++;
			mutex_exit(&cp->cache_lock);
			kmem_cache_free_global(cp, buf);
			buf = NULL;
		}
	}
	TRACE_1(TR_FAC_KMEM, TR_KMEM_CACHE_ALLOC_END,
		"kmem_cache_alloc_end:buf %x", buf);
	return (buf);
}

void
kmem_cache_free(kmem_cache_t *cp, void *buf)
{
	kmem_cpu_cache_t *ccp;
	kmem_magazine_t *mp, *emp;
	u_int rounds;

	TRACE_2(TR_FAC_KMEM, TR_KMEM_CACHE_FREE_START,
		"kmem_cache_free_start:cache %S buf %x", cp->cache_name, buf);
	KMEM_CACHE_FREE_DEBUG(cp, buf, 0);

	ccp = KMEM_CPU_CACHE(cp);
	mutex_enter(&ccp->cc_lock);
	rounds = (u_int)ccp->cc_rounds;
	mp = ccp->cc_loaded_mag;
	if (rounds < ccp->cc_magsize) {
		ccp->cc_rounds = rounds + 1;
		ccp->cc_free++;
		mp->mag_round[rounds] = buf;
		mutex_exit(&ccp->cc_lock);
		TRACE_0(TR_FAC_KMEM, TR_KMEM_CACHE_FREE_END,
			"kmem_cache_free_end");
		return;
	}
	if ((emp = ccp->cc_empty_mag) != NULL) {
		ASSERT(ccp->cc_full_mag == NULL);
		ccp->cc_free++;
		ccp->cc_full_mag = mp;
		ccp->cc_loaded_mag = emp;
		ccp->cc_empty_mag = NULL;
		ccp->cc_rounds = 1;
		emp->mag_round[0] = buf;
		mutex_exit(&ccp->cc_lock);
		TRACE_0(TR_FAC_KMEM, TR_KMEM_CACHE_FREE_END,
			"kmem_cache_free_end");
		return;
	}
	if (ccp->cc_magsize > 0) {
		kmem_cache_t *mcp;
		if (!mutex_tryenter(&cp->cache_depot_lock)) {
			mutex_enter(&cp->cache_depot_lock);
			cp->cache_depot_contention++;
		}
		if ((emp = cp->cache_emag_list) != NULL) {
			cp->cache_emag_list = emp->mag_next;
			if (--cp->cache_emag_total < cp->cache_emag_min)
				cp->cache_emag_min = cp->cache_emag_total;
			if (mp != NULL) {
				if (ccp->cc_full_mag == NULL) {
					ccp->cc_full_mag = mp;
				} else {
					mp->mag_next = cp->cache_fmag_list;
					cp->cache_fmag_list = mp;
					cp->cache_fmag_total++;
				}
			}
			cp->cache_depot_free++;
			mutex_exit(&cp->cache_depot_lock);
			ccp->cc_loaded_mag = emp;
			ccp->cc_rounds = 1;
			emp->mag_round[0] = buf;
			mutex_exit(&ccp->cc_lock);
			TRACE_0(TR_FAC_KMEM, TR_KMEM_CACHE_FREE_END,
				"kmem_cache_free_end");
			return;
		}
		/*
		 * There are no magazines in the depot.  Since this is a
		 * very rare code path, and reestablishing all the invariants
		 * after dropping locks is tricky, we instead do the simplest
		 * possible thing: allocate a new magazine and leave it in
		 * the depot for benefit of the *next* kmem_cache_free().
		 * This one will just fall through to the global layer.
		 */
		mcp = cp->cache_magazine_cache;
		mutex_exit(&cp->cache_depot_lock);
		mutex_exit(&ccp->cc_lock);
		emp = kmem_cache_alloc_global(mcp, KM_NOSLEEP);
		mutex_enter(&ccp->cc_lock);
		mutex_enter(&cp->cache_depot_lock);
		if (emp != NULL && ccp->cc_magsize > 0 &&
		    cp->cache_magazine_cache == mcp) {
			emp->mag_next = cp->cache_emag_list;
			cp->cache_emag_list = emp;
			cp->cache_emag_total++;
			emp = NULL;
		}
		mutex_exit(&cp->cache_depot_lock);
		if (emp != NULL) {
			mutex_exit(&ccp->cc_lock);
			kmem_cache_free_global(mcp, emp);
			mutex_enter(&ccp->cc_lock);
		}
	}
	mutex_exit(&ccp->cc_lock);
	KMEM_CACHE_ALLOC_DEBUG(cp, buf, 0, 0);
	KMEM_CACHE_DESTRUCTOR(cp, buf);
	kmem_cache_free_global(cp, buf);
	TRACE_0(TR_FAC_KMEM, TR_KMEM_CACHE_FREE_END, "kmem_cache_free_end");
}

void *
kmem_zalloc(size_t size, int flags)
{
	void *buf;
	int index = (int)(size - 1) >> KMEM_ALIGN_SHIFT;

	TRACE_2(TR_FAC_KMEM, TR_KMEM_ZALLOC_START,
		"kmem_zalloc_start:size %d flags %x", size, flags);

	if ((u_int)index < KMEM_MAXBUF >> KMEM_ALIGN_SHIFT) {
		kmem_cache_t *cp = kmem_alloc_table[index];
		kmem_cpu_cache_t *ccp = KMEM_CPU_CACHE(cp);
		kmem_magazine_t *mp;
		int rounds;
		KMEM_RANDOM_ALLOCATION_FAILURE(flags, cp->cache_alloc_fail);
		mutex_enter(&ccp->cc_lock);
		rounds = ccp->cc_rounds - 1;
		mp = ccp->cc_loaded_mag;
		if (rounds >= 0) {
			ccp->cc_rounds = rounds;
			ccp->cc_alloc++;
			buf = mp->mag_round[rounds];
			mutex_exit(&ccp->cc_lock);
			KMEM_CACHE_ALLOC_DEBUG(cp, buf, 0, 0);
			if (size <= KMEM_BZERO_INLINE) {
				int32_t *Xbuf = (int32_t *)buf;
				int32_t *Xend = (int32_t *)((char *)buf + size);
				while (Xbuf < Xend) {
					*Xbuf++ = 0;
					*Xbuf++ = 0;
				}
			} else {
				bzero(buf, size);
			}
		} else {
			mutex_exit(&ccp->cc_lock);
			if ((buf = kmem_cache_alloc(cp, flags)) != NULL)
				bzero(buf, size);
		}
		KMEM_PRECISE_REDZONE_SETUP(cp, buf, size);
	} else {
		buf = kmem_backend_alloc(&kmem_default_backend, size, flags);
		if (buf != NULL)
			bzero(buf, size);
	}

	TRACE_1(TR_FAC_KMEM, TR_KMEM_ZALLOC_END, "kmem_zalloc_end:buf %x", buf);

	return (buf);
}

void *
kmem_alloc(size_t size, int flags)
{
	void *buf;
	int index = (int)(size - 1) >> KMEM_ALIGN_SHIFT;

	TRACE_2(TR_FAC_KMEM, TR_KMEM_ALLOC_START,
		"kmem_alloc_start:size %d flags %x", size, flags);

	if ((u_int)index < KMEM_MAXBUF >> KMEM_ALIGN_SHIFT) {
		kmem_cache_t *cp = kmem_alloc_table[index];
		kmem_cpu_cache_t *ccp = KMEM_CPU_CACHE(cp);
		kmem_magazine_t *mp;
		int rounds;
		KMEM_RANDOM_ALLOCATION_FAILURE(flags, cp->cache_alloc_fail);
		mutex_enter(&ccp->cc_lock);
		rounds = ccp->cc_rounds - 1;
		mp = ccp->cc_loaded_mag;
		if (rounds >= 0) {
			ccp->cc_rounds = rounds;
			ccp->cc_alloc++;
			buf = mp->mag_round[rounds];
			mutex_exit(&ccp->cc_lock);
			KMEM_CACHE_ALLOC_DEBUG(cp, buf, 0, 0);
		} else {
			mutex_exit(&ccp->cc_lock);
			buf = kmem_cache_alloc(cp, flags);
		}
		KMEM_PRECISE_REDZONE_SETUP(cp, buf, size);
	} else {
		buf = kmem_backend_alloc(&kmem_default_backend, size, flags);
	}

	TRACE_1(TR_FAC_KMEM, TR_KMEM_ALLOC_END, "kmem_alloc_end:buf %x", buf);

	return (buf);
}

void
kmem_free(void *buf, size_t size)
{
	int index = (size - 1) >> KMEM_ALIGN_SHIFT;

	TRACE_2(TR_FAC_KMEM, TR_KMEM_FREE_START,
		"kmem_free_start:buf %x size %d", buf, size);

	if ((u_int)index < KMEM_MAXBUF >> KMEM_ALIGN_SHIFT) {
		kmem_cache_t *cp = kmem_alloc_table[index];
		kmem_cpu_cache_t *ccp = KMEM_CPU_CACHE(cp);
		kmem_magazine_t *mp;
		u_int rounds;
		KMEM_CACHE_FREE_DEBUG(cp, buf, size);
		mutex_enter(&ccp->cc_lock);
		rounds = (u_int)ccp->cc_rounds;
		mp = ccp->cc_loaded_mag;
		if (rounds < ccp->cc_magsize) {
			ccp->cc_rounds = rounds + 1;
			ccp->cc_free++;
			mp->mag_round[rounds] = buf;
			mutex_exit(&ccp->cc_lock);
		} else {
			mutex_exit(&ccp->cc_lock);
			KMEM_CACHE_ALLOC_DEBUG(cp, buf, 0, 0);
			kmem_cache_free(cp, buf);
		}
	} else {
		kmem_backend_free(&kmem_default_backend, buf, size);
	}

	TRACE_0(TR_FAC_KMEM, TR_KMEM_FREE_END, "kmem_free_end");
}

/*
 * XXX -- remove this legacy code as soon as the Platform consolidation
 * gets their drivers cleaned up.  Alternatively, provide fixed versions
 * of the affected binaries the day this wad goes in.
 */
#define	KMEM_FAST_ALLOC_SUPPORTED	/* XXX -- only for a build or two */
#ifdef KMEM_FAST_ALLOC_SUPPORTED

#undef kmem_fast_alloc
#undef kmem_fast_zalloc
#undef kmem_fast_free

/* ARGSUSED */
void *
kmem_fast_alloc(char **base, size_t size, int chunks, int flags)
{
	void *buf;

	if ((buf = *base) == NULL)
		return (kmem_perm_alloc(size, KMEM_ALIGN, flags));
	*base = *(void **)buf;
	return (buf);
}

void *
kmem_fast_zalloc(char **base, size_t size, int chunks, int flags)
{
	void *buf;

	if ((buf = kmem_fast_alloc(base, size, chunks, flags)) != NULL)
		bzero(buf, size);
	return (buf);
}

void
kmem_fast_free(char **base, void *buf)
{
	*(void **)buf = *base;
	*base = buf;
}

#endif

static void
kmem_magazine_destroy(kmem_cache_t *cp, kmem_magazine_t *mp, int nrounds)
{
	if (mp != NULL) {
		int round;
		for (round = 0; round < nrounds; round++) {
			void *buf = mp->mag_round[round];
			KMEM_CACHE_ALLOC_DEBUG(cp, buf, 0, 0);
			KMEM_CACHE_DESTRUCTOR(cp, buf);
			kmem_cache_free_global(cp, buf);
		}
		kmem_cache_free_global(cp->cache_magazine_cache, mp);
	}
}

static void
kmem_async_dispatch(void (*func)(kmem_cache_t *), kmem_cache_t *cp, int flags)
{
	kmem_async_t *ap, *anext, *aprev;

	TRACE_3(TR_FAC_KMEM, TR_KMEM_ASYNC_DISPATCH_START,
		"kmem_async_dispatch_start:%K(%S) flags %x",
		func, cp->cache_name, flags);

	mutex_enter(&kmem_async_lock);
	if ((ap = kmem_async_freelist) == NULL) {
		mutex_exit(&kmem_async_lock);
		if ((ap = kmem_perm_alloc(sizeof (*ap), 0, flags)) == NULL)
			return;
		mutex_enter(&kmem_async_lock);
	} else {
		kmem_async_freelist = ap->async_next;
	}
	ap->async_next = anext = &kmem_async_queue;
	ap->async_prev = aprev = kmem_async_queue.async_prev;
	aprev->async_next = ap;
	anext->async_prev = ap;
	ap->async_func = func;
	ap->async_cache = cp;
	cv_signal(&kmem_async_cv);
	mutex_exit(&kmem_async_lock);

	TRACE_1(TR_FAC_KMEM, TR_KMEM_ASYNC_DISPATCH_END,
		"kmem_async_dispatch_end:async_entry %x", ap);
}

/*
 * Reclaim all unused memory from a cache.
 */
static void
kmem_cache_reap(kmem_cache_t *cp)
{
	int reaplimit;
	kmem_magazine_t *mp, *fmp;
	kmem_cpu_cache_t *ccp;
	void *buf = NULL;

	ASSERT(MUTEX_HELD(&kmem_async_serialize));

	/*
	 * Ask the cache's owner to free some memory if possible.
	 * The idea is to handle things like the inode cache, which
	 * typically sits on a bunch of memory that it doesn't truly
	 * *need*.  Reclaim policy is entirely up to the owner; this
	 * callback is just an advisory plea for help.
	 */
	if (cp->cache_reclaim != NULL)
		cp->cache_reclaim(cp->cache_private);

	/*
	 * We want to ensure that unused buffers scattered across multiple
	 * cpus will eventually coalesce into complete slabs.  Each time
	 * this routine runs it selects a victim cpu, returns its full
	 * magazine to the depot, and returns one buffer from its loaded
	 * magazine to the global layer.  Eventually all unused memory
	 * is reclaimed.
	 */
	mutex_enter(&cp->cache_depot_lock);
	if (++cp->cache_cpu_rotor >= ncpus)
		cp->cache_cpu_rotor = 0;
	ccp = &cp->cache_cpu[cp->cache_cpu_rotor];
	mutex_exit(&cp->cache_depot_lock);

	mutex_enter(&ccp->cc_lock);
	if ((fmp = ccp->cc_full_mag) != NULL) {
		ccp->cc_full_mag = NULL;
		mutex_enter(&cp->cache_depot_lock);
		fmp->mag_next = cp->cache_fmag_list;
		cp->cache_fmag_list = fmp;
		cp->cache_fmag_total++;
		mutex_exit(&cp->cache_depot_lock);
	}
	if (ccp->cc_rounds > 0) {
		ccp->cc_alloc++;
		buf = ccp->cc_loaded_mag->mag_round[--ccp->cc_rounds];
	}
	mutex_exit(&ccp->cc_lock);

	if (buf != NULL) {
		KMEM_CACHE_ALLOC_DEBUG(cp, buf, 0, 0);
		KMEM_CACHE_DESTRUCTOR(cp, buf);
		kmem_cache_free_global(cp, buf);
	}

	mutex_enter(&cp->cache_depot_lock);
	reaplimit = min(cp->cache_fmag_reaplimit, cp->cache_fmag_min);
	cp->cache_fmag_reaplimit = 0;
	while (--reaplimit >= 0 && (mp = cp->cache_fmag_list) != NULL) {
		cp->cache_fmag_total--;
		cp->cache_fmag_min--;
		cp->cache_fmag_list = mp->mag_next;
		mutex_exit(&cp->cache_depot_lock);
		kmem_magazine_destroy(cp, mp, cp->cache_magazine_size);
		mutex_enter(&cp->cache_depot_lock);
	}
	reaplimit = min(cp->cache_emag_reaplimit, cp->cache_emag_min);
	cp->cache_emag_reaplimit = 0;
	while (--reaplimit >= 0 && (mp = cp->cache_emag_list) != NULL) {
		cp->cache_emag_total--;
		cp->cache_emag_min--;
		cp->cache_emag_list = mp->mag_next;
		mutex_exit(&cp->cache_depot_lock);
		kmem_magazine_destroy(cp, mp, 0);
		mutex_enter(&cp->cache_depot_lock);
	}
	mutex_exit(&cp->cache_depot_lock);
}

/*
 * Reclaim all unused memory from all caches.  Called from the VM system
 * when memory gets tight.
 */
void
kmem_reap(void)
{
	kmem_cache_t *cp;

	if ((int)(lbolt - kmem_reap_lasttime) < kmem_reap_interval)
		return;
	kmem_reap_lasttime = lbolt;

	mutex_enter(&kmem_cache_lock);
	for (cp = kmem_null_cache.cache_next; cp != &kmem_null_cache;
	    cp = cp->cache_next)
		kmem_async_dispatch(kmem_cache_reap, cp, KM_NOSLEEP);
	mutex_exit(&kmem_cache_lock);
}

/*
 * Purge all magazines from a cache and set its magazine limit to zero.
 * All calls are serialized by kmem_async_serialize.
 */
static void
kmem_cache_magazine_purge(kmem_cache_t *cp)
{
	kmem_cpu_cache_t *ccp;
	kmem_magazine_t *mp, *fmp, *emp;
	int rounds, magsize, cpu_seqid;

	ASSERT(MUTEX_HELD(&kmem_async_serialize));
	ASSERT(MUTEX_NOT_HELD(&cp->cache_lock));

	for (cpu_seqid = 0; cpu_seqid < cp->cache_ncpus; cpu_seqid++) {
		ccp = &cp->cache_cpu[cpu_seqid];

		mutex_enter(&ccp->cc_lock);
		rounds = ccp->cc_rounds;
		magsize = ccp->cc_magsize;
		mp = ccp->cc_loaded_mag;
		fmp = ccp->cc_full_mag;
		emp = ccp->cc_empty_mag;
		ccp->cc_rounds = -1;
		ccp->cc_magsize = 0;
		ccp->cc_loaded_mag = NULL;
		ccp->cc_full_mag = NULL;
		ccp->cc_empty_mag = NULL;
		mutex_exit(&ccp->cc_lock);

		kmem_magazine_destroy(cp, mp, rounds);
		kmem_magazine_destroy(cp, fmp, magsize);
		kmem_magazine_destroy(cp, emp, 0);
	}

	mutex_enter(&cp->cache_depot_lock);
	cp->cache_fmag_min = cp->cache_fmag_reaplimit = cp->cache_fmag_total;
	cp->cache_emag_min = cp->cache_emag_reaplimit = cp->cache_emag_total;
	mutex_exit(&cp->cache_depot_lock);

	kmem_cache_reap(cp);
}

/*
 * Enable per-cpu magazines on a cache.
 */
static void
kmem_cache_magazine_enable(kmem_cache_t *cp, kmem_cache_t *mcp)
{
	int cpu_seqid;

	ASSERT(MUTEX_HELD(&kmem_async_serialize) || !cp->cache_magazine_size);

	if (cp->cache_flags & KMF_NOMAGAZINE)
		return;

	mutex_enter(&cp->cache_depot_lock);
	cp->cache_magazine_cache = mcp;
	cp->cache_magazine_size = mcp->cache_bufsize / sizeof (void *) - 1;
	cp->cache_depot_contention_last = cp->cache_depot_contention + INT_MAX;
	mutex_exit(&cp->cache_depot_lock);

	for (cpu_seqid = 0; cpu_seqid < cp->cache_ncpus; cpu_seqid++) {
		kmem_cpu_cache_t *ccp = &cp->cache_cpu[cpu_seqid];
		mutex_enter(&ccp->cc_lock);
		ccp->cc_magsize = cp->cache_magazine_size;
		mutex_exit(&ccp->cc_lock);
	}

}

/*
 * Recompute a cache's magazine size.  The trade-off is that larger magazines
 * provide a higher transfer rate with the global layer, while smaller
 * magazines reduce memory consumption.  Magazine resizing is an expensive
 * operation; it should not be done frequently.  Changes to the magazine size
 * are serialized by kmem_async_serialize.
 *
 * Note: at present this only grows the magazine size.  It might be useful
 * to allow shrinkage too.
 */
static void
kmem_magazine_resize(kmem_cache_t *cp)
{
	ASSERT(MUTEX_HELD(&kmem_async_serialize));

	if (cp->cache_magazine_size < cp->cache_magazine_maxsize) {
		kmem_cache_t *mcp = cp->cache_magazine_cache->cache_next;
		kmem_cache_magazine_purge(cp);
		kmem_cache_magazine_enable(cp, mcp);
	}
}

/*
 * Rescale a cache's hash table, so that the table size is roughly the
 * cache size.  We want the average lookup time to be extremely small.
 */
static void
kmem_hash_rescale(kmem_cache_t *cp)
{
	int old_size, new_size, h, rescale;
	kmem_bufctl_t **old_table, **new_table, *bcp;

	ASSERT(MUTEX_HELD(&kmem_async_serialize));

	TRACE_2(TR_FAC_KMEM, TR_KMEM_HASH_RESCALE_START,
		"kmem_hash_rescale_start:cache %S buftotal %d",
		cp->cache_name, cp->cache_buftotal);

	new_size = max(KMEM_MIN_HASH_SIZE,
		1 << (highbit(3 * cp->cache_buftotal + 4) - 2));
	if ((new_table = kmem_zalloc(new_size * sizeof (kmem_bufctl_t *),
	    KM_NOSLEEP)) == NULL)
		return;

	mutex_enter(&cp->cache_lock);

	old_size = cp->cache_hash_mask + 1;
	old_table = cp->cache_hash_table;

	cp->cache_hash_mask = new_size - 1;
	cp->cache_hash_table = new_table;

	for (h = 0; h < old_size; h++) {
		bcp = old_table[h];
		while (bcp != NULL) {
			void *addr = bcp->bc_addr;
			kmem_bufctl_t *next_bcp = bcp->bc_next;
			kmem_bufctl_t **hash_bucket = KMEM_HASH(cp, addr);
			bcp->bc_next = *hash_bucket;
			*hash_bucket = bcp;
			bcp = next_bcp;
		}
	}
	rescale = cp->cache_rescale++;

	mutex_exit(&cp->cache_lock);

	if (rescale == 0)	/* if old_table is the initial table */
		kmem_page_free(&kmem_default_backend, old_table, 1);
	else
		kmem_free(old_table, old_size * sizeof (kmem_bufctl_t *));

	TRACE_0(TR_FAC_KMEM, TR_KMEM_HASH_RESCALE_END, "kmem_hash_rescale_end");
}

/*
 * Perform periodic maintenance on a cache: hash rescaling,
 * depot working-set update, and magazine resizing.
 */
static void
kmem_cache_update(kmem_cache_t *cp)
{
	int need_hash_rescale = 0;
	int need_magazine_resize = 0;

	mutex_enter(&cp->cache_lock);

	/*
	 * If the cache has become much larger or smaller than its hash table,
	 * fire off a request to rescale the hash table.
	 */
	if ((cp->cache_flags & KMF_HASH) &&
	    (cp->cache_buftotal > (cp->cache_hash_mask << 1) ||
	    (cp->cache_buftotal < (cp->cache_hash_mask >> 1) &&
	    cp->cache_hash_mask > KMEM_MIN_HASH_SIZE)))
		need_hash_rescale = 1;

	mutex_enter(&cp->cache_depot_lock);

	/*
	 * Update the depot working set sizes
	 */
	cp->cache_fmag_reaplimit = cp->cache_fmag_min;
	cp->cache_fmag_min = cp->cache_fmag_total;

	cp->cache_emag_reaplimit = cp->cache_emag_min;
	cp->cache_emag_min = cp->cache_emag_total;

	/*
	 * If there's a lot of contention in the depot,
	 * increase the magazine size.
	 */
	if (cp->cache_magazine_size < cp->cache_magazine_maxsize &&
	    cp->cache_depot_contention - cp->cache_depot_contention_last >
	    kmem_depot_contention)
		need_magazine_resize = 1;

	cp->cache_depot_contention_last = cp->cache_depot_contention;

	mutex_exit(&cp->cache_depot_lock);
	mutex_exit(&cp->cache_lock);

	if (need_hash_rescale)
		kmem_async_dispatch(kmem_hash_rescale, cp, KM_NOSLEEP);
	if (need_magazine_resize)
		kmem_async_dispatch(kmem_magazine_resize, cp, KM_NOSLEEP);
}

static void
kmem_cache_debug_enable(kmem_cache_t *cp)
{
	ASSERT(MUTEX_HELD(&kmem_async_serialize));

	if ((cp->cache_flags & KMF_BUFTAG) ||
	    (cp->cache_cflags & (KMC_NOHASH | KMC_NODEBUG | KMC_NOTOUCH)) ||
	    !(kmem_flags & KMF_BUFTAG))
		return;

	if (cp->cache_debug == NULL) {
		char name[40];
		sprintf(name, "%s.DEBUG", cp->cache_name);
		cp->cache_debug = kmem_cache_create(name, cp->cache_bufsize,
			cp->cache_align, cp->cache_constructor,
			cp->cache_destructor, cp->cache_reclaim,
			cp->cache_private, cp->cache_backend,
			cp->cache_cflags | KMC_NOMAGAZINE);
	}

	kmem_cache_magazine_purge(cp);

	if ((cp->cache_debug->cache_flags & KMF_AUDIT) &&
	    kmem_transaction_log == NULL)
		kmem_transaction_log = kmem_log_init(kmem_log_size);

	if ((cp->cache_debug->cache_flags & KMF_CONTENTS) &&
	    kmem_content_log == NULL)
		kmem_content_log = kmem_log_init(kmem_log_size);

	cp->cache_active = cp->cache_debug;
}

static void
kmem_cache_debug_disable(kmem_cache_t *cp)
{
	ASSERT(MUTEX_HELD(&kmem_async_serialize));

	if (cp->cache_active != cp) {
		cp->cache_active = cp;
		kmem_cache_magazine_enable(cp, cp->cache_magazine_cache);
	}
}

static void
kmem_update(void *dummy)
{
	kmem_cache_t *cp;

	mutex_enter(&kmem_cache_lock);
	for (cp = kmem_null_cache.cache_next; cp != &kmem_null_cache;
	    cp = cp->cache_next) {
		if (kmem_debug_enable == cp ||
		    kmem_debug_enable == (void *)-1)
			kmem_async_dispatch(kmem_cache_debug_enable, cp,
				KM_NOSLEEP);
		if (kmem_debug_disable == cp ||
		    kmem_debug_disable == (void *)-1)
			kmem_async_dispatch(kmem_cache_debug_disable, cp,
				KM_NOSLEEP);
		kmem_cache_update(cp);
	}
	kmem_debug_enable = NULL;
	kmem_debug_disable = NULL;
	mutex_exit(&kmem_cache_lock);

	/*
	 * XXX -- Check to see if the system is out of kernelmap.
	 * This gets an 'XXX' because the allocator shouldn't
	 * know about such things.  This really belongs in the
	 * rmap code -- when a resource map runs low, it should
	 * call kmem_reap().
	 */
	if (mapwant(kernelmap))
		kmem_reap();

	(void) timeout(kmem_update, dummy, kmem_reap_interval);
}

static int
kmem_cache_kstat_update(kstat_t *ksp, int rw)
{
	struct kmem_cache_kstat *kmcp;
	kmem_cache_t *cp;
	kmem_slab_t *sp;
	int cpu_buf_avail;
	int buf_avail = 0;
	int cpu_seqid;

	if (rw == KSTAT_WRITE)
		return (EACCES);

	kmcp = kmem_cache_kstat;
	cp = ksp->ks_private;

	kmcp->kmc_alloc_fail.value.l		= cp->cache_alloc_fail;
	kmcp->kmc_alloc.value.l			= cp->cache_alloc;
	kmcp->kmc_global_alloc.value.l		= cp->cache_alloc;

	for (cpu_seqid = 0; cpu_seqid < cp->cache_ncpus; cpu_seqid++) {
		kmem_cpu_cache_t *ccp = &cp->cache_cpu[cpu_seqid];
		struct kmem_cpu_kstat *kmcpup = &kmem_cpu_kstat[cpu_seqid];

		mutex_enter(&ccp->cc_lock);

		cpu_buf_avail = 0;
		if (ccp->cc_rounds > 0)
			cpu_buf_avail += ccp->cc_rounds;
		if (ccp->cc_full_mag)
			cpu_buf_avail += ccp->cc_magsize;

		kmcpup->kmcpu_alloc_from.value.l	= ccp->cc_alloc;
		kmcpup->kmcpu_free_to.value.l		= ccp->cc_free;
		kmcpup->kmcpu_buf_avail.value.l		= cpu_buf_avail;

		kmcp->kmc_alloc.value.l			+= ccp->cc_alloc;
		buf_avail				+= cpu_buf_avail;

		mutex_exit(&ccp->cc_lock);
	}

	mutex_enter(&cp->cache_depot_lock);

	kmcp->kmc_depot_alloc.value.l		= cp->cache_depot_alloc;
	kmcp->kmc_depot_free.value.l		= cp->cache_depot_free;
	kmcp->kmc_depot_contention.value.l	= cp->cache_depot_contention;
	kmcp->kmc_empty_magazines.value.l	= cp->cache_emag_total;
	kmcp->kmc_full_magazines.value.l	= cp->cache_fmag_total;
	kmcp->kmc_magazine_size.value.l		= cp->cache_magazine_size;

	kmcp->kmc_alloc.value.l			+= cp->cache_depot_alloc;
	buf_avail += cp->cache_fmag_total * cp->cache_magazine_size;

	mutex_exit(&cp->cache_depot_lock);

	kmcp->kmc_buf_size.value.l	= cp->cache_bufsize;
	kmcp->kmc_align.value.l		= cp->cache_align;
	kmcp->kmc_chunk_size.value.l	= cp->cache_chunksize;
	kmcp->kmc_slab_size.value.l	= cp->cache_slabsize;
	if (cp->cache_constructor != NULL && !(cp->cache_flags & KMF_DEADBEEF))
		kmcp->kmc_buf_constructed.value.l = buf_avail;
	else
		kmcp->kmc_buf_constructed.value.l = 0;
	for (sp = cp->cache_freelist; sp != &cp->cache_nullslab;
	    sp = sp->slab_next)
		buf_avail += sp->slab_chunks - sp->slab_refcnt;
	kmcp->kmc_buf_avail.value.l	= buf_avail;
	kmcp->kmc_buf_total.value.l	= cp->cache_buftotal;
	kmcp->kmc_buf_max.value.l	= cp->cache_bufmax;
	kmcp->kmc_slab_create.value.l	= cp->cache_slab_create;
	kmcp->kmc_slab_destroy.value.l	= cp->cache_slab_destroy;
	kmcp->kmc_hash_size.value.l	= (cp->cache_hash_mask + 1) & -2;
	kmcp->kmc_hash_lookup_depth.value.l = cp->cache_lookup_depth;
	kmcp->kmc_hash_rescale.value.l	= cp->cache_rescale;
	kmcp->kmc_memory_class.value.l	= cp->cache_backend->be_memclass;
	return (0);
}

static void
kmem_cache_create_finish(kmem_cache_t *cp)
{
	ASSERT(MUTEX_HELD(&kmem_async_serialize));

	if (cp->cache_ncpus > ncpus) {
		/*
		 * We over-allocated cache_cpu[] in kmem_cache_create()
		 * because we didn't know ncpus at that time.  Now we do
		 * so return the unused memory to kmem_perm_freelist.
		 */
		int size, cpu_seqid;

		for (cpu_seqid = ncpus; cpu_seqid < cp->cache_ncpus;
		    cpu_seqid++)
			mutex_destroy(&cp->cache_cpu[cpu_seqid].cc_lock);

		size = (cp->cache_ncpus - ncpus) * sizeof (kmem_cpu_cache_t);

		if (size >= sizeof (kmem_perm_t) + KMEM_PERM_MINFREE) {
			char *buf = (char *)&cp->cache_cpu[ncpus];
			kmem_perm_t *pp = (kmem_perm_t *)buf;

			mutex_enter(&kmem_perm_lock);
			kmem_misc_kstat.perm_size.value.l += size;
			pp->perm_next = kmem_perm_freelist;
			pp->perm_current = buf + sizeof (kmem_perm_t);
			pp->perm_avail = size - sizeof (kmem_perm_t);
			kmem_perm_freelist = pp;
			mutex_exit(&kmem_perm_lock);
		}
		cp->cache_ncpus = ncpus;
	}

	if ((cp->cache_kstat = kstat_create("unix", 0, cp->cache_name,
	    "kmem_cache", KSTAT_TYPE_NAMED,
	    sizeof (struct kmem_cache_kstat) / sizeof (kstat_named_t) +
	    sizeof (struct kmem_cpu_kstat) * ncpus / sizeof (kstat_named_t),
	    KSTAT_FLAG_VIRTUAL)) != NULL) {
		cp->cache_kstat->ks_data = kmem_cache_kstat;
		cp->cache_kstat->ks_update = kmem_cache_kstat_update;
		cp->cache_kstat->ks_private = cp;
		cp->cache_kstat->ks_lock = &cp->cache_lock;
		kstat_install(cp->cache_kstat);
	}
}

kmem_cache_t *
kmem_cache_create(
	char *name,		/* descriptive name for this cache */
	size_t bufsize,		/* size of the objects it manages */
	int align,		/* required object alignment */
	int (*constructor)(void *, void *, int), /* object constructor */
	void (*destructor)(void *, void *),	/* object destructor */
	void (*reclaim)(void *), /* memory reclaim callback */
	void *private,		/* pass-thru arg for constr/destr/reclaim */
	kmem_backend_t *bep,	/* back-end pointer */
	int cflags)		/* cache creation flags */
{
	int cpu_seqid, chunksize;
	kmem_cache_t *cp, *cnext, *cprev;
	kmem_magazine_type_t *mtp;
	char namebuf[64];
	int csize;

	if (bep == NULL)
		bep = &kmem_default_backend;

	mutex_enter(&kmem_backend_lock);
	bep->be_clients++;
	mutex_exit(&kmem_backend_lock);

	mutex_enter(&kmem_cache_lock);
	if ((cp = kmem_cache_freelist) == NULL) {
		mutex_exit(&kmem_cache_lock);
		/*
		 * Make sure that cp->cache_cpu[] is nicely aligned
		 * to prevent false sharing of cache lines.
		 */
		csize = KMEM_CACHE_SIZE(max_ncpus);
		cp = kmem_perm_alloc(roundup(csize, KMEM_CPU_CACHE_SIZE),
			KMEM_CPU_CACHE_SIZE, KM_SLEEP);
		cp = (kmem_cache_t *)((char *)cp +
			(-csize & (KMEM_CPU_CACHE_SIZE - 1)));
		bzero(cp, csize);
		cp->cache_ncpus = max_ncpus;
	} else {
		kmem_cache_freelist = cp->cache_next;
		mutex_exit(&kmem_cache_lock);
		bzero(cp, KMEM_CACHE_SIZE(ncpus));
		cp->cache_ncpus = ncpus;
	}

	strncpy(cp->cache_name, name, KMEM_CACHE_NAMELEN);
	mutex_init(&cp->cache_lock, name, MUTEX_DEFAULT, NULL);
	sprintf(namebuf, "%s_depot", name);
	mutex_init(&cp->cache_depot_lock, namebuf, MUTEX_DEFAULT, NULL);

	for (cpu_seqid = 0; cpu_seqid < cp->cache_ncpus; cpu_seqid++) {
		kmem_cpu_cache_t *ccp = &cp->cache_cpu[cpu_seqid];
		sprintf(namebuf, "%s_cpu_cache_%d", name, cpu_seqid);
		mutex_init(&ccp->cc_lock, namebuf, MUTEX_ADAPTIVE, NULL);
		ccp->cc_rounds = -1;	/* no current magazine */
	}

	ASSERT(!(cflags & KMC_NOHASH) || !(cflags & KMC_NOTOUCH));

	if (!(cflags & (KMC_NODEBUG | KMC_NOTOUCH)))
		cp->cache_flags = kmem_flags;

	if (cflags & KMC_NOMAGAZINE)
		cp->cache_flags |= KMF_NOMAGAZINE;

	if (cp->cache_flags & KMF_PAGEPERBUF)
		align = bep->be_pagesize;

	if (align < kmem_align)
		align = kmem_align;

	if ((align & (align - 1)) || align > bep->be_pagesize)
		cmn_err(CE_PANIC, "kmem_cache_create: bad alignment %d", align);

	chunksize = (bufsize + (KMEM_ALIGN - 1)) & -KMEM_ALIGN;
	cp->cache_offset = chunksize - KMEM_ALIGN;
	if (cp->cache_flags & KMF_BUFTAG) {
		cp->cache_offset = chunksize;
		chunksize += sizeof (kmem_buftag_t);
	}
	chunksize = (chunksize + align - 1) & -align;

	cp->cache_bufsize	= bufsize;
	cp->cache_chunksize	= chunksize;
	cp->cache_align		= align;
	cp->cache_constructor	= constructor;
	cp->cache_destructor	= destructor;
	cp->cache_reclaim	= reclaim;
	cp->cache_private	= private;
	cp->cache_backend	= bep;
	cp->cache_freelist	= &cp->cache_nullslab;
	cp->cache_debug		= NULL;
	cp->cache_active	= cp;
	cp->cache_cflags	= cflags;
	cp->cache_nullslab.slab_cache = cp;
	cp->cache_nullslab.slab_refcnt = -1;
	cp->cache_nullslab.slab_next = &cp->cache_nullslab;
	cp->cache_nullslab.slab_prev = &cp->cache_nullslab;

	for (mtp = kmem_magazine_type; chunksize <= mtp->mt_maxbuf; mtp++)
		continue;
	cp->cache_magazine_maxsize = mtp->mt_magsize;

	for (mtp = kmem_magazine_type; chunksize <= mtp->mt_minbuf; mtp++)
		continue;
	kmem_cache_magazine_enable(cp, mtp->mt_cache);

	if ((cflags & KMC_NOHASH) || (!(cflags & KMC_NOTOUCH) &&
	    !(cp->cache_flags & KMF_BUFTAG) && chunksize < kmem_minhash)) {
		cp->cache_slabsize = bep->be_pagesize;
		cp->cache_maxcolor =
		    (cp->cache_slabsize - sizeof (kmem_slab_t)) % chunksize;
		ASSERT(chunksize + sizeof (kmem_slab_t) <= cp->cache_slabsize);
		cp->cache_flags &= ~KMF_AUDIT;
	} else {
		int chunks, bestfit, waste, slabsize;
		int minwaste = INT_MAX;
		for (chunks = 1; chunks <= KMEM_VOID_FRACTION; chunks++) {
			slabsize = roundup(chunksize * chunks,
				bep->be_pagesize);
			chunks = slabsize / chunksize;
			waste = (slabsize % chunksize) / chunks;
			if (waste < minwaste) {
				minwaste = waste;
				bestfit = slabsize;
			}
		}
		cp->cache_slabsize = bestfit;
		cp->cache_maxcolor = bestfit % chunksize;
		cp->cache_flags |= KMF_HASH;
		cp->cache_hash_table = kmem_page_alloc(&kmem_default_backend,
			1, KM_SLEEP);
		cp->cache_hash_mask = PAGESIZE / sizeof (void *) - 1;
		cp->cache_hash_shift = highbit((u_long)chunksize) - 1;
		bzero(cp->cache_hash_table, PAGESIZE);
		cp->cache_bufctl_cache = (cp->cache_flags & KMF_AUDIT) ?
			kmem_bufctl_audit_cache : kmem_bufctl_cache;
		kmem_async_dispatch(kmem_hash_rescale, cp, KM_SLEEP);
	}

	mutex_enter(&kmem_cache_lock);
	cp->cache_next = cnext = &kmem_null_cache;
	cp->cache_prev = cprev = kmem_null_cache.cache_prev;
	cnext->cache_prev = cp;
	cprev->cache_next = cp;
	mutex_exit(&kmem_cache_lock);

	/*
	 * We can't quite finish creating the cache now because caches can
	 * be created very early in the life of the system.  Specifically
	 * kmem_cache_create() can be called before the total number of
	 * cpus is known and before the kstat framework is initialized.
	 * However, the cache *is* usable at this point, so we can just
	 * direct the kmem async thread (which doesn't come to life until
	 * ncpus is known and kstats are working) to apply the finishing
	 * touches later, when it's safe to do so.
	 */
	kmem_async_dispatch(kmem_cache_create_finish, cp, KM_SLEEP);

	return (cp);
}

static void
kmem_cache_destroy_finish(kmem_cache_t *cp)
{
	int cpu_seqid;

	ASSERT(MUTEX_HELD(&kmem_async_serialize));

	if (cp->cache_kstat)
		kstat_delete(cp->cache_kstat);

	if (cp->cache_flags & KMF_HASH) {
		if (cp->cache_rescale == 0)	/* initial hash table */
			kmem_page_free(&kmem_default_backend,
				cp->cache_hash_table, 1);
		else
			kmem_free(cp->cache_hash_table, sizeof (void *) *
				(cp->cache_hash_mask + 1));
	}

	for (cpu_seqid = 0; cpu_seqid < cp->cache_ncpus; cpu_seqid++)
		mutex_destroy(&cp->cache_cpu[cpu_seqid].cc_lock);

	mutex_destroy(&cp->cache_depot_lock);
	mutex_destroy(&cp->cache_lock);

	mutex_enter(&kmem_cache_lock);
	cp->cache_next = kmem_cache_freelist;
	kmem_cache_freelist = cp;
	mutex_exit(&kmem_cache_lock);
}

void
kmem_cache_destroy(kmem_cache_t *cp)
{
	/*
	 * Remove the cache from the global cache list (so that no one else
	 * can schedule async events on its behalf), purge the cache, and
	 * then destroy it.  Since the async thread processes requests in
	 * FIFO order we can assume that kmem_cache_destroy_finish() will
	 * not be invoked until all other pending async events for this
	 * cache have completed and the cache is empty.
	 *
	 * Note that we *must* purge the cache synchonously because we have
	 * pointers to the caller's constructor, destructor, and reclaim
	 * routines.  These can either go away (via module unloading) or
	 * cease to make sense (by referring to destroyed client state)
	 * as soon as kmem_cache_destroy() returns control to the caller.
	 */
	mutex_enter(&kmem_cache_lock);
	cp->cache_prev->cache_next = cp->cache_next;
	cp->cache_next->cache_prev = cp->cache_prev;
	mutex_exit(&kmem_cache_lock);

	mutex_enter(&kmem_async_serialize);	/* lock out the async thread */
	kmem_cache_magazine_purge(cp);
	mutex_enter(&cp->cache_lock);
	if (cp->cache_buftotal != 0)
		cmn_err(CE_PANIC, "kmem_cache_destroy: '%s' (%x) not empty",
		    cp->cache_name, (int)cp);
	cp->cache_reclaim = NULL;
	/*
	 * The cache is now dead.  There should be no further activity.
	 * We enforce this by setting land mines in the constructor and
	 * destructor routines that induce a kernel text fault if invoked.
	 */
	cp->cache_constructor = (int (*)(void *, void *, int))1;
	cp->cache_destructor = (void (*)(void *, void *))2;
	mutex_exit(&cp->cache_lock);
	mutex_exit(&kmem_async_serialize);

	mutex_enter(&kmem_backend_lock);
	cp->cache_backend->be_clients--;
	mutex_exit(&kmem_backend_lock);

	/*
	 * If this cache ever had debugging enabled, destroy the debug cache
	 */
	if (cp->cache_debug != NULL)
		kmem_cache_destroy(cp->cache_debug);

	kmem_async_dispatch(kmem_cache_destroy_finish, cp, KM_SLEEP);
}

void
kmem_async_thread(void)
{
	kmem_async_t *ap, *anext, *aprev;
	kstat_t *ksp;
	cpu_t *cpup;
	int nk;

	kmem_cache_kstat = kmem_perm_alloc(sizeof (struct kmem_cache_kstat) +
		ncpus * sizeof (struct kmem_cpu_kstat), 0, KM_SLEEP);
	kmem_cpu_kstat = (void *)(kmem_cache_kstat + 1);

	bcopy(&kmem_cache_kstat_template, kmem_cache_kstat,
		sizeof (struct kmem_cache_kstat));

	mutex_enter(&cpu_lock);
	cpup = cpu_list;
	do {
		kstat_named_t *src = (void *)&kmem_cpu_kstat_template;
		kstat_named_t *dst = (void *)&kmem_cpu_kstat[cpup->cpu_seqid];
		bcopy(src, dst, sizeof (kmem_cpu_kstat_template));
		nk = sizeof (struct kmem_cpu_kstat) / sizeof (kstat_named_t);
		while (--nk >= 0)
			sprintf((dst++)->name, (src++)->name, cpup->cpu_id);
		cpup = cpup->cpu_next;
	} while (cpup != cpu_list);
	mutex_exit(&cpu_lock);

	if ((ksp = kstat_create("unix", 0, "kmem_misc", "kmem",
	    KSTAT_TYPE_NAMED, sizeof (kmem_misc_kstat) / sizeof (kstat_named_t),
	    KSTAT_FLAG_VIRTUAL)) != NULL) {
		ksp->ks_data = &kmem_misc_kstat;
		kstat_install(ksp);
	}

	kmem_backend_kstat_create(&kmem_default_backend,
		"kmem_default_backend");

	(void) timeout(kmem_update, 0, kmem_reap_interval);

	mutex_enter(&kmem_async_lock);
	for (;;) {
		while (kmem_async_queue.async_next == &kmem_async_queue)
			cv_wait(&kmem_async_cv, &kmem_async_lock);
		ap = kmem_async_queue.async_next;
		anext = ap->async_next;
		aprev = ap->async_prev;
		aprev->async_next = anext;
		anext->async_prev = aprev;
		mutex_exit(&kmem_async_lock);

		TRACE_3(TR_FAC_KMEM, TR_KMEM_ASYNC_SERVICE_START,
			"kmem_async_service_start:async_entry %x %K(%S)",
			ap, ap->async_func, ap->async_cache->cache_name);

		mutex_enter(&kmem_async_serialize);
		(*ap->async_func)(ap->async_cache);
		mutex_exit(&kmem_async_serialize);

		TRACE_0(TR_FAC_KMEM, TR_KMEM_ASYNC_SERVICE_END,
			"kmem_async_service_end");

		mutex_enter(&kmem_async_lock);
		ap->async_next = kmem_async_freelist;
		kmem_async_freelist = ap;
	}
}

void
kmem_cpu_init(cpu_t *cp)
{
	cp->cpu_cache_offset = KMEM_CACHE_SIZE(cp->cpu_seqid);
}

void
kmem_init(void)
{
	int i, size, cache_size;
	kmem_cache_t *cp;
	kmem_magazine_type_t *mtp;

#ifdef DEBUG
	/*
	 * Hack to deal with suninstall brokenness.
	 * Suninstall has to run in 16MB of *total virtual memory*, so if
	 * we've got less than 20MB, don't do memory-intensive kmem debugging.
	 */
	if ((physmem >> (20 - PAGESHIFT)) < 20)
		kmem_flags = 0;
#endif

	kmem_default_backend.be_page_alloc = kmem_getpages;
	kmem_default_backend.be_page_free = kmem_freepages;
	kmem_default_backend.be_pagesize = PAGESIZE;
	kmem_default_backend.be_pageshift = PAGESHIFT;
	kmem_default_backend.be_memclass = KMEM_CLASS_WIRED;

	mutex_init(&kmem_cache_lock, "kmem_cache", MUTEX_DEFAULT, NULL);
	mutex_init(&kmem_backend_lock, "kmem_backend", MUTEX_DEFAULT, NULL);
	mutex_init(&kmem_perm_lock, "kmem_perm_lock", MUTEX_DEFAULT, NULL);
	cv_init(&kmem_async_cv, "kmem_async_cv", CV_DEFAULT, NULL);
	mutex_init(&kmem_async_lock, "kmem_async_lock", MUTEX_DEFAULT, NULL);
	mutex_init(&kmem_async_serialize, "kmem_async_serialize",
		MUTEX_DEFAULT, NULL);

	kmem_async_queue.async_next = &kmem_async_queue;
	kmem_async_queue.async_prev = &kmem_async_queue;

	kmem_null_cache.cache_next = &kmem_null_cache;
	kmem_null_cache.cache_prev = &kmem_null_cache;

	for (i = 0; i < sizeof (kmem_magazine_type) / sizeof (*mtp); i++) {
		char namebuf[64];
		mtp = &kmem_magazine_type[i];
		sprintf(namebuf, "kmem_magazine_%d", mtp->mt_magsize);
		mtp->mt_cache = kmem_cache_create(namebuf,
			(mtp->mt_magsize + 1) * sizeof (void *),
			mtp->mt_align, NULL, NULL, NULL, NULL, NULL,
			kmem_self_debug | KMC_NOHASH | KMC_NOMAGAZINE);
	}

	kmem_slab_cache = kmem_cache_create("kmem_slab_cache",
		sizeof (kmem_slab_t), 32,
		NULL, NULL, NULL, NULL, NULL, kmem_self_debug | KMC_NOHASH);

	kmem_bufctl_cache = kmem_cache_create("kmem_bufctl_cache",
		sizeof (kmem_bufctl_t), 16,
		NULL, NULL, NULL, NULL, NULL, kmem_self_debug | KMC_NOHASH);

	kmem_bufctl_audit_cache = kmem_cache_create("kmem_bufctl_audit_cache",
		sizeof (kmem_bufctl_audit_t), 32,
		NULL, NULL, NULL, NULL, NULL, kmem_self_debug | KMC_NOHASH);

	kmem_pagectl_cache = kmem_cache_create("kmem_pagectl_cache",
		sizeof (kmem_pagectl_t), 16, NULL, NULL, NULL, NULL, NULL, 0);

	kmem_reap_interval = 15 * hz;

	/*
	 * Set up the default caches to back kmem_alloc()
	 */
	size = KMEM_ALIGN;
	for (i = 0; i < sizeof (kmem_alloc_sizes) / sizeof (int); i++) {
		char name[40];
		int align = KMEM_ALIGN;
		cache_size = kmem_alloc_sizes[i];
		if (cache_size >= kmem_minhash &&
		    (cache_size & (cache_size - 1)) == 0)
			align = cache_size;
		if ((cache_size & PAGEOFFSET) == 0)
			align = PAGESIZE;
		sprintf(name, "kmem_alloc_%d", cache_size);
		cp = kmem_cache_create(name, cache_size, align,
			NULL, NULL, NULL, NULL, NULL, 0);
		while (size <= cache_size) {
			kmem_alloc_table[(size - 1) >> KMEM_ALIGN_SHIFT] = cp;
			size += KMEM_ALIGN;
		}
	}

	/*
	 * Use about 2% (1/50) of available memory for
	 * transaction and content logging (if enabled).
	 */
	if (kmem_log_size == 0)
		kmem_log_size = (int)(kmem_maxavail() / 50);

	if (kmem_flags & KMF_AUDIT)
		kmem_transaction_log = kmem_log_init(kmem_log_size);

	if (kmem_flags & KMF_CONTENTS)
		kmem_content_log = kmem_log_init(kmem_log_size);

	kmem_ready = 1;
}
