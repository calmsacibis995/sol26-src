/*
 * Copyright (c) 1994 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpr_impl.c	1.43	96/09/20 SMI"

/*
 * Platform specific implementation code
 */

#include <sys/types.h>
#include <sys/sunddi.h>
#include <sys/promif.h>
#include <sys/prom_isa.h>
#include <sys/prom_plat.h>
#include <sys/mmu.h>
#include <vm/hat_sfmmu.h>
#include <sys/iommu.h>
#include <sys/scb.h>
#include <sys/cpuvar.h>
#include <sys/intreg.h>
#include <sys/pte.h>
#include <vm/hat.h>
#include <vm/page.h>
#include <vm/as.h>
#include <sys/cpr.h>
#include <sys/cpr_impl.h>
#include <sys/map.h>
#include <sys/vmmac.h>
#include <sys/clock.h>
#include <sys/kmem.h>
#include <vm/seg_kmem.h>
#include <sys/debug/debug.h>
#include <sys/msgbuf.h>
#include <sys/cpu_module.h>

/* for exporting the mapping info to prom */
#include <sys/spitasi.h>

static void i_cpr_save_va_to_tte();
extern u_int va_to_pfn(caddr_t);
extern int spl0(void);
extern int spl8(void);
extern int splzs(void);
extern int splhi(void);
extern void i_ddi_splx(int);
extern int i_ddi_splaudio(void);
extern int disable_vec_intr();
extern void kadb_promsync(int, void *);
extern void i_cpr_read_maps(caddr_t, u_int);
extern char i_cpr_end_jumpback[1];

extern int cprboot_magic;
extern cpu_t cpu0;
extern struct scb trap_table;
static size_t cpr_omap_buf_size;
static void *cpr_omap_buf;
static size_t cpr_nmap_buf_size;
static void *cpr_nmap_buf;
static char *cpr_obp_tte_str;
static size_t cpr_obp_tte_strlen;
static caddr_t kadb_defer_word;
static size_t kadb_defer_word_strlen;

struct sun4u_machdep m_info;
static struct prom_retain_info cpr_prom_retain[CPR_PROM_RETAIN_CNT];
caddr_t cpr_vaddr = NULL;


extern void tickint_program(), gen_clk_int();
extern void tickint_clnt_sub();
extern void tickint_clnt_add();

void i_cpr_disable_clkintr();

static int i_cpr_is_in_new_map(u_int);
static void i_cpr_unmap_oprom_pages();
static void i_cpr_map_nprom_pages();
static void i_cpr_fix_prom_page(u_int);
static void i_cpr_hat_kern_restore();

#define	SYS_CLK_INTERVAL	10000

u_int	i_cpr_count_special_kpages(int);
extern	int	cpr_setbit(u_int);
extern	int	cpr_test_mode;

/*
 * Stop real time clock and all interrupt activities in the system
 */
void
i_cpr_stop_intr()
{
	int s;
	longlong_t constant = 0;

	(void) splaudio();

	/* Stop the level14 and level10 clocks */
	s = spl8();
	tickint_clnt_sub(gen_clk_int);	/* Disable level10 interrupts */
	tickint_program(constant);	/* Disable tick registers interrupts */
	splx(s);
}

/*
 * Set machine up to take interrupts
 */
void
i_cpr_enable_intr()
{
	tickint_clnt_add(gen_clk_int, SYS_CLK_INTERVAL);
	(void) spl0();
}

void
i_cpr_disable_clkintr()
{
	int s;
	longlong_t constant = 0;

	s = spl8();
	tickint_clnt_sub(gen_clk_int);
	tickint_program(constant);
	splx(s);
}

/*
 * Enable the level 10 clock interrupt.
 */
void
i_cpr_enable_clkintr()
{
	tickint_clnt_add(gen_clk_int, SYS_CLK_INTERVAL);
}

/*
 * Write necessary machine dependent information to cpr state file,
 * eg. sun4u mmu ctx secondary for the current running process (cpr) ...
 */
int
i_cpr_write_machdep(vnode_t *vp)
{
	struct cpr_machdep_desc cmach;
	int rc, j, ctxnum;
	u_int i;
	tte_t	tte;
	caddr_t addr;
	caddr_t textva, datava;
	extern char end[];

	textva = (caddr_t)(KERNELBASE & MMU_PAGEMASK4M);
	datava = datava = (caddr_t)((u_int)end & MMU_PAGEMASK4M);

	cmach.md_magic = (u_int) CPR_MACHDEP_MAGIC;
	cmach.md_size = sizeof (struct sun4u_machdep) + cpr_obp_tte_strlen
	    + kadb_defer_word_strlen;

	if (rc = cpr_write(vp, (caddr_t)&cmach, sizeof (cmd_t))) {
		errp("cpr_write_machdep: Failed to write desc\n");
		return (rc);
	}

	m_info.mmu_ctx_pri = sfmmu_getctx_pri();
	m_info.mmu_ctx_sec = sfmmu_getctx_sec();

	j = 0;
	m_info.dtte_cnt = 0;
	for (i = 0; i < dtlb_entries; i++) {
		dtlb_rd_entry(i, &tte, &addr, &ctxnum);
		if (TTE_IS_VALID(&tte) && TTE_IS_LOCKED(&tte)) {
			if ((addr == textva) || (addr == datava)) {
				m_info.dtte[j].no = i;
				m_info.dtte[j].va_tag = addr;
				m_info.dtte[j].ctx = ctxnum;
				m_info.dtte[j].tte.tte_inthi = tte.tte_inthi;
				m_info.dtte[j].tte.tte_intlo = tte.tte_intlo;
				m_info.dtte_cnt++;
				j++;
			}

		}
	}

	j = 0;
	m_info.itte_cnt = 0;
	for (i = 0; i < itlb_entries; i++) {
		itlb_rd_entry(i, &tte, &addr, &ctxnum);
		if (TTE_IS_VALID(&tte) && TTE_IS_LOCKED(&tte)) {
			if (addr == textva) {
				m_info.itte[j].no = i;
				m_info.itte[j].va_tag = addr;
				m_info.itte[j].ctx = ctxnum;
				m_info.itte[j].tte.tte_inthi = tte.tte_inthi;
				m_info.itte[j].tte.tte_intlo = tte.tte_intlo;
				m_info.itte_cnt++;
				j++;
			}

		}
	}

	/*
	 * Get info for current mmu ctx, write them out to the
	 * state file. Pack them into one buf and do 1 write.
	 *
	 * Later on if we need to support MP that boot from other
	 * cpu, we can record the cpuid that suspend was running
	 * on and then let prom set the same cpu to let resume to
	 * run on before jump back to kernel.
	 */
	m_info.tra_va = (caddr_t)&trap_table;
	m_info.mapbuf_va = (caddr_t) cpr_nmap_buf;
	m_info.mapbuf_pfn = i_cpr_va_to_pfn((caddr_t) cpr_nmap_buf);
	m_info.mapbuf_size = (u_int) cpr_nmap_buf_size;
	m_info.curt_pfn = i_cpr_va_to_pfn((caddr_t)curthread);
	m_info.qsav_pfn =
		i_cpr_va_to_pfn((caddr_t)(&ttolwp(curthread)->lwp_qsav));

	m_info.test_mode = cpr_test_mode;

	/* prom retained pages */
	for (i = 0; i < CPR_PROM_RETAIN_CNT; i++) {
		m_info.retain[i].svaddr = cpr_prom_retain[i].svaddr;
		m_info.retain[i].spfn = cpr_prom_retain[i].spfn;
		m_info.retain[i].cnt = cpr_prom_retain[i].cnt;
	}

	if (rc = cpr_write(vp, (caddr_t)&m_info,
	    sizeof (struct sun4u_machdep))) {
		errp("cpr_write_machdep: Failed to write machdep info\n");
		return (rc);
	}

	if (cpr_obp_tte_strlen)
		if (rc = cpr_write(vp, cpr_obp_tte_str, cpr_obp_tte_strlen)) {
			errp("cpr_write_machdep: Failed to write translation "
			    "machdep info\n");
			return (rc);
		}

	if (kadb_defer_word_strlen)
		if (rc = cpr_write(vp,
		    kadb_defer_word, kadb_defer_word_strlen)) {
			errp("cpr_write_machdep: Failed to write kadb "
			    "machdep info\n");
		}

	return (rc);
}

/*
 * Save miscellaneous information which needs to be written to the
 * state file.  This information is required to re-initialize
 * kernel/prom handshaking.
 */
void
i_cpr_save_machdep_info()
{
	/*
	 * Verify that the jumpback code has not exceeded one page.  For
	 * this test to work, i_cpr_read_maps() must be the first
	 * function in the cpr binary.
	 */
	DEBUG5(errp("i_cpr_save_machdep_info: jumpback size = 0x%x\n",
	    (caddr_t) &i_cpr_end_jumpback - (caddr_t) i_cpr_read_maps));

	if ((u_int)&i_cpr_end_jumpback & MMU_PAGEMASK !=
		(u_int) i_cpr_read_maps & MMU_PAGEMASK)
		cmn_err(CE_PANIC, "cpr: jumpback code exceeds one page.\n");

	/*
	 * Save the information about our sw ttes. This will be
	 * used to teach obp how to parse our sw ttes.
	 */
	i_cpr_save_va_to_tte();

	/*
	 * If kadb is running, get a copy of the strings which it
	 * exports to the prom.
	 */
	kadb_defer_word_strlen = 0;
	if (dvec != NULL) {
		kadb_defer_word = NULL;
		kadb_promsync(KADB_CALLB_FORMAT, (void *) &kadb_defer_word);
		if (kadb_defer_word != NULL)
			kadb_defer_word_strlen = strlen(kadb_defer_word) + 1;
	}
}


void
i_cpr_machdep_setup()
{
	extern  int callback_handler(cell_t *arg_array);

	/*
	 * XXX We need to revisit startup code to see if there
	 * are anything else need to be restored, e.g. virt_avail etc.
	 */
	i_cpr_hat_kern_restore();

	prom_interpret("' unix-tte is va>tte-data", 0, 0, 0, 0, 0);

	(void) prom_set_callback((void *)callback_handler);

	if (dvec != NULL) {
		kadb_promsync(KADB_CALLB_ARM, (void *) 0);
	}
}

/*
 * This function traverses the old prom mapping list and
 * unload mappings for the prom pages which are not in the new
 * prom mapping list. Prom routines may not be accessible if
 * the old mapping for that is in conflict with the new mapping.
 * XXX It is assumed that USERLIMIT is the prom virtual address base.
 */
static void
i_cpr_unmap_oprom_pages()
{
	struct translation *promt;
	int size;
	u_int offset;
	u_int oldpfn;
	u_int vaddr;
	tte_t tte, *ttep;

	DEBUG5(errp("\nChecking i_cpr_unmap_oprom_pages:\n"));
	DEBUG5(errp("---------------------------------\n"));

	ttep = &tte;
	for (promt = cpr_omap_buf; promt && promt->tte_hi; promt++) {

		/* Skip kernel mappings */
		if (promt->virt_lo < USERLIMIT)
			continue;

		ttep->tte_inthi = promt->tte_hi;
		ttep->tte_intlo = promt->tte_lo;
		size = promt->size_lo;
		offset = 0;

		while (size) {
			vaddr = promt->virt_lo + offset;

			DEBUG5(errp("Checking vaddr %x\n", vaddr));

			if ((oldpfn = sfmmu_vatopfn((caddr_t)vaddr, KHATID))
				!= -1) {
				DEBUG5(errp("vaddr: %x oldpfn: %x\n\n", vaddr,
					oldpfn));
			} else {
				prom_panic("%x is not mapped in kernel.");
			}

			if (!i_cpr_is_in_new_map(vaddr)) {
				DEBUG5(errp("--->%x does not exist in new "
					"mapping.\n"));
				DEBUG5(errp("Unloading %x\n", vaddr));
				hat_unload(kas.a_hat, (caddr_t)vaddr,
				    MMU_PAGESIZE, HAT_UNLOAD_UNLOCK);
				if (sfmmu_vatopfn((caddr_t)vaddr, KHATID) != -1)
					prom_panic("hat_unload failed!");
				else
					DEBUG5(errp("Unloaded"));
			}

			size -= MMU_PAGESIZE;
			offset += MMU_PAGESIZE;
		}
	}
}


static int
i_cpr_is_in_new_map(u_int vaddr)
{
	u_int svaddr, evaddr;
	struct translation *promt;

	for (promt = cpr_nmap_buf; promt && promt->tte_hi; promt++) {

		/* Skip kernel mappings */
		if (promt->virt_lo < USERLIMIT)
			continue;

		svaddr = promt->virt_lo;
		evaddr = promt->virt_lo + promt->size_lo - 1;

		if (vaddr >= svaddr && vaddr <= evaddr)
			return (1);
	}

	return (0);
}

/*
 * This function traverses the new prom mapping list and creates
 * equivalent mappings in the sfmmu mapping hash for prom pages if
 * they are not existing in old mappings. Since this is called
 * after i_cpr_unmap_oprom_pages, there should be no conflicts.
 * XXX It is assumed that USERLIMIT is the prom virtual address base.
 */
static void
i_cpr_map_nprom_pages()
{
	struct translation *promt;
	tte_t tte, *ttep;
	u_int offset;
	u_int pfn, oldpfn, basepfn;
	u_int vaddr;
	int size;
	u_int attr, flags;
	extern int pf_is_memory(u_int);

	ttep = &tte;
	flags = HAT_LOAD_LOCK | SFMMU_NO_TSBLOAD;
	DEBUG5(errp("\nChecking i_cpr_map_nprom_pages:\n"));
	DEBUG5(errp("--------------------------------\n"));

	for (promt = cpr_nmap_buf; promt && promt->tte_hi; promt++) {

		/* Skip kernel mappings */
		if (promt->virt_lo < USERLIMIT)
			continue;

		/* can we change this to PROC_TEXT? */
		attr = PROC_DATA | HAT_NOSYNC;

		ttep->tte_inthi = promt->tte_hi;
		ttep->tte_intlo = promt->tte_lo;


		if (TTE_IS_GLOBAL(ttep)) {
			/*
			 * The prom better not use global translations
			 * because a user process might use the same
			 * virtual addresses
			 */
			prom_panic("map_nprom_pages: global tte");
		}
		if (TTE_IS_LOCKED(ttep)) {
			/* clear the lock bit */
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

		/*
		 * Since this is still just a 32 bit machine ignore
		 * virth_hi and size_hi
		 */
		size = promt->size_lo;
		offset = 0;
		basepfn = TTE_TO_PFN((caddr_t)promt->virt_lo, ttep);
		while (size) {
			vaddr = promt->virt_lo + offset;
			pfn = basepfn + mmu_btop(offset);

			DEBUG5(errp("Checking vaddr %x\n", vaddr));

			if (pf_is_memory(pfn)) {
				if (attr & SFMMU_UNCACHEPTTE)
					prom_panic("prom mapped mem uncached");
			} else {
				if (!(attr & SFMMU_SIDEFFECT))
					prom_panic("prom mapped io with no e");
			}

			/*
			 * Check to see if kernel already has a mapping for
			 * this pfn.
			 */
			if ((oldpfn = sfmmu_vatopfn((caddr_t)vaddr, KHATID)) ==
				-1) {
				/*
				 * No. Kernel does not know about this mapping
				 * We will have to map it in later.
				 */
				DEBUG5(errp("--->No oldpfn. vaddr: %x\n",
					vaddr));
			} else {
				/*
				 * Yes. Kernel has a mapping for the vaddr.
				 * Let's check to see whether it's the same as
				 * prom mapping or not.
				 */
				if (pfn == oldpfn) {
					/*
					 * Yes. It's the same, so do nothing and
					 * skip to next page.
					 */
					DEBUG5(errp("Same. vaddr: %x oldpfn: "
						"%x newpfn: %x\n\n", vaddr,
						oldpfn, pfn));
					goto skip;
				} else {
					/*
					 * No. There is conflict here!
					 * Let's unmap this kernel mapping now
					 * and map it back into kernel to match
					 * prom's mapping later.
					 */
					DEBUG5(errp("--->Conflict! vaddr: %x "
						"oldpfn: %x newpfn: %x\n",
						vaddr, oldpfn, pfn));
					DEBUG5(errp("Unmapping vaddr %x\n",
						vaddr));
					hat_unload(kas.a_hat, (caddr_t)vaddr,
					    MMU_PAGESIZE, HAT_UNLOAD_UNLOCK);

					/*
					 * Make sure the page is indeed
					 * unmapped.
					 */
					if (sfmmu_vatopfn((caddr_t)vaddr,
						KHATID) != -1) {
						DEBUG5(errp("hat_unload() "
							"failed!\n"));
						prom_panic("hat_unload:");
					} else {
						DEBUG5(errp("Unloaded.\n"));
						/*
						 * XXX need to claim this
						 * page otherwise we will have
						 * memory leak.
						 */
					}
				}
			}


			/*
			 * We are here because there is one page needs to be
			 * mapped into kernel.
			 */
			DEBUG5(errp("Mapping  vaddr %x\n", vaddr));

			if (size >= MMU_PAGESIZE4M &&
				!(vaddr & MMU_PAGEOFFSET4M) &&
				!(mmu_ptob(pfn) & MMU_PAGEOFFSET4M)) {
				sfmmu_memtte(ttep, pfn, attr, TTE4M);
				sfmmu_tteload(kas.a_hat, ttep, (caddr_t)vaddr,
					NULL, flags);
				size -= MMU_PAGESIZE4M;
				offset += MMU_PAGESIZE4M;
				continue;
			}

			sfmmu_memtte(ttep, pfn, attr, TTE8K);
			sfmmu_tteload(kas.a_hat, ttep, (caddr_t)vaddr,
				NULL, flags);
			/*
			 * Make sure the page has been mapped into kernel
			 * successfully.
			 */
			if ((pfn = sfmmu_vatopfn((caddr_t)vaddr, KHATID)) !=
				-1) {
				DEBUG5(errp("Mapped\n"));
			} else {
				DEBUG5(errp("sfmmu_tteload failed!\n"));
				prom_panic("sfmmu_tteload:");
			}

			/*
			 * This page is now used by prom, so we need to
			 * remove it from the free lists and etc in the
			 * resumed kernel.
			 */
			i_cpr_fix_prom_page(pfn);

skip:
			DEBUG5(errp("\n"));
			size -= MMU_PAGESIZE;
			offset += MMU_PAGESIZE;
		}
	}
}

/*
 * We are passed a pfn which the prom was not using before the checkpoint
 * but is using now.  Since the page was not claimed by cprbooter for one
 * of the resume kernel pages, and since the old prom state instance was
 * not using it, it may have been on the free list.  We get a pointer to
 * its page struct and pull it off the freelist to prevent the kernel
 * from assigning it.
 */
static void
i_cpr_fix_prom_page(u_int pfnum)
{
	page_t	*pp;

	DEBUG5(errp("fix: called with pfn = %x\n", pfnum));
	if ((pp = page_numtopp_nolock(pfnum)) != NULL) {
		DEBUG5(errp("fix: found in page array\n"));
		if (se_nolock(&pp->p_selock) && PP_ISFREE(pp)) {
			DEBUG5(errp("fix: selock=%x free=%x\n",
				pp->p_selock, PP_ISFREE(pp)));
			/*
			 * Remove it from the freelist and lock it.
			 */
			if (page_trylock(pp, SE_EXCL) == 0)
				cmn_err(CE_PANIC, "prom page locked");

			(void) page_reclaim(pp, NULL);
		}
	}
}


static void
i_cpr_hat_kern_restore()
{
	/*
	 * The mappings for prom pages are different after resume,
	 * so we need to update the translations. Kernel pages are
	 * fine.
	 */
	if (CPR->c_substate != C_ST_STATEF_ALLOC_RETRY) {
		if (ncpus > 1) {
			sfmmu_init_tsbs();
		}

		/* Only if the prom has done a reset */

		i_cpr_unmap_oprom_pages();
		i_cpr_map_nprom_pages();

		/* Free up translation buf space. */
		kmem_free(cpr_omap_buf, cpr_omap_buf_size);
		kmem_freepages((void *)cpr_nmap_buf,
				btop(cpr_nmap_buf_size));
	}
}



u_int
i_cpr_va_to_pfn(caddr_t vaddr)
{
	return (va_to_pfn((caddr_t)vaddr));
}

void
i_cpr_set_tbr()
{

}

/*
 * The bootcpu is cpu0 on sun4u.
 */
i_cpr_find_bootcpu()
{
	return (cpu0.cpu_id);
}

extern int cpr_get_mcr(void);

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
 * Return the virtual address of the mapping area
 */
caddr_t
i_cpr_map_setup(void)
{
	ulong_t vpage, alignpage;
	caddr_t vaddr;
	tte_t tte;

	/*
	 * Allocate a virtual memory range spanned by an hmeblk.
	 * This would be 8 hments or 64k bytes. But starting VA
	 * must be 64k (8-page) aligned so we allocate a large chunk and
	 * give back what we don't use.
	 */

	vpage = rmalloc(kernelmap, ((NHMENTS * 2) -1));
	if (vpage == NULL) {
		return (NULL);
	}
	alignpage = (vpage + NHMENTS -1) & ~(NHMENTS -1);
	if (alignpage == vpage) {
		rmfree(kernelmap, NHMENTS - 1, vpage + NHMENTS);
	} else {
		ulong_t presize, postsize;
		presize = alignpage - vpage;
		if (presize != 0)
			rmfree(kernelmap, presize, vpage);
		postsize = NHMENTS - 1 - presize;
		if (postsize != 0)
			rmfree(kernelmap, postsize, alignpage + NHMENTS);
	}
	vaddr = (caddr_t) kmxtob(alignpage);

	/*
	 * Now load up a translation for pfn = 0 just so an hmeblk is hashed in
	 * for our special cpr vaddr. We also use pp = 0 so we don't add
	 * a p_mapping entry.
	 */
	sfmmu_memtte(&tte, 0, PROT_READ | HAT_NOSYNC, TTE8K);
	sfmmu_tteload(kas.a_hat, &tte, vaddr, NULL, HAT_LOAD_LOCK);

	cpr_vaddr = vaddr;
	return (cpr_vaddr);
}

/*
 * Map pages into the kernel's address space at the  location computed
 * by i_cpr_map_setup.
 */
void
i_cpr_mapin(caddr_t vaddr, u_int len, u_int pf)
{
	register int i;
	tte_t tte;
	u_int attr = PROT_READ | PROT_WRITE | HAT_NOSYNC;

	ASSERT(vaddr == cpr_vaddr);
	ASSERT(len <= NHMENTS);

	for (i = 0; i < len; i++, pf++, vaddr += MMU_PAGESIZE) {
		/* unload previous mappings */
		hat_unload(kas.a_hat, vaddr, MMU_PAGESIZE, HAT_UNLOAD_UNLOCK);

		/*
		 * use pp = 0 so we don't add a p_mapping entry
		 */
		sfmmu_memtte(&tte, pf, attr, TTE8K);
		sfmmu_tteload(kas.a_hat, &tte, vaddr, NULL, HAT_LOAD_LOCK);
	}
}

/* ARGSUSED */
void
i_cpr_mapout(caddr_t vaddr, u_int len)
{
}

/*
 * We're done using the mapping area; unload mappings created during
 * cpr dump, and release virtual space back to kernelmap.
 */
void
i_cpr_map_destroy(void)
{
	caddr_t vaddr = cpr_vaddr;
	register int i;

	for (i = 0; i < NHMENTS; i++, vaddr += MMU_PAGESIZE) {
		hat_unload(kas.a_hat, vaddr, PAGESIZE, HAT_UNLOAD_UNLOCK);
	}

	rmfree(kernelmap, NHMENTS, btokmx(cpr_vaddr));
	cpr_vaddr = NULL;
}

void
i_cpr_vac_ctxflush(void)
{
}

/* ARGSUSED */
void
i_cpr_handle_xc(int flag)
{
}

/*
 * Allocate a buffer and read the "translations" prom property.  This
 * contains all of the prom's mappings imported by the kernel at
 * startup, but not now available in its original form.  Space for one
 * additional translation is allocated to mark the end of the mappings.
 *
 * Also allocate a page-aligned buffer large enough to hold the
 * new version of the same property after the prom has done a reset
 * and reinitialized its mappings.  A pointer to the buffer will be
 * made available to cprboot to fill in.
 */
void
i_cpr_read_prom_mappings()
{
	char *prop = "translations";
	int translen;
	dnode_t node;

	/*
	 * the "translations" property is associated with the mmu node
	 */
	node = (dnode_t)prom_getphandle(prom_mmu_ihandle());

	translen = prom_getproplen(node, prop);

	cpr_omap_buf_size = translen + sizeof (struct translation);
	cpr_omap_buf = kmem_alloc(cpr_omap_buf_size, KM_SLEEP);

	(void) prom_getprop(node, prop, (caddr_t) cpr_omap_buf);

	/* clear the last entry for sfmmu_map_prom_mappings */
	bzero((void *)((caddr_t)cpr_omap_buf + translen),
			sizeof (struct translation));

	cpr_nmap_buf_size = roundup(CPR_MAX_TRANSIZE, MMU_PAGESIZE);
	cpr_nmap_buf = kmem_getpages(cpr_nmap_buf_size/MMU_PAGESIZE, KM_SLEEP);
}


/*
 * This code is stolen from create_va_to_tte() in startup.c.
 */
void
i_cpr_save_va_to_tte()
{
	extern char obp_tte_str[];
	extern int khmehash_num, uhmehash_num;
	extern struct hmehash_bucket *khme_hash, *uhme_hash;

#define	OFFSET(type, field)	((int)(&((type *)0)->field))

	cpr_obp_tte_str = kmem_alloc(MMU_PAGESIZE, KM_SLEEP);

	/*
	 * Will use this to teach obp how to parse our sw ttes
	 * in resume.  The formatted string is written to the state
	 * file and we must include the terminal null in the length.
	 */
	sprintf(cpr_obp_tte_str, obp_tte_str,
		MMU_PAGESHIFT,
		TTE8K,
		sizeof (struct sf_hment),
		OFFSET(struct sf_hment, hme_tte),
		OFFSET(struct hme_blk, hblk_tag),
		OFFSET(struct hme_blk, hblk_nextpa),
		OFFSET(struct hme_blk, hblk_misc),
		OFFSET(struct hme_blk, hblk_hme),
		NHMENTS,
		HBLK_SZMASK,
		HBLK_RANGE_SHIFT,
		OFFSET(struct hmehash_bucket, hmeh_nextpa),
		sizeof (struct hmehash_bucket),
		HTAG_SFMMUPSZ,
		HTAG_REHASHSZ,
		MAX_HASHCNT,
		uhme_hash,
		khme_hash,
		UHMEHASH_SZ,
		KHMEHASH_SZ,
		KHATID,
		sizeof (struct ctx),
		OFFSET(struct ctx, c_sfmmu),
		ctxs,
		ASI_MEM);

	cpr_obp_tte_strlen = strlen(cpr_obp_tte_str) + 1;
}



/*
 * This function takes care of pages which are not in kas or need to be
 * taken care of in a special way. For example msgbuf pages are not in
 * kas and their pages are allocated via prom_retain().
 */
u_int
i_cpr_count_special_kpages(int flag)
{
	int i, j, tot_pages = 0;

	/*
	 * Save information about prom retained msgbuf pages
	 */

	/* msgbuf */
	if (flag == CPR_TAG) {
		cpr_prom_retain[CPR_MSGBUF].svaddr = MSGBUF_BASE;
		cpr_prom_retain[CPR_MSGBUF].spfn =
			va_to_pfn((caddr_t) MSGBUF_BASE);
		cpr_prom_retain[CPR_MSGBUF].cnt =
			btop(roundup(sizeof (struct msgbuf), MMU_PAGESIZE));
	}

	/*
	 * Go through the prom_retain array to tag those pages.
	 */
	for (i = 0; i < CPR_PROM_RETAIN_CNT; i++) {
		for (j = 0; j < cpr_prom_retain[i].cnt; j++) {
			u_int	pfn = cpr_prom_retain[i].spfn + j;

			if (pf_is_memory(pfn)) {
				if (flag == CPR_TAG) {
					if (!cpr_setbit(pfn))
						tot_pages++;
				} else
					tot_pages++;
			}
		}
	}

	return (tot_pages);
}
