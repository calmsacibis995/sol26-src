/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)mach_ppcmmu.c 1.20     96/07/10 SMI"

#include <sys/sysmacros.h>
#include <sys/t_lock.h>
#include <sys/memlist.h>
#include <sys/cpu.h>
#include <vm/seg_kmem.h>
#include <sys/mman.h>
#include <sys/mmu.h>
#include <sys/kmem.h>
#include <sys/pte.h>
#include <sys/debug.h>
#include <sys/debug/debug.h>
#include <sys/vm_machparam.h>
#include <vm/hat_ppcmmu.h>
#include <vm/mach_ppcmmu.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <sys/vmmac.h>
#include <sys/map.h>
#include <sys/cmn_err.h>
#include <sys/vm_machparam.h>
#include <sys/proc.h>
#include <sys/ddidmareq.h>
#include <sys/sysconfig_impl.h>
#include <sys/prom_isa.h>
#include <sys/obpdefs.h>
#include <sys/bootconf.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/byteorder.h>
#include <sys/systm.h>
#include <sys/bitmap.h>

/* prom memory translation callbacks */
#define	NUM_CALLBACKS	3

/*
 * External Data
 */
extern int segkmem_ready;
extern caddr_t page_table_virtual; /* virtual address of page table */
extern short cputype;
extern struct seg kvseg;

/*
 * static data.
 */
static u_int kern_pt_size;
static u_int kern_pt_virt;
static u_int kern_pt_phys;
static int ppcmmu_kernel_pt_ready = 0;

/* static table for the translations property */
static struct trans {
	unsigned int virt;
	unsigned int size;
	unsigned int phys;
	unsigned int mode;
};

/*
 * External functions.
 */
extern void ppcmmu_unmap_bat(int);
extern struct batinfo *ppcmmu_findbat(caddr_t);
extern u_int ppcmmu_get_bat_l(int);
extern u_int ppcmmu_get_bat_u(int);
extern page_t *page_numtopp_nolock(u_int pfnum);
page_t *page_numtopp_alloc(u_int pfnum);
void ppcmmu_takeover(void);

/*
 * static functions.
 */
static int lomem_check_limits(ddi_dma_lim_t *limits, u_int lo, u_int hi);
static caddr_t ppcdevmap(int, int, u_int);
static int get_translations_prop(struct trans *translations, int get_trans);

/*
 * va_to_pfn(caddr_t addr)
 *	Given the virtual address find the physical page number, returns -1
 *	for invalid mapping. If segkmem_ready != 0 then it simply calls
 *	hat_getpfnum().
 */
u_int
va_to_pfn(caddr_t addr)
{
	u_int pfn;
	int mode, valid;
	struct batinfo *bat;

	if (segkmem_ready != 0) {
		return (addr >= (caddr_t)KERNELBASE ? hat_getkpfnum(addr) :
			hat_getpfnum(curproc->p_as->a_hat, addr));
	}
	/*
	 * The hat layer is not set up yet so check the bat registers first
	 * and then check the page tables.
	 */
	bat = ppcmmu_findbat(addr);

	if (bat != NULL) {
		return ((u_int)(bat->batinfo_paddr +
		    (addr - bat->batinfo_vaddr)) >> MMU_PAGESHIFT);
	}
	/*
	 * use the prom's mmu translate method since the prom may
	 * fault the mapping into its page tables when it is accessed
	 * so we may not see it if we go routing directly thru the
	 * prom's page table
	 */
	if ((prom_translate_virt(addr, &valid, &pfn, &mode) != -1) &&
	    (valid == -1)) {
		return (pfn >> MMU_PAGESHIFT);
	}
	return ((u_int)-1);
}

u_int
va_to_pa(u_int vaddr)
{
	u_int pfn;

	pfn = va_to_pfn((caddr_t)vaddr);
	if (pfn != (u_int)-1)
		return ((pfn << MMU_PAGESHIFT) | (vaddr & MMU_PAGEOFFSET));
	else
		return ((u_int)-1);
}

/*
 * this should eventually be changes to dynamically size the pages tables
 * based on the amount of physical memory, etc.  For now we set it to
 * size large enough to contain the kernel and all prom mappings (until
 * the prom is released.
 * Note that the size is settable in /etc/system for now.
 */
u_int ppc_page_table_sz = 0x80000;

static u_int
get_page_table_sz()
{
	return (ppc_page_table_sz);
}

#define	PAGE_TABLE_MAP_MODE 0x18
/*
 * Called from before startup() to initialize kernel variables like nptegp, etc.
 * based on the information passed from boot.
 * We also allocate space for the page table here - later on we will switch
 * from using the page table allocated by OF to this page table.
 */
void
ppcmmu_param_init(void)
{
	int i;
	int maxbats;

	kern_pt_virt = (u_int)PAGE_TABLE;
	kern_pt_size = get_page_table_sz();	/* minimum page table size */

	/*
	 * nptegp is used to initialize the hat layer.  It is set here based
	 * on the eventual page table so that other structures can be sized
	 * appropriately.
	 */
	nptegp = kern_pt_size >> 7;

	/*
	 * allocate space for the kernel's page table.
	 *
	 * prom_allocate_phys does the work of the next three calls but
	 * it assumes default values some places where we want to
	 * use a specific value.
	 */

	if (prom_claim_virt(kern_pt_size, (caddr_t)kern_pt_virt)
	    == (caddr_t)-1) {
		prom_printf("prom_claim_virt page table failed\n");
		prom_enter_mon();
	}

	if (prom_allocate_phys(kern_pt_size, kern_pt_size, &kern_pt_phys)
	    == -1) {
		prom_printf("prom_allocate_phys page table failed\n");
		prom_enter_mon();
	}

	if (prom_map_phys(PAGE_TABLE_MAP_MODE, kern_pt_size,
		(caddr_t)kern_pt_virt, 0, kern_pt_phys) == -1)  {
		prom_printf("prom_map_phys page table failed\n");
		prom_enter_mon();
	}

	/*
	 * Identify CPU specific features.
	 *
	 * XXXPPC: The information about the CPU specific features may be
	 * available as properties from Open Firmware implementations.
	 */
	cache_blockmask = dcache_blocksize - 1;
	switch (cputype & CPU_ARCH) {
	default:
		printf("ppcmmu_param_init: Unknown CPU type - assuming 601");
		/*FALLTHROUGH*/
	case PPC_601_ARCH:
		mmu601 = 1;		/* MMU is 601 specific */
		lwarx_size = 4;
		coherency_size = 32;
		break;
	case PPC_603_ARCH:
	case PPC_604_ARCH:
		mmu601 = 0;
		lwarx_size = 4;
		coherency_size = dcache_blocksize;
		break;
	}

	/*
	 * Read Bat information into bats[] array.
	 */
	maxbats = mmu601 ? 4 : 8;
	for (i = 0; i < maxbats; i++) {
		register u_int batword_lower, batword_upper;

		batword_lower = ppcmmu_get_bat_l(i);
		batword_upper = ppcmmu_get_bat_u(i);
		if (mmu601) {
			if ((batword_lower & BAT_601_V) == 0) {
				bats[i].batinfo_valid = 0;
				continue;
			}
			bats[i].batinfo_valid = 1;
			bats[i].batinfo_size =
				((batword_lower & BAT_601_BSM) + 1) <<
				BAT_601_BLOCK_SHIFT;
			bats[i].batinfo_paddr =
				(caddr_t)(batword_lower & BAT_601_PBN);
			bats[i].batinfo_vaddr =
				(caddr_t)(batword_upper & BAT_601_BLPI);
		} else { /* PowerPC archtecture BATs: IBATs and DBATs */
			if ((batword_upper & (BAT_VS|BAT_VP)) == 0) {
				bats[i].batinfo_valid = 0;
				continue;
			}
			bats[i].batinfo_valid = 1;
			if (i > 3)
				bats[i].batinfo_type = 1;
			bats[i].batinfo_size =
				((batword_upper & BAT_BL) + 4) <<
				BAT_BL_SHIFT;
			bats[i].batinfo_paddr =
				(caddr_t)(batword_lower & BAT_BRPN);
			bats[i].batinfo_vaddr =
				(caddr_t)(batword_upper & BAT_BEPI);
		}
	}
}

/*
 * This routine will place a mapping in the page table
 * This is only used by the startup code to grab hold
 * of the MMU before everything is setup.
 * It may also be called as a result of a memory request from
 * the prom.
 */
void
map_one_page(caddr_t phys, caddr_t virt, int mode)
{
	ulong_t hash1, h, i;
	hwpte_t *hwpte;
	pte_t swpte;

	/*
	 * we should never be called until the kernel's page table
	 * has been allocated and associated kernel globals initialized.
	 */
	ASSERT(ppcmmu_kernel_pt_ready != 0);

	/*
	 * This routine can be called as a result of a prom callback,
	 * which could potentially happen after we have set up segkmem.
	 */
	if (segkmem_ready) {
		hat_devload(kas.a_hat, virt, MMU_PAGESIZE,
		    (u_long)(phys) >> MMU_PAGESHIFT,
		    PROT_READ|PROT_WRITE|PROT_EXEC, HAT_LOAD_LOCK);
		return;
	}

	/*
	 * fill in the software pte
	 */
	swpte.pte_api = API((ulong_t)virt);
	swpte.pte_vsid = VSID(KERNEL_VSID_RANGE, virt);
	swpte.pte_valid = PTE_VALID;
	swpte.pte_pp = MMU_STD_SRWX;		/* SRWX */
	swpte.pte_wimg = mode >> 3;
	swpte.pte_ppn = (ulong_t)phys >> MMU_PAGESHIFT;

	/* compute primary from virt */
	hash1 =  ((VSID(KERNEL_VSID_RANGE, virt) & HASH_VSIDMASK) ^
	    (((u_long)virt >> MMU_PAGESHIFT) & HASH_PAGEINDEX_MASK));

	/* search for an invalid page table entry, then use it */
	h = hash1;
	hwpte = hash_get_primary(h);
	for (i = 0; (h == hash1) || i < NPTEPERPTEG; i++, hwpte++) {
		if (i == NPTEPERPTEG) {
			/*
			 * switch to secondary hash table
			 */
			i = 0;
			h = hash1 ^ hash_pteg_mask;
			hwpte = hash_get_secondary(hash1);
		}
		if (hwpte_valid(hwpte) == PTE_INVALID) {
			/*
			 * found one - update the pte
			 */
			swpte.pte_h = (h != hash1);
			mmu_update_pte(hwpte, &swpte, 0, virt);
#ifdef PRINT_MAPS
			prom_printf("P: 0x%x V: 0x%x PTE: 0x%x: 0x%x 0x%x\n",
			    phys, virt, hwpte, *(u_int *)hwpte,
			    *(((u_int *)hwpte)+1));
#endif /* PRINT_MAPS */
			return;
		}
		/*
		 * make sure virt is not already mapped.
		 */
		if ((api_from_hwpte(hwpte) == API((ulong_t)virt)) &&
		    (hbit_from_hwpte(hwpte) == (h != hash1)) &&
		    (vsid_from_hwpte(hwpte) == VSID(KERNEL_VSID_RANGE, virt))) {
#if defined(DEBUG)
			prom_printf("map_one_page: Duplicate PTE.\n");
			prom_printf("Not mapping virtual address %x\n", virt);
#endif
			return;
		}
	}
	cmn_err(CE_PANIC, "map_one_page: no available PTE found");
}
/*
 * unmap a page.
 * This routine should only be called as a result of prom callback
 * or early on during startup whilst we are taking over the mmu.
 * Note that is should never be called before the kernel's page table
 * and associated kernel globals have been initialized.
 */
void
unmap_one_page(caddr_t virt)
{
	ulong_t hash1, h, i;
	hwpte_t *hwpte;

	ASSERT(ppcmmu_kernel_pt_ready != 0);

	/*
	 * if segkmem has been initialized, then we can use the
	 * standard methods to unmap the memory.
	 */
	if (segkmem_ready) {
		hat_unlock(kas.a_hat, virt, MMU_PAGESIZE);
		hat_unload(kas.a_hat, virt, MMU_PAGESIZE, HAT_UNLOAD);
		return;
	}

	/* compute primary from virt */
	hash1 =  ((VSID(KERNEL_VSID_RANGE, virt) & HASH_VSIDMASK) ^
	    (((u_long)virt >> MMU_PAGESHIFT) & HASH_PAGEINDEX_MASK));
	h = hash1;
	hwpte = hash_get_primary(h);

	/* search for the matching page table entry, then invaldiate it */
	for (i = 0; (h == hash1) || i < NPTEPERPTEG; i++, hwpte++) {
		if (i == NPTEPERPTEG) {
			/* switch to the secondary hash table */
			i = 0;
			h = hash1 ^ hash_pteg_mask;
			hwpte = hash_get_secondary(hash1);
		}
		if (hwpte_valid(hwpte) == PTE_INVALID)
			continue;
		if ((api_from_hwpte(hwpte) == API((ulong_t)virt)) &&
		    (hbit_from_hwpte(hwpte) == (h != hash1)) &&
		    (vsid_from_hwpte(hwpte) == VSID(KERNEL_VSID_RANGE, virt))) {
			mmu_delete_pte(hwpte, virt);
			return;
		}
	}
	cmn_err(CE_WARN, "unmap_one_page: couldn't find mapping!?!\n");
}

#ifdef PPCMMU_DEBUG
void
dump_pteg(u_int *gp)
{
	int i, print_header;

	print_header = 1;

	for (i = 0; i < NPTEPERPTEG; i++, gp += 2) {
		struct pte *ptep = (struct pte *)gp;

		if (ptep->pte_valid) {
			if (print_header) {
				print_header = 0;
				prom_printf("0x%x:", gp);
			}
			prom_printf(" %c 0x%x 0x%x", (ptep->pte_h == 0)?'P':'S',
			    *gp, *(gp+1));
		}
	}
	if (print_header == 0)
		prom_printf("\n");
}

void
dump_pt(u_int *ptp, u_int num_ptegs)
{
	int i;

	for (i = 0; i < num_ptegs; i++, ptp += 16)
		dump_pteg(ptp);
}

#endif /* PPCMMU_DEBUG */

/*
 * This routine is called from the prom when a mapping is being
 * created.  It is the PROM's way of asking the kernel to map pages
 * in the page table.  This is really only used after we have taken
 * over memory management - if we call one of the prom services,
 * we need to provide a way for the PROM to allocate memory if it
 * needs to.
 */
static int
map_callback(u_int *args, int num_args, u_int *ret)
{
	u_int phys, virt, size, mode;

	ASSERT(num_args >= 0);

	phys = args[0] &= MMU_PAGEMASK;	/* round down phys addr */
	virt = args[1];
	size = args[2];
	mode = args[3];
	size = btopr((virt + size) - (virt & MMU_PAGEMASK));
	virt &= MMU_PAGEMASK;

	/* create ptes for this mapping */
	for (; size; virt += MMU_PAGESIZE, phys += MMU_PAGESIZE, size--) {
		map_one_page((caddr_t)phys, (caddr_t)virt, mode);
	}

	return (*ret = 0);
}

/*
 * prom callback to unmap a page
 */
static int
unmap_callback(u_int *args, int num_args, u_int *ret)
{
	u_int virt, size;

	ASSERT(num_args >= 0);

	virt = args[0];
	size = args[1];

	size = btopr((virt + size) - (virt & MMU_PAGEMASK));
	virt &= MMU_PAGEMASK;

	/* remove ptes for this mapping */
	for (; size; virt += MMU_PAGESIZE, size--) {
		unmap_one_page((caddr_t)virt);
	}

	return (*ret = 0);
}
/*
 * PROM callback
 * Given a virtual address find the physical address it maps and the mode
 */
static int
translate_callback(u_int *args, int num_args, u_int *ret)
{
	ulong_t hash1, h, i;
	u_int virt;
	hwpte_t *hwpte;
	int rc;

	ASSERT(num_args >= 0);

	virt = args[0];
	rc = -1;

	/*
	 * We probably should not be seeing any requests from the
	 * prom for addresses mapped by bats.  The map_callback() routine
	 * maps everything in the page table.  And at the time we take
	 * over memory management, all mappings are placed in the
	 * page table.  Although the kernel can create mappings using
	 * BATs, the PROM should not be interested in these mappings
	 */
	ASSERT(ppcmmu_findbat((caddr_t)virt) == NULL);

	/* compute primary from virt */
	hash1 =  ((VSID(KERNEL_VSID_RANGE, virt) & HASH_VSIDMASK) ^
	    (((u_long)virt >> MMU_PAGESHIFT) & HASH_PAGEINDEX_MASK));

	/* search for the matching page table entry */
	h = hash1;
	hwpte = hash_get_primary(h);
	for (i = 0; (h == hash1) || i < NPTEPERPTEG; i++, hwpte++) {
		if (i == NPTEPERPTEG) {
			/*
			 * switch to the secondary hash table
			 */
			i = 0;
			h = hash1 ^ hash_pteg_mask;
			hwpte = hash_get_secondary(hash1);
		}

		if (hwpte_valid(hwpte) == PTE_INVALID)
			continue;

		if ((api_from_hwpte(hwpte) == API((ulong_t)virt)) &&
		    (hbit_from_hwpte(hwpte) == (h != hash1)) &&
		    (vsid_from_hwpte(hwpte) == VSID(KERNEL_VSID_RANGE, virt))) {
			ret[1] = 0;	/* physical address high */
			ret[2] = (u_int)(hwpte->pte_ppn << MMU_PAGESHIFT);
			ret[2] = (u_int)((ulong_t)ret[2]
			    | (ulong_t)virt & MMU_PAGEOFFSET);
			/* mode */
			ret[3] = hwpte->pte_wimg << 3 | hwpte->pte_pp;
			rc = 0;
			break;
		}
	}
	return (ret[0] = rc);
}

/*
 * This routine sets up callbacks for memory management from the prom.
 */
void
setup_prom_callbacks()
{
	extern int init_callback_handler(struct callbacks *prom_callbacks);
	static struct callbacks prom_callbacks[NUM_CALLBACKS + 1] = {
		"map",		map_callback,
		"unmap",	unmap_callback,
		"translate",	translate_callback,
		NULL, NULL
	};

	init_callback_handler(prom_callbacks);
}

/*
 * Take over memory management from the PROM
 * We do this by:
 *	- allocating new page table memory (already done in ppcmmu_init)
 * 	- copying the translations into the new page table
 *	- install prom callbacks in case anyone calls prom_map etc
 * 	- setting appropriate global variables
 * 	- setting the SDR1 register to point to new page table
 */
void
ppcmmu_takeover()
{
	u_int sdr1;
	u_int htabmask;
	int num_translations;
	u_int trans_tbl_sz;
	struct trans *tp, *trans_ent;
	extern kern_segregs_sdr1(u_int);

	/*
	 * zero out page table
	 */
	bzero((caddr_t)kern_pt_virt, (size_t)kern_pt_size);

	/*
	 * Setup for kernel Page Table usage
	 */
	nptegp = 512;		   /* minimum PTEG pairs */

	sdr1 = kern_pt_phys + ((kern_pt_size >> 16) - 1);
	htabmask =  sdr1 & SDR1_HTABMASK;
	nptegp  *= htabmask + 1;
	nptegmask = (2 * nptegp) - 1;
	hash_pteg_mask = (htabmask << 10) | 0x3FF;
	ptes = (hwpte_t *)kern_pt_virt;
	eptes = (hwpte_t *)((u_int)kern_pt_virt + kern_pt_size);
	mptes = (hwpte_t *)((u_int)ptes + (kern_pt_size >> 1));
	ppcmmu_kernel_pt_ready = 1;

	/*
	 * Copy current translations into new page table
	 */

	/*
	 * just get the current number of translations for now.
	 */
	num_translations = get_translations_prop(NULL, 0);

	/*
	 * create a table to hold the translations property.
	 * we add an extra page to the table to deal with extra translations
	 * that will be created as a result of allocating a table to
	 * contain the translations.
	 */
	trans_tbl_sz = num_translations * sizeof (struct trans);
	trans_tbl_sz = roundup(trans_tbl_sz, MMU_PAGESIZE) + MMU_PAGESIZE;

	tp = (struct trans *)prom_alloc(0, trans_tbl_sz, MMU_PAGESIZE);
	trans_ent = tp;

	/*
	 * now fill in our buffer with the contents of the translations
	 * property
	 */
	num_translations = get_translations_prop(tp, 1);

	ASSERT(num_translations <= (trans_tbl_sz / sizeof (struct trans)));

	while (num_translations--) {
		int size;
		u_int vaddr, paddr, endvaddr, npages;

		size = htonl(trans_ent->size);		/* convert size */

		/*
		 * skip over BAT mappings - there really should be none
		 */
		if (size >= 0x10000000) {
			trans_ent++;
			continue;
		}

		/* create ptes for this mapping */
		vaddr = htonl(trans_ent->virt);	/* convert virt */
		paddr = htonl(trans_ent->phys) & MMU_PAGEMASK;
		endvaddr = vaddr + size;
		vaddr &= MMU_PAGEMASK;
		npages = btopr(endvaddr - vaddr);
#ifdef PRINT_TRANS
		prom_printf("V: 0x%x P: 0x%x pages: 0x%x mode: 0x%x\n",
		    vaddr, paddr, npages, htonl(trans_ent->mode));
#endif	/* PRINT_TRANS */

		for (; npages--; vaddr += MMU_PAGESIZE,
		    paddr += MMU_PAGESIZE) {
			/*
			 * skip over the mappings for the translations
			 * table since we will unmap it when we are
			 * done copying the translations
			 */
			if ((vaddr >= (u_int)tp) &&
			    (vaddr < ((u_int)tp + trans_tbl_sz))) {
				continue;
			}
			map_one_page((caddr_t)paddr, (caddr_t)vaddr,
			    htonl(trans_ent->mode));
		}
		trans_ent++;
	}

	/*
	 * free the translations table
	 */
	prom_free((caddr_t)tp, trans_tbl_sz);

	/*
	 * If we expect to have anymore calls to prom_map etc
	 * we need to have the call backs setup so the prom can
	 * tell us that a new mapping has occured.
	 */
	setup_prom_callbacks();

	/*
	 * Set segment registers and sdr1 to point to the new table
	 */
	kern_segregs_sdr1(sdr1);
}

/*
 * Update hat data structures (hmes, ptegp, etc.) to reflect the mappings
 * in the page table. Also we need to lock all the kernel mappings in the
 * page table.
 *
 * NOTE:
 *	Normally all the mappings in the page table should be for the virtual
 *	addresses above KERNELBASE, except the mappings created for Boot
 *	(i.e boot-text/boot-data)  and PROM mappings
 *	which are unloaded later in the startup().
 */
void
hat_kern_setup()
{
	register hwpte_t *pte;
	register struct hment *hme;
	register struct ptegp *ptegp;
	register u_int mask;
	register int i;
	register hwpte_t *hwpte;
	u_int addr;

	hat_enter(kas.a_hat);

	hme = hwpte_to_hme(ptes);
	pte = ptes;
	while (pte < eptes) {
		ptegp = hwpte_to_ptegp(pte);
		mask = ((pte >= mptes) ? 0x100 : 1);
		for (i = 0; i < NPTEPERPTEG; i++, pte++, hme++) {
			if (!hwpte_valid(pte))
				continue;
			hme->hme_valid = 1;
			ptegp->ptegp_validmap |= (mask << i);
			ptelock(ptegp, pte);
		}
	}

	/*
	 * Update Sysmap[] pte array
	 */
	for (addr = (u_int)SYSBASE; addr < (u_int)SYSLIMIT;
	    addr += MMU_PAGESIZE) {
		if ((hwpte = ppcmmu_ptefind(&kas, (caddr_t)addr,
			PTEGP_NOLOCK)) == NULL)
			continue;
		pte_to_spte(hwpte, &Sysmap[mmu_btop(addr - SYSBASE)]);
	}

	hat_exit(kas.a_hat);
}

/*
 * Unload the mappings created by Boot for virtual addresses below KERNELBASE
 * (i.e boot-text/boot-data, etc.).
 *
 * After this the boot services are no longer available.
 */
void
unload_boot()
{
	register int i;

	/*
	 * unload any Bat mappings in the range 0-KERNELBASE.
	 */
	for (i = 0; i < 8; i++) {
		if (bats[i].batinfo_valid &&
			bats[i].batinfo_vaddr < (caddr_t)KERNELBASE) {
			ppcmmu_unmap_bat(i);
			bats[i].batinfo_valid = 0;
		}
	}
}

/*
 * Search Bat mappings for a valid mapping for this address 'addr'.
 */
struct batinfo *
ppcmmu_findbat(caddr_t addr)
{
	struct batinfo *bat;
	u_int i;

	for (bat = bats, i = 0; i < 8; i++, bat++) {
		if (bat->batinfo_valid == 0)
			continue;
		if ((addr >= (caddr_t)bat->batinfo_vaddr) &&
			(addr < (bat->batinfo_vaddr + bat->batinfo_size))) {
			return (bat);
		}
	}

	return (NULL); /* no Bat mapping found */
}

/*
 * Kernel lomem memory allocation/freeing
 */

static struct lomemlist {
	struct lomemlist	*lomem_next;	/* next in a list */
	u_long		lomem_paddr;	/* base kernel virtual */
	u_long		lomem_size;	/* size of space (bytes) */
} lomemusedlist, lomemfreelist;

static kmutex_t	lomem_lock;		/* mutex protecting lomemlist data */
static int lomemused, lomemmaxused;
static caddr_t lomem_startva;
/*LINTED static unused */
static caddr_t lomem_endva;
static u_long  lomem_startpa;
static kcondvar_t lomem_cv;
static u_long  lomem_wanted;

/*
 * Space for low memory (below 16 meg), contiguous, memory
 * for DMA use (allocated by ddi_iopb_alloc()).  This default,
 * changeable in /etc/system, allows 2 64K buffers, plus space for
 * a few more small buffers for (e.g.) SCSI command blocks.
 */
long lomempages = (2*64*1024/MMU_PAGESIZE + 4);

#ifdef DEBUG
static int lomem_debug = 0;
#endif DEBUG

void
lomem_init()
{
	register int nfound;
	register int pfn;
	register struct lomemlist *dlp;
	int biggest_group = 0;

	/*
	 * Try to find lomempages pages of contiguous memory below 16meg.
	 * If we can't find lomempages, find the biggest group.
	 * With 64K alignment being needed for devices that have
	 * only 16 bit address counters (next byte fixed), make sure
	 * that the first physical page is 64K aligned; if lomempages
	 * is >= 16, we have at least one 64K segment that doesn't
	 * span the address counter's range.
	 */

again:
	if (lomempages <= 0)
		return;

	for (nfound = 0, pfn = 0; pfn < btop(16*1024*1024); pfn++) {
		if (page_numtopp_alloc(pfn) == NULL) {
			/* Encountered an unallocated page. Back out.  */
			if (nfound > biggest_group)
				biggest_group = nfound;
			for (; nfound; --nfound) {
				/*
				 * Fix Me: cannot page_free here. Causes it to
				 * go to sleep!!
				 */
				page_free(page_numtopp_nolock(pfn-nfound), 1);
			}
			/*
			 * nfound is back to zero to continue search.
			 * Bump pfn so next pfn is on a 64Kb boundary.
			 */
			pfn |= (btop(64*1024) - 1);
		} else {
			if (++nfound >= lomempages)
				break;
		}
	}

	if (nfound < lomempages) {

		/*
		 * Ran beyond 16 meg.  pfn is last in group + 1.
		 * This is *highly* unlikely, as this search happens
		 *   during startup, so there should be plenty of
		 *   pages below 16mb.
		 */

		if (nfound > biggest_group)
			biggest_group = nfound;

		cmn_err(CE_WARN, "lomem_init: obtained only %d of %d pages.\n",
				biggest_group, (int)lomempages);

		if (nfound != biggest_group) {
			/*
			 * The last group isn't the biggest.
			 * Free it and try again for biggest_group.
			 */
			for (; nfound; --nfound) {
				page_free(page_numtopp_nolock(pfn-nfound), 1);
			}
			lomempages = biggest_group;
			goto again;
		}

		--pfn;	/* Adjust to be pfn of last in group */
	}


	/* pfn is last page frame number; compute  first */
	pfn -= (nfound - 1);
	lomem_startva = ppcdevmap(pfn, nfound, PROT_READ|PROT_WRITE);
	lomem_endva = lomem_startva + ptob(lomempages);

	/* Set up first free block */
	lomemfreelist.lomem_next = dlp =
		(struct lomemlist *)kmem_alloc(sizeof (struct lomemlist), 0);
	dlp->lomem_next = NULL;
	dlp->lomem_paddr = lomem_startpa = ptob(pfn);
	dlp->lomem_size  = ptob(nfound);

	/*
	 * Downgrade the page locks to shared locks instead of exclusive
	 * locks.
	 */
	for (; nfound; --nfound) {
		page_downgrade(page_numtopp_nolock(pfn));
		pfn++;
	}

#ifdef DEBUG
	if (lomem_debug)
		printf("lomem_init: %d pages, phys=%x virt=%x\n",
		    (int)lomempages, (int)dlp->lomem_paddr,
		    (int)lomem_startva);
#endif DEBUG

	mutex_init(&lomem_lock, "lomem_lock", MUTEX_DEFAULT, DEFAULT_WT);
	cv_init(&lomem_cv, "lomem_cv", CV_DEFAULT, NULL);
}

/*
 * Allocate contiguous, memory below 16meg.
 * Only used for ddi_iopb_alloc (and ddi_memm_alloc) - os/ddi_impl.c.
 */
caddr_t
lomem_alloc(nbytes, limits, align, cansleep)
	u_int nbytes;
	ddi_dma_lim_t *limits;
	int align;
	int cansleep;
{
	register struct lomemlist *dlp;	/* lomem list ptr scans free list */
	register struct lomemlist *dlpu; /* New entry for used list if needed */
	struct lomemlist *dlpf;	/* New entry for free list if needed */
	struct lomemlist *pred;	/* Predecessor of dlp */
	struct lomemlist *bestpred = NULL;
	register u_long left, right;
	u_long leftrounded, rightrounded;

	if (align > 16) {
		cmn_err(CE_WARN, "lomem_alloc: align > 16\n");
		return (NULL);
	}

	if ((dlpu = (struct lomemlist *)kmem_alloc(sizeof (struct lomemlist),
		cansleep ? 0 : KM_NOSLEEP)) == NULL)
			return (NULL);

	/* In case we need a second lomem list element ... */
	if ((dlpf = (struct lomemlist *)kmem_alloc(sizeof (struct lomemlist),
		cansleep ? 0 : KM_NOSLEEP)) == NULL) {
			kmem_free(dlpu, sizeof (struct lomemlist));
			return (NULL);
	}

	/* Force 16-byte multiples and alignment; great simplification. */
	align = 16;
	nbytes = (nbytes + 15) & (~15);

	mutex_enter(&lomem_lock);

again:
	for (pred = &lomemfreelist; (dlp = pred->lomem_next) != NULL;
	    pred = dlp) {
		/*
		 * The criteria for choosing lomem space are:
		 *   1. Leave largest possible free block after allocation.
		 *	From this follows:
		 *		a. Use space in smallest usable block.
		 *		b. Avoid fragments (i.e., take from end).
		 *	Note: This may mean that we fragment a smaller
		 *	block when we could have allocated from the end
		 *	of a larger one, but c'est la vie.
		 *
		 *   2. Prefer taking from right (high) end.  We start
		 *	with 64Kb aligned space, so prefer not to break
		 *	up the first chunk until we have to.  In any event,
		 *	reduce fragmentation by being consistent.
		 */
		if (dlp->lomem_size < nbytes ||
			(bestpred &&
			dlp->lomem_size > bestpred->lomem_next->lomem_size))
				continue;

		left = dlp->lomem_paddr;
		right = dlp->lomem_paddr + dlp->lomem_size;
		leftrounded = ((left + limits->dlim_adreg_max - 1) &
						~limits->dlim_adreg_max);
		rightrounded = right & ~limits->dlim_adreg_max;

		/*
		 * See if this block will work, either from left, from
		 * right, or after rounding up left to be on an "address
		 * increment" (dlim_adreg_max) boundary.
		 */
		if (lomem_check_limits(limits, right - nbytes, right - 1) ||
		    lomem_check_limits(limits, left, left + nbytes - 1) ||
		    (leftrounded + nbytes <= right &&
			lomem_check_limits(limits, leftrounded,
						leftrounded+nbytes-1))) {
			bestpred = pred;
		}
	}

	if (bestpred == NULL) {
		if (cansleep) {
			if (lomem_wanted == 0 || nbytes < lomem_wanted)
				lomem_wanted = nbytes;
			cv_wait(&lomem_cv, &lomem_lock);
			goto again;
		}
		mutex_exit(&lomem_lock);
		kmem_free(dlpu, sizeof (struct lomemlist));
		kmem_free(dlpf, sizeof (struct lomemlist));
		return (NULL);
	}

	/* bestpred is predecessor of block we're going to take from */
	dlp = bestpred->lomem_next;

	if (dlp->lomem_size == nbytes) {
		/* Perfect fit.  Just use whole block. */
		ASSERT(lomem_check_limits(limits,  dlp->lomem_paddr,
				dlp->lomem_paddr + dlp->lomem_size - 1));
		bestpred->lomem_next = dlp->lomem_next;
		dlp->lomem_next = lomemusedlist.lomem_next;
		lomemusedlist.lomem_next = dlp;
	} else {
		left = dlp->lomem_paddr;
		right = dlp->lomem_paddr + dlp->lomem_size;
		leftrounded = ((left + limits->dlim_adreg_max - 1) &
						~limits->dlim_adreg_max);
		rightrounded = right & ~limits->dlim_adreg_max;

		if (lomem_check_limits(limits, right - nbytes, right - 1)) {
			/* Take from right end */
			dlpu->lomem_paddr = right - nbytes;
			dlp->lomem_size -= nbytes;
		} else if (lomem_check_limits(limits, left, left+nbytes-1)) {
			/* Take from left end */
			dlpu->lomem_paddr = left;
			dlp->lomem_paddr += nbytes;
			dlp->lomem_size -= nbytes;
		} else if (rightrounded - nbytes >= left &&
			lomem_check_limits(limits, rightrounded - nbytes,
							rightrounded - 1)) {
			/* Take from right after rounding down */
			dlpu->lomem_paddr = rightrounded - nbytes;
			dlpf->lomem_paddr = rightrounded;
			dlpf->lomem_size  = right - rightrounded;
			dlp->lomem_size -= (nbytes + dlpf->lomem_size);
			dlpf->lomem_next = dlp->lomem_next;
			dlp->lomem_next  = dlpf;
			dlpf = NULL;	/* Don't free it */
		} else {
			ASSERT(leftrounded + nbytes <= right &&
				lomem_check_limits(limits, leftrounded,
						leftrounded + nbytes - 1));
			/* Take from left after rounding up */
			dlpu->lomem_paddr = leftrounded;
			dlpf->lomem_paddr = leftrounded + nbytes;
			dlpf->lomem_size  = right - dlpf->lomem_paddr;
			dlpf->lomem_next  = dlp->lomem_next;
			dlp->lomem_size = leftrounded - dlp->lomem_paddr;
			dlp->lomem_next  = dlpf;
			dlpf = NULL;	/* Don't free it */
		}
		dlp = dlpu;
		dlpu = NULL;	/* Don't free it */
		dlp->lomem_size = nbytes;
		dlp->lomem_next = lomemusedlist.lomem_next;
		lomemusedlist.lomem_next = dlp;
	}

	if ((lomemused += nbytes) > lomemmaxused)
		lomemmaxused = lomemused;

	mutex_exit(&lomem_lock);

	if (dlpu) kmem_free(dlpu, sizeof (struct lomemlist));
	if (dlpf) kmem_free(dlpf, sizeof (struct lomemlist));

#ifdef DEBUG
	if (lomem_debug) {
		printf("lomem_alloc: alloc paddr 0x%x size %d\n",
		    (int)dlp->lomem_paddr, (int)dlp->lomem_size);
	}
#endif DEBUG
	return (lomem_startva + (dlp->lomem_paddr - lomem_startpa));
}

static int
lomem_check_limits(ddi_dma_lim_t *limits, u_int lo, u_int hi)
{
	return (lo >= limits->dlim_addr_lo && hi <= limits->dlim_addr_hi &&
		((hi & ~(limits->dlim_adreg_max)) ==
			(lo & ~(limits->dlim_adreg_max))));
}

void
lomem_free(kaddr)
	caddr_t kaddr;
{
	register struct lomemlist *dlp, *pred, *dlpf;
	u_long paddr;

	/* Convert kaddr from virtual to physical */
	paddr = (kaddr - lomem_startva) + lomem_startpa;

	mutex_enter(&lomem_lock);

	/* Find the allocated block in the used list */
	for (pred = &lomemusedlist; (dlp = pred->lomem_next) != NULL;
	    pred = dlp)
		if (dlp->lomem_paddr == paddr)
			break;

	if (dlp->lomem_paddr != paddr) {
		cmn_err(CE_WARN, "lomem_free: bad addr=0x%x paddr=0x%x\n",
			(int)kaddr, (int)paddr);
		return;
	}

	lomemused -= dlp->lomem_size;

	/* Remove from used list */
	pred->lomem_next = dlp->lomem_next;

	/* Insert/merge into free list */
	for (pred = &lomemfreelist; (dlpf = pred->lomem_next) != NULL;
	    pred = dlpf) {
		if (paddr <= dlpf->lomem_paddr)
			break;
	}

	/* Insert after pred; dlpf may be NULL */
	if (pred->lomem_paddr + pred->lomem_size == dlp->lomem_paddr) {
		/* Merge into pred */
		pred->lomem_size += dlp->lomem_size;
		kmem_free(dlp, sizeof (struct lomemlist));
	} else {
		/* Insert after pred */
		dlp->lomem_next = dlpf;
		pred->lomem_next = dlp;
		pred = dlp;
	}

	if (dlpf &&
		pred->lomem_paddr + pred->lomem_size == dlpf->lomem_paddr) {
		pred->lomem_next = dlpf->lomem_next;
		pred->lomem_size += dlpf->lomem_size;
		kmem_free(dlpf, sizeof (struct lomemlist));
	}

	if (pred->lomem_size >= lomem_wanted) {
		lomem_wanted = 0;
		cv_broadcast(&lomem_cv);
	}

	mutex_exit(&lomem_lock);

#ifdef DEBUG
	if (lomem_debug) {
		printf("lomem_free: freeing addr 0x%x -> addr=0x%x, size=%d\n",
		    (int)paddr, (int)pred->lomem_paddr, (int)pred->lomem_size);
	}
#endif DEBUG
}

caddr_t
ppcdevmap(pf, npf, prot)
	int pf;
	int npf;
	u_int prot;
{
	caddr_t addr;

	addr = (caddr_t)kmxtob(rmalloc(kernelmap, npf));
	segkmem_mapin(&kvseg, addr, npf * MMU_PAGESIZE, prot | HAT_NOSYNC, pf,
				HAT_LOAD_LOCK);
	return (addr);
}

/*
 * This routine is like page_numtopp, but accepts only free pages, which
 * it allocates (unfrees) and returns with the exclusive lock held.
 * It is used by machdep.c/dma_init() to find contiguous free pages.
 */
page_t *
page_numtopp_alloc(register u_int pfnum)
{
	register page_t *pp;

	pp = page_numtopp_nolock(pfnum);
	if (pp == NULL)
		return ((page_t *)NULL);

	if (!page_trylock(pp, SE_EXCL))
		return (NULL);

	if (!PP_ISFREE(pp)) {
		page_unlock(pp);
		return (NULL);
	}

	/* If associated with a vnode, destroy mappings */

	if (pp->p_vnode) {

		page_destroy_free(pp);

		if (!page_lock(pp, SE_EXCL, (kmutex_t *)NULL, P_NO_RECLAIM))
			return (NULL);
	}

	if (!PP_ISFREE(pp) || !page_reclaim(pp, (kmutex_t *)NULL)) {
		page_unlock(pp);
		return (NULL);
	}

	return (pp);
}

/*
 * Get the translations property - we cache the phandle for the
 * mmu node for future callers.
 * If get_prop is set, we assume translations is a valid buffer and
 * retrieve the property.  Otherwise, we just return the length.
 *
 * Note that checking (translations == NULL) is not a good check to
 * see if we have a valid buffer since this buffer may be alloc'd
 * by prom_alloc() - the prom may not have a problem with returning
 * a buffer at addr 0x0.
 */
static int
get_translations_prop(struct trans *translations, int get_prop)
{

	int len;
	static dnode_t mmunode = (dnode_t)NULL;
	ihandle_t node;

	if (mmunode == (dnode_t)NULL) {
		node = prom_mmu_ihandle();
		ASSERT(node != (ihandle_t)-1);

		mmunode = (dnode_t)prom_getphandle(node);
	}
	ASSERT(mmunode != (dnode_t)-1);

	len = prom_getproplen(mmunode, "translations");

	if (len <= 0) {
		return (-1);
	}
	/*
	 * is the caller only interested in the length?
	 */
	if (get_prop == 0) {
		/* Each translations entry consists of 4 integers */
		return (len >> 4);
	}

	/*
	 * fill in our buffer
	 */
	if (prom_getprop(mmunode, "translations", (caddr_t)translations)
	    < 0) {
		return (-1);
	} else {
		/* Each translations entry consists of 4 integers */
		return (len >> 4);    /* number of translation entries */
	}
}
