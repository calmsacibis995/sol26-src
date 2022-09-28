/*
 * Copyright (c) 1993-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)mach_sfmmu.c 1.84     96/07/09 SMI"

#include <sys/types.h>
#include <vm/hat.h>
#include <vm/hat_sfmmu.h>
#include <vm/page.h>
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
#include <sys/vm_machparam.h>
#include <sys/promif.h>
#include <sys/prom_isa.h>
#include <sys/prom_plat.h>
#include <sys/prom_debug.h>
#include <sys/privregs.h>
#include <sys/bootconf.h>

/*
 * Static routines
 */
static void	sfmmu_remap_kernel(void);
static void	sfmmu_set_tlb(void);
static void	sfmmu_map_prom_mappings(struct translation *);
static struct translation *
		read_prom_mappings(void);

static void	sfmmu_map_prom_mappings(struct translation *);

/*
 * Global Data:
 */
caddr_t	textva, datava;
tte_t	ktext_tte, kdata_tte;		/* ttes for kernel text and data */

/*
 * XXX Need to understand how crash uses this routine and get rid of it
 * if possible.
 */
void
mmu_setctx(struct ctx *ctx)
{
#ifdef lint
	ctx = ctx;
#endif /* lint */

	STUB(mmu_setctx);
}

/*
 * Global Routines called from within:
 *	usr/src/uts/sun4u
 *	usr/src/uts/sfmmu
 *	usr/src/uts/sun
 */

u_int
va_to_pfn(caddr_t vaddr)
{
	extern u_int tba_taken_over;
	unsigned long long physaddr;
	int mode, valid;

	if (tba_taken_over) {
		return (sfmmu_vatopfn(vaddr, KHATID));
	}

	if ((prom_translate_virt(vaddr, &valid, &physaddr, &mode) != -1) &&
	    (valid == -1)) {
		return ((u_int)(physaddr >> MMU_PAGESHIFT));
	}

	return ((uint)-1);
}

u_longlong_t
va_to_pa(caddr_t vaddr)
{
	uint pfn;

	if ((pfn = va_to_pfn(vaddr)) == -1)
		return ((u_longlong_t)-1);
	return (((u_longlong_t)pfn << MMU_PAGESHIFT) |
		((uint)vaddr & MMU_PAGEOFFSET));
}

void
hat_kern_setup(void)
{
	int cpuid, tsbsz;
	struct translation *trans_root;
	extern void startup_fixup_physavail(void);

	/*
	 * These are the steps we take to take over the mmu from the prom.
	 *
	 * (1)	Read the prom's mappings through the translation property.
	 * (2)	Remap the kernel text and kernel data with 2 locked 4MB ttes.
	 *	Create the the hmeblks for these 2 ttes at this time.
	 * (3)	Create hat structures for all other prom mappings.  Since the
	 *	kernel text and data hme_blks have already been created we
	 *	skip the equivalent prom's mappings.
	 * (4)	Initialize the tsb and its corresponding hardware regs.
	 * (5)	Take over the trap table (currently in startup).
	 * (6)	Up to this point it is possible the prom required some of its
	 *	locked tte's.  Now that we own the trap table we remove them.
	 */
	sfmmu_init_tsbs();
	trans_root = read_prom_mappings();
	sfmmu_remap_kernel();
	startup_fixup_physavail();
	sfmmu_map_prom_mappings(trans_root);
	tsbsz = TSB_BYTES(tsb_szcode);
	/*
	 * We inv tsb for boot cpu because we used it in
	 * sfmmu_map_prom_mappings()
	 */
	sfmmu_inv_tsb(tsballoc_base, tsbsz);
	cpuid = getprocessorid();
	sfmmu_set_itsb(tsb_bases[cpuid].tsb_pfnbase, TSB_SPLIT_CODE,
		tsb_szcode);
	sfmmu_set_dtsb(tsb_bases[cpuid].tsb_pfnbase, TSB_SPLIT_CODE,
		tsb_szcode);
}

/*
 * This routine remaps the kernel using large ttes
 * All entries except locked ones will be removed from the tlb.
 * It assumes that both the text and data segments reside in a separate
 * 4mb virtual and physical contigous memory chunk.  This routine
 * is only executed by the first cpu.  The remaining cpus execute
 * sfmmu_mp_startup() instead.
 * XXX It assumes that the start of the text segment is KERNELBASE.  It should
 * actually be based on start.
 */
static void
sfmmu_remap_kernel(void)
{
	u_int	pfn;
	u_int	attr;
	int	flags;

	extern char end[];
	extern struct as kas;

	textva = (caddr_t)(KERNELBASE & MMU_PAGEMASK4M);
	pfn = va_to_pfn(textva);
	if ((int)pfn == -1)
		prom_panic("can't find kernel text pfn");
	pfn &= TTE_PFNMASK(TTE4M);

	attr = PROC_TEXT | HAT_NOSYNC;
	flags = HAT_LOAD_LOCK | SFMMU_NO_TSBLOAD;
	sfmmu_memtte(&ktext_tte, pfn, attr, TTE4M);
	/*
	 * We set the lock bit in the tte to lock the translation in
	 * the tlb.
	 */
	ktext_tte.tte_lock = 1;
	sfmmu_tteload(kas.a_hat, &ktext_tte, textva, (struct machpage *)NULL,
		flags);

	datava = (caddr_t)((u_int)end & MMU_PAGEMASK4M);
	pfn = va_to_pfn(datava);
	if ((int)pfn == -1)
		prom_panic("can't find kernel data pfn");
	pfn &= TTE_PFNMASK(TTE4M);

	attr = PROC_DATA | HAT_NOSYNC;
	sfmmu_memtte(&kdata_tte, pfn, attr, TTE4M);
	/*
	 * We set the lock bit in the tte to lock the translation in
	 * the tlb.  We also set the mod bit to avoid taking dirty bit
	 * traps on kernel data.
	 */
	TTE_SET_LOFLAGS(&kdata_tte, TTE_LCK_INT | TTE_HWWR_INT,
		TTE_LCK_INT | TTE_HWWR_INT);
	sfmmu_tteload(kas.a_hat, &kdata_tte, datava, (struct machpage *)NULL,
		flags);

	sfmmu_set_tlb();
}

/*
 * Setup the kernel's locked tte's
 */
static void
sfmmu_set_tlb(void)
{
	dnode_t node;
	u_int len, index;

	node = cpunodes[getprocessorid()].nodeid;
	len = prom_getprop(node, "#itlb-entries", (caddr_t)&index);
	if (len != sizeof (index))
		prom_panic("bad #itlb-entries property");
	prom_itlb_load(index - 1, *(u_longlong_t *)&ktext_tte, textva);
	len = prom_getprop(node, "#dtlb-entries", (caddr_t)&index);
	if (len != sizeof (index))
		prom_panic("bad #dtlb-entries property");
	prom_dtlb_load(index - 1, *(u_longlong_t *)&kdata_tte, datava);
	prom_dtlb_load(index - 2, *(u_longlong_t *)&ktext_tte, textva);
}

/*
 * This routine is executed by all other cpus except the first one
 * at initialization time.  It is responsible for taking over the
 * mmu from the prom.  We follow these steps.
 * Lock the kernel's ttes in the TLB
 * Initialize the tsb hardware registers
 * Take over the trap table
 * Flush the prom's locked entries from the TLB
 */
void
sfmmu_mp_startup(void)
{
	int cpuid;

	extern struct scb trap_table;
	extern void setwstate(u_int);
	extern void install_va_to_tte();

	cpuid = getprocessorid();
	sfmmu_set_tlb();
	sfmmu_set_itsb(tsb_bases[cpuid].tsb_pfnbase, TSB_SPLIT_CODE,
		tsb_szcode);
	sfmmu_set_dtsb(tsb_bases[cpuid].tsb_pfnbase, TSB_SPLIT_CODE,
		tsb_szcode);
	setwstate(WSTATE_KERN);
	prom_set_traptable((caddr_t)&trap_table);
	install_va_to_tte();
}

/*
 * This function traverses the prom mapping list and creates equivalent
 * mappings in the sfmmu mapping hash.
 */
static void
sfmmu_map_prom_mappings(struct translation *trans_root)
{
	struct translation *promt;
	tte_t tte, *ttep;
	u_int offset;
	u_int pfn, oldpfn, basepfn;
	u_int vaddr;
	int size;
	u_int attr;
	int flags = HAT_LOAD_LOCK | SFMMU_NO_TSBLOAD;
	struct machpage *pp;
	extern int pf_is_memory(u_int);
	extern struct memlist *virt_avail;

	ttep = &tte;
	for (promt = trans_root; promt && promt->tte_hi; promt++) {
		/* hack until we get rid of map-for-unix */
		if (promt->virt_lo < KERNELBASE)
			continue;
		ttep->tte_inthi = promt->tte_hi;
		ttep->tte_intlo = promt->tte_lo;
		attr = PROC_DATA | HAT_NOSYNC;
		if (TTE_IS_GLOBAL(ttep)) {
			/*
			 * The prom better not use global translations
			 * because a user process might use the same
			 * virtual addresses
			 */
			cmn_err(CE_PANIC, "map_prom: global translation");
			TTE_SET_LOFLAGS(ttep, TTE_GLB_INT, 0);
		}
		if (TTE_IS_LOCKED(ttep)) {
			/* clear the lock bits */
			TTE_SET_LOFLAGS(ttep, TTE_LCK_INT, 0);
		}
		if (!TTE_IS_VCACHEABLE(ttep)) {
			attr |= SFMMU_UNCACHEVTTE;
		}
		if (!TTE_IS_PCACHEABLE(ttep)) {
			attr |= SFMMU_UNCACHEPTTE;
		}
		if (TTE_IS_SIDEFFECT(ttep)) {
			attr |= SFMMU_SIDEFFECT;
		}
		if (TTE_IS_IE(ttep)) {
			attr |= HAT_STRUCTURE_LE;
		}

		/*
		 * Since this is still just a 32 bit machine ignore
		 * virth_hi and size_hi
		 */
		size = promt->size_lo;
		offset = 0;
		basepfn = TTE_TO_PFN((caddr_t)promt->virt_lo, ttep);
		while (size) {
			vaddr = promt->virt_lo + offset;
			/*
			 * make sure address is not in virt-avail list
			 */
			if (address_in_memlist(virt_avail, (caddr_t)vaddr,
			    size)) {
				cmn_err(CE_PANIC, "map_prom: inconsistent "
				    "translation/avail lists");
			}

			pfn = basepfn + mmu_btop(offset);
			if (pf_is_memory(pfn)) {
				if (attr & SFMMU_UNCACHEPTTE) {
					cmn_err(CE_PANIC, "map_prom: "
					    "uncached prom memory page");
				}
			} else {
				if (!(attr & SFMMU_SIDEFFECT)) {
					cmn_err(CE_PANIC, "map_prom: prom "
					    "i/o page without side-effect");
				}
			}
			pp = PP2MACHPP(page_numtopp_nolock(pfn));
			if ((oldpfn = sfmmu_vatopfn((caddr_t)vaddr, KHATID))
				!= -1) {
				/*
				 * mapping already exists.
				 * Verify they are equal
				 */
				if (pfn != oldpfn) {
					cmn_err(CE_PANIC, "map_prom: mapping "
					    "conflict");
				}
				size -= MMU_PAGESIZE;
				offset += MMU_PAGESIZE;
				continue;
			}
			if (pp != NULL && PP_ISFREE((page_t *)pp)) {
				cmn_err(CE_PANIC, "map_prom: prom page not "
				    "on free list");
			}
			if (!pp && size >= MMU_PAGESIZE4M &&
			    !(vaddr & MMU_PAGEOFFSET4M) &&
			    !(mmu_ptob(pfn) & MMU_PAGEOFFSET4M)) {
				sfmmu_memtte(ttep, pfn, attr, TTE4M);
				sfmmu_tteload(kas.a_hat, ttep, (caddr_t)vaddr,
					pp, flags);
				size -= MMU_PAGESIZE4M;
				offset += MMU_PAGESIZE4M;
				continue;
			}
#ifdef NOTYET
	/*
	 * OBP needs to support large page size before we can reenable this.
	 */
			if (!pp && size >= MMU_PAGESIZE512K &&
			    !(vaddr & MMU_PAGEOFFSET512K) &&
			    !(mmu_ptob(pfn) & MMU_PAGEOFFSET512K)) {
				sfmmu_memtte(ttep, pfn, attr, TTE512K);
				sfmmu_tteload(kas.a_hat, ttep, (caddr_t)vaddr,
					pp, flags);
				size -= MMU_PAGESIZE512K;
				offset += MMU_PAGESIZE512K;
				continue;
			}
			if (!pp && size >= MMU_PAGESIZE64K &&
			    !(vaddr & MMU_PAGEOFFSET64K) &&
			    !(mmu_ptob(pfn) & MMU_PAGEOFFSET64K)) {
				sfmmu_memtte(ttep, pfn, attr, TTE64K);
				sfmmu_tteload(kas.a_hat, ttep, (caddr_t)vaddr,
					pp, flags);
				size -= MMU_PAGESIZE64K;
				offset += MMU_PAGESIZE64K;
				continue;
			}
#endif /* NOTYET */
			sfmmu_memtte(ttep, pfn, attr, TTE8K);
			sfmmu_tteload(kas.a_hat, ttep, (caddr_t)vaddr,
				pp, flags);
			size -= MMU_PAGESIZE;
			offset += MMU_PAGESIZE;
		}
	}
}

/*
 * This routine reads in the "translations" property in to a buffer and
 * returns a pointer to this buffer
 */
static struct translation *
read_prom_mappings(void)
{
	char *prop = "translations";
	int translen;
	dnode_t node;
	struct translation *transroot;

	/*
	 * the "translations" property is associated with the mmu node
	 */
	node = (dnode_t)prom_getphandle(prom_mmu_ihandle());

	/*
	 * We use the TSB space to read in the prom mappings.  This space
	 * is currently not being used because we haven't taken over the
	 * trap table yet.  It should be big enough to hold the mappings.
	 */
	if ((translen = prom_getproplen(node, prop)) == -1)
		cmn_err(CE_PANIC, "no translations property");
	translen = roundup(translen, MMU_PAGESIZE);
	PRM_DEBUG(translen);
	if (translen > TSB_BYTES(tsb_szcode))
		cmn_err(CE_PANIC, "not enough space for translations");

	transroot = (struct translation *)tsballoc_base;
	ASSERT(transroot);
	if (prom_getprop(node, prop, (caddr_t)transroot) == -1) {
		cmn_err(CE_PANIC, "translations getprop failed");
	}
	return (transroot);
}


/*
 * Allocate hat structs from the nucleus data memory.
 */
caddr_t
ndata_alloc_hat(caddr_t hat_alloc_base, caddr_t nalloc_end, int npages)
{
	int 	hmehash_sz, ctx_sz;
	int	thmehash_num, wanted_hblks, max_hblks;
	int	wanted_hblksz = 0;
	caddr_t	wanted_endva;

	extern int		uhmehash_num;
	extern int		khmehash_num;
	extern struct hmehash_bucket	*uhme_hash;
	extern struct hmehash_bucket	*khme_hash;
	extern int		sfmmu_add_nucleus_hblks();
	extern int		highbit(u_long);
	extern int		ecache_linesize;

	PRM_DEBUG(npages);

	/*
	 * Allocate ctx structures
	 *
	 * based on v_proc to calculate how many ctx structures
	 * is not possible;
	 * use whatever module_setup() assigned to nctxs
	 */
	PRM_DEBUG(nctxs);
	ctx_sz = nctxs * sizeof (struct ctx);
	ctxs = (struct ctx *)hat_alloc_base;
	hat_alloc_base += ctx_sz;
	hat_alloc_base = (caddr_t)roundup((u_int)hat_alloc_base,
		ecache_linesize);
	PRM_DEBUG(ctxs);

	ASSERT(hat_alloc_base < nalloc_end);

	/*
	 * The number of buckets in the hme hash tables
	 * is a power of 2 such that the average hash chain length is
	 * HMENT_HASHAVELEN.  The number of buckets for the user hash is
	 * a function of physical memory and a predefined overmapping factor.
	 * The number of buckets for the kernel hash is a function of
	 * KERNELSIZE.
	 */
	uhmehash_num = (npages * HMEHASH_FACTOR) /
		(HMENT_HASHAVELEN * (HMEBLK_SPAN(TTE8K) >> MMU_PAGESHIFT));
	uhmehash_num = 1 << highbit(uhmehash_num - 1);
	uhmehash_num = min(uhmehash_num, MAX_UHME_BUCKETS);
	khmehash_num = KERNELSIZE /
		(HMENT_HASHAVELEN * HMEBLK_SPAN(TTE8K));
	khmehash_num = 1 << highbit(khmehash_num - 1);
	thmehash_num = uhmehash_num + khmehash_num;
	hmehash_sz = thmehash_num * sizeof (struct hmehash_bucket);
	khme_hash = (struct hmehash_bucket *)hat_alloc_base;
	uhme_hash = (struct hmehash_bucket *)((caddr_t)khme_hash +
		khmehash_num * sizeof (struct hmehash_bucket));
	hat_alloc_base += hmehash_sz;
	hat_alloc_base = (caddr_t)roundup((u_int)hat_alloc_base,
		ecache_linesize);
	PRM_DEBUG(khme_hash);
	PRM_DEBUG(khmehash_num);
	PRM_DEBUG(uhme_hash);
	PRM_DEBUG(uhmehash_num);
	PRM_DEBUG(hmehash_sz);
	PRM_DEBUG(hat_alloc_base);
	ASSERT(hat_alloc_base < nalloc_end);

	/*
	 * Allocate nucleus hme_blks
	 * We only use hme_blks out of the nucleus pool when we are mapping
	 * other hme_blks.  The absolute worse case if we were to use all of
	 * physical memory for hme_blks so we allocate enough nucleus
	 * hme_blks to map all of physical memory.  This is real overkill
	 * so might want to divide it by a certain factor.
	 * RFE: notice that I will only allocate as many hmeblks as
	 * there is space in the nucleus.  We should add a check at the
	 * end of sfmmu_tteload to check how many "nucleus" hmeblks we have.
	 * If we go below a certain threshold we kmem alloc more.  The
	 * "nucleus" hmeblks need not be part of the nuclues.  They just
	 * need to be preallocated to avoid the recursion on kmem alloc'ed
	 * hmeblks.
	 */
	wanted_hblks = MIN(npages, mmu_btop((u_int)SYSEND -
		(u_int)KERNELBASE)) / (HMEBLK_SPAN(TTE8K) >> MMU_PAGESHIFT);
	PRM_DEBUG(wanted_hblks);
	if (wanted_hblks > 0) {
		max_hblks = ((uint)nalloc_end - (uint)hat_alloc_base) /
			HME8BLK_SZ;
		wanted_hblks = min(wanted_hblks, max_hblks);
		PRM_DEBUG(wanted_hblks);
		wanted_hblksz = wanted_hblks * HME8BLK_SZ;
		wanted_endva = (caddr_t)roundup((uint)hat_alloc_base +
			wanted_hblksz, MMU_PAGESIZE);
		wanted_hblksz = wanted_endva - hat_alloc_base;
		(void) sfmmu_add_nucleus_hblks(hat_alloc_base, wanted_hblksz);
		PRM_DEBUG(wanted_hblksz);
		hat_alloc_base += wanted_hblksz;
		ASSERT(!((u_int)hat_alloc_base & MMU_PAGEOFFSET));
	}
	ASSERT(hat_alloc_base <= nalloc_end);
	PRM_DEBUG(hat_alloc_base);
	PRM_DEBUG(HME8BLK_SZ);
	return (hat_alloc_base);
}

/*
 * This function bop allocs a tsb per cpu
 */
caddr_t
sfmmu_tsb_alloc(caddr_t tsbbase, int npages)
{
	int tsbsz;
	caddr_t vaddr;
	extern int	ncpunode;
	extern caddr_t	tsballoc_base;

	/*
	 * If the total amount of freemem is less than 32mb then choose
	 * a small tsb so more memory is available to the user.
	 */
	if (npages <= TSB_FREEMEM_MIN) {
		tsb_szcode = TSB_128K_SZCODE;
	} else if (npages < TSB_FREEMEM_LARGE) {
		tsb_szcode = TSB_512K_SZCODE;
	} else {
		tsb_szcode = TSB_1MB_SZCODE;
	}

	tsbsz = TSB_BYTES(tsb_szcode);
	PRM_DEBUG(tsb_szcode);

	tsballoc_base = (caddr_t)roundup((u_int)tsbbase, tsbsz);
	if ((vaddr = (caddr_t)BOP_ALLOC(bootops, tsballoc_base,
	    tsbsz * ncpunode, tsbsz)) == NULL) {
		cmn_err(CE_PANIC, "sfmmu_tsb_alloc: can't alloc tsbs");
	}
	ASSERT(vaddr == tsballoc_base);
	return (tsballoc_base + (tsbsz * ncpunode));
}

void
sfmmu_panic(struct regs *rp)
{
	printf("sfmmu_panic rp = 0x%x\n", rp);
	cmn_err(CE_PANIC, "sfmmu_panic rp = 0x%x\n", rp);
}
