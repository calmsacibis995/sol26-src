/*
 * Copyright (c) 1992 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpr_impl.c	1.53	96/06/06 SMI"

/*
 * Platform specific implementation code
 */


#include <sys/types.h>
#include <sys/promif.h>
#include <vm/hat.h>
#include <sys/mmu.h>
#include <sys/iommu.h>
#include <sys/scb.h>
#include <sys/cpuvar.h>
#include <sys/cpu.h>
#include <sys/intreg.h>
#include <sys/pte.h>
#include <sys/clock.h>
#include <sys/systm.h>
#include <sys/machsystm.h>
#include <vm/seg.h>
#include <vm/mach_srmmu.h>
#include <vm/hat_srmmu.h>
#include <sys/cpr.h>
#include <sys/cpr_impl.h>
#include <sys/map.h>
#include <sys/vmmac.h>
#include <vm/page.h>
#include <vm/mach_page.h>

extern u_int va_to_pfn();
extern void vac_flushall(void);
extern int i_ddi_splaudio(void);
extern int spl0(void);

extern	int cpr_setbit(u_int);
u_int i_cpr_count_special_kpages(int);
extern	u_int cpr_kas_page_cnt;

extern int cprboot_magic;
extern int vac;
extern int xc_level_ignore;
extern struct vnode prom_ppages;

struct sun4m_machdep machdep_info;

u_int	i_cpr_count_special_kpages(int);

/*
 * Stop real time clock and all interrupt activities in the system
 */
void
i_cpr_stop_intr()
{
	set_clk_mode(0, IR_ENA_CLK10);  /* stop real time clock */
	set_clk_mode(0, IR_ENA_CLK14);  /* stop real time clock */
	splzs(); /* block all network traffic and everything */
}

void
i_cpr_disable_clkintr()
{
	set_clk_mode(0, IR_ENA_CLK10);  /* stop real time clock */
	set_clk_mode(0, IR_ENA_CLK14);  /* stop real time clock */
}

/*
 * Set machine up to take interrupts
 */
void
i_cpr_enable_intr()
{
	/* Enable interrupt */
	set_clk_mode(IR_ENA_CLK14, 0);  /* start real time clock */
	clkstart();			/* start the realtime clock */

	set_intmask(IR_ENA_INT, 0);

	(void) spl0();			/* allow all interrupts */
}

/*
 * Enable the level 10 clock interrupt.
 */
void
i_cpr_enable_clkintr()
{
	set_clk_mode(IR_ENA_CLK14, 0);  /* start real time clock */
	clkstart();			/* start the realtime clock */
}

/*
 * Write necessary machine dependent information to cpr state file,
 * eg. sun4m pte table ...
 */
int
i_cpr_write_machdep(vnode_t *vp)
{
	struct cpr_machdep_desc cmach;
	int rc;

	cmach.md_magic = (u_int) CPR_MACHDEP_MAGIC;
	cmach.md_size = sizeof (struct sun4m_machdep);

	if (rc = cpr_write(vp, (caddr_t)&cmach, sizeof (cmd_t))) {
		errp("cpr_write_machdep: Failed to write desc\n");
		return (rc);
	}

	/*
	 * Get info for current mmu ctx and l1 ptp pointer and
	 * write them out to the state file.
	 * Pack them into one buf and do 1 write.
	 */
	machdep_info.mmu_ctp = mmu_getctp();
	machdep_info.mmu_ctx = mmu_getctx();
	machdep_info.mmu_ctl = mmu_getcr();

	rc = cpr_write(vp, (caddr_t)&machdep_info, cmach.md_size);

	return (rc);
}

void
i_cpr_save_machdep_info()
{
}

/*
 * Initial setup to get the system back to an operable state.
 * 	1. Restore IOMMU
 */
void
i_cpr_machdep_setup()
{
	union iommu_ctl_reg ioctl_reg;
	union iommu_base_reg iobase_reg;
	extern iommu_pte_t *phys_iopte;

	if (cache & CACHE_VAC) {
		if (use_cache) {
			cache_init();
			turn_cache_on(getprocessorid());
		}
	}

	/* load iommu base addr */
	iobase_reg.base_uint = 0;
	iobase_reg.base_reg.base = ((u_int)phys_iopte) >> IOMMU_PTE_BASE_SHIFT;
	iommu_set_base(iobase_reg.base_uint);

	/* set control reg and turn it on */
	ioctl_reg.ctl_uint = 0;
	ioctl_reg.ctl_reg.enable = 1;
	ioctl_reg.ctl_reg.range = IOMMU_CTL_RANGE;
	iommu_set_ctl(ioctl_reg.ctl_uint);

	DEBUG2(errp("IOMMU:iobs_reg.base_reg.base %x "
			"ioctl_reg.ctl_reg.range %x\n",
			iobase_reg.base_reg.base, ioctl_reg.ctl_reg.range));

	/* kindly flush all tlbs for any leftovers */
	iommu_flush_all();

	/*
	 * Install PROM callback handler (give both, promlib picks the
	 * appropriate handler).
	 */
	if (prom_sethandler(NULL, vx_handler) != 0)
		cmn_err(CE_PANIC, "CPR cannot re-register sync callback");

	DEBUG2(errp("i_cpr_machdep_setup: Set up IOMMU\n"));
}

u_int
i_cpr_va_to_pfn(caddr_t vaddr)
{
	return (va_to_pfn((caddr_t) vaddr));
}

void
i_cpr_set_tbr()
{
	(void) set_tbr(&scb);
}

/*
 * The bootcpu is always 0 on sun4m.
 */
i_cpr_find_bootcpu()
{
	return (0);
}

extern u_int cpr_get_mcr(void);

int
cpr_is_supported()
{
	return (1);
}

timestruc_t
i_cpr_todget()
{
	timestruc_t ts;

	mutex_enter(&tod_lock);
	ts = tod_get();
	mutex_exit(&tod_lock);
	return (ts);
}

/*
 * XXX These should live in the cpr state struct
 * XXX as impl private void * thingies
 */
caddr_t cpr_vaddr = NULL;
struct pte *cpr_pte = NULL;

/*
 * Return the virtual address of the mapping area
 */
caddr_t
i_cpr_map_setup(void)
{
	ulong_t vpage, alignpage;
	caddr_t vaddr, end;

	/*
	 * Allocate a big enough chunk of kernel virtual that we can be
	 * assured it will span a full level3 pte, then give back the part
	 * we don't need.  Code below assumes that Sysbase is aligned
	 * on a level3 boundary.
	 */
	ASSERT(((int)kmxtob(0) & ((NL3PTEPERPT * MMU_PAGESIZE) - 1)) == 0);
	vpage = rmalloc(kernelmap, (NL3PTEPERPT * 2) - 1);
	if (vpage == NULL) {
		return (NULL);
	}
	alignpage = (vpage + NL3PTEPERPT - 1) & ~(NL3PTEPERPT - 1);
	if (alignpage == vpage) {
		rmfree(kernelmap, NL3PTEPERPT - 1, vpage + NL3PTEPERPT);
	} else {
		ulong_t presize, postsize;
		presize = alignpage - vpage;
		if (presize != 0)
			rmfree(kernelmap, presize, vpage);
		postsize = NL3PTEPERPT - 1 - presize;
		if (postsize != 0)
			rmfree(kernelmap, postsize, alignpage + NL3PTEPERPT);
	}
	vaddr = kmxtob(alignpage);
	end = kmxtob(alignpage + NL3PTEPERPT);
	/*
	 * Now get ptes for the range, mark each invalid
	 *
	 */
	while (vaddr < end) {
		struct pte *pte;
		union ptes *ptep;
		kmutex_t *mtx;
		ptbl_t *ptbl;
		extern struct pte *srmmu_ptealloc(struct as *, caddr_t,
		    int, ptbl_t **, kmutex_t **, int);
#ifdef	DEBUG
		struct pte *prev_pte;
		struct ptbl *prev_ptbl;
		/*LINTED*/
		prev_pte = pte;		/* used before set, but value ignored */
		/*LINTED*/
		prev_ptbl = ptbl;	/* used before set, but value ignored */
#endif
		pte = srmmu_ptealloc(&kas, vaddr, 3, &ptbl, &mtx, 0);
		ptep = (union ptes *)pte;
		ptep->pte_int = MMU_ET_INVALID;

		if (cpr_vaddr == NULL) {
			cpr_vaddr = vaddr;
			cpr_pte = pte;
#ifdef	DEBUG
			prev_ptbl = ptbl;
#endif
		} else {
			/*EMPTY*/
			ASSERT(pte == prev_pte + 1);
			ASSERT(ptbl == prev_ptbl);
		}
		ASSERT(ptbl->ptbl_flags & PTBL_KEEP);
		ASSERT(PTBL_LEVEL(ptbl->ptbl_flags) == 3);

		unlock_ptbl(ptbl, mtx);
		vaddr += MMU_PAGESIZE;
	}
	return (cpr_vaddr);
}

/*
 * Map pages into the kernel's address space at the  location computed
 * by i_cpr_map_init above
 * We have already allocated a ptbl, a pte and a piece of kernelmap.
 * All we need to do is to plug in the new mapping and deal with
 * the TLB and vac.
 */

void
i_cpr_mapin(caddr_t vaddr, u_int len, u_int pf)
{
	struct pte *pte = cpr_pte;
	union ptpe rp;
	register int i;

	ASSERT(cpr_pte != NULL);
	ASSERT(vaddr == cpr_vaddr);
	ASSERT(len <= NL3PTEPERPT);

#ifdef	VAC
	if (vac)
		vac_flushall();
#endif
	for (i = 0; i < len; i++, pf++, pte++, vaddr += MMU_PAGESIZE) {
		/*
		 * XXX tlbflush probably only is  useful on unmapping
		 */
		srmmu_tlbflush(3, vaddr, KCONTEXT, FL_LOCALCPU);
		rp.ptpe_int = PTEOF(0, pf, MMU_STD_SRX, 1);
		*pte = rp.pte;
	}
}

void
i_cpr_mapout(caddr_t vaddr, u_int len)
{
	struct pte *pte = cpr_pte;
	union ptpe rp;
	u_int pf = pte->PhysicalPageNumber;
	register int i;

	ASSERT(vaddr == cpr_vaddr);
	ASSERT(len <= NL3PTEPERPT);

	/*
	 * It is a bit excessive to invalidate the pte given that we will
	 * do this when we're all done, but it does at least give us
	 * a consistent red zone effect
	 */
	for (i = 0; i < len; i++, pf++, pte++, vaddr += MMU_PAGESIZE) {
		ASSERT(pte->PhysicalPageNumber == pf);
		rp.ptpe_int = MMU_ET_INVALID;
		*pte = rp.pte;
		srmmu_tlbflush(3, vaddr, KCONTEXT, FL_LOCALCPU);
	}
}

/*
 * We're done using the mapping area, clean it up and give it back
 * We re-invalidate all the l3 ptes, since the page they live in will
 * have been copied out while they were in use, and the kernel will panic
 * if it finds a valid entry when it tries to reuse it after we free
 * up the kernel virtual space that corresponds to it
 */
void
i_cpr_map_destroy(void)
{
	struct pte *pte = cpr_pte;
	union ptpe rp;
	caddr_t vaddr = cpr_vaddr;
	register int i;

	for (i = 0; i < NL3PTEPERPT; i++, pte++, vaddr += MMU_PAGESIZE) {
		rp.ptpe_int = MMU_ET_INVALID;
		*pte = rp.pte;
		srmmu_tlbflush(3, vaddr, KCONTEXT, FL_LOCALCPU);
	}
	/*
	 * It looks like we don't have to do anything about the ptbl,
	 * it will just get reused when needed
	 */
	rmfree(kernelmap, NL3PTEPERPT, btokmx(cpr_vaddr));
	cpr_vaddr = NULL;
	cpr_pte = NULL;
}

void
i_cpr_vac_ctxflush(void)
{

#ifdef	VAC
	if (vac) {
		XCALL_PROLOG
		vac_allflush(FL_TLB_CACHE);
		XCALL_EPILOG
		vac_ctxflush(0, FL_LOCALCPU);
	}
#endif
}

void
i_cpr_handle_xc(u_int flag)
{
	xc_level_ignore = flag;
}

void
i_cpr_read_prom_mappings()
{
}

u_int
i_cpr_count_special_kpages(int flag)
{
	struct page *pp;
	u_int pfn;
	int tot_pages = 0;

	/*
	 * Hack: page 2 & 3 (msgbuf kluge) are not counted as kas in
	 * startup, so we need to manually tag them.
	 */
	if (flag == CPR_TAG) {
		if (cpr_setbit(2) == 0 && cpr_setbit(3) == 0)
			tot_pages += 2;
	} else
		tot_pages += 2;

	/*
	 * prom allocated kernel mem is hardcoded into prom_ppages vnode.
	 */
	pp = prom_ppages.v_pages;
	while (pp) {
		pfn = ((machpage_t *)pp)->p_pagenum;
		if (pfn != (u_int)-1 && pf_is_memory(pfn)) {
			if (flag == CPR_TAG) {
				if (!cpr_setbit(pfn))
					tot_pages++;
			} else
				tot_pages++;
		}
		if ((pp = pp->p_vpnext) == prom_ppages.v_pages)
			break;
	}

	return (tot_pages);
}
