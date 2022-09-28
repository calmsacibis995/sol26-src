/*
 * Copyright (c) 1990-1992, 1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)ppage.c	1.12	96/06/03 SMI"

#include <sys/t_lock.h>
#include <sys/map.h>
#include <sys/mman.h>
#include <sys/vm.h>
#include <sys/mmu.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/systm.h>
#include <vm/as.h>
#include <vm/hat_i86.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

/*
 * External Routines:
 */
extern page_t *page_numtopp_alloc(u_int);
extern caddr_t i86devmap(int, int, u_int);
extern pte_t mmu_invalid_pte;

/*
 * Architecures with a vac use this routine to quickly make
 * a vac-aligned mapping.  We don't have a vac, so we don't
 * care about that - just make this simple.
 */
/* ARGSUSED2 */
caddr_t
ppmapin(pp, vprot, avoid)
	register struct page *pp;
	register u_int vprot;
	register caddr_t avoid;
{
	u_long a;
	caddr_t va;

	a = rmalloc_wait(kernelmap, (long)CLSIZE);
	va = kmxtob(a);

	hat_memload(kas.a_hat, va, pp, vprot|HAT_NOSYNC, HAT_LOAD_LOCK);

	return (va);
}

void
ppmapout(va)
	register caddr_t va;
{
	hat_unload(kas.a_hat, va, PAGESIZE, HAT_UNLOAD_UNLOCK);
	rmfree(kernelmap, (size_t)CLSIZE, (ulong_t)btokmx(va));
}


/*
 * Copy the data from the physical page represented by "frompp" to
 * that represented by "topp". ppcopy uses CPU->cpu_caddr1 and
 * CPU->cpu_caddr2.  It assumes that no one uses either map at interrupt
 * level and no one sleeps with an active mapping there.
 */
void
ppcopy(frompp, topp)
	page_t *frompp;
	page_t *topp;
{
	caddr_t caddr1;
	caddr_t caddr2;
	kmutex_t *ppaddr_mutex;
	struct	pte	pte;
	u_int	*caddr1_pte, *caddr2_pte;
	extern void 	hat_mempte();

	ASSERT(se_assert(&frompp->p_selock));
	ASSERT(se_assert(&topp->p_selock));

	kpreempt_disable(); /* can't preempt if holding caddr1, caddr2 */

	caddr1 = CPU->cpu_caddr1;
	caddr2 = CPU->cpu_caddr2;
	caddr1_pte = (u_int *)CPU->cpu_caddr1pte;
	caddr2_pte = (u_int *)CPU->cpu_caddr2pte;

	ppaddr_mutex = &CPU->cpu_ppaddr_mutex;

	mutex_enter(ppaddr_mutex);

	hat_mempte(frompp, PROT_READ|PROT_WRITE, &pte, caddr1);
	*caddr1_pte = *(u_int *)&pte;
	mmu_tlbflush_entry(caddr1);
	hat_mempte(topp, PROT_READ|PROT_WRITE, &pte, caddr2);
	*caddr2_pte = *(u_int *)&pte;
	mmu_tlbflush_entry(caddr2);
	bcopy(caddr1, caddr2, PAGESIZE);


	mutex_exit(ppaddr_mutex);

	kpreempt_enable();
}

/*
 * Zero the physical page from off to off + len given by `pp'
 * without changing the reference and modified bits of page.
 * pagezero uses CPU->cpu_caddr2 and assumes that no one uses this
 * map at interrupt level and no one sleeps with an active mapping there.
 *
 * pagezero() must not be called at interrupt level.
 */
void
pagezero(pp, off, len)
	page_t *pp;
	u_int off, len;
{
	caddr_t caddr2;
	u_int 	*caddr2_pte;
	kmutex_t *ppaddr_mutex;
	struct	pte pte;
	extern void 	hat_mempte();

	ASSERT((int)len > 0 && (int)off >= 0 && off + len <= PAGESIZE);
	ASSERT(se_assert(&pp->p_selock));

	kpreempt_disable(); /* can't preempt if holding caddr2 */

	caddr2 = CPU->cpu_caddr2;
	caddr2_pte = (u_int *)CPU->cpu_caddr2pte;

	ppaddr_mutex = &CPU->cpu_ppaddr_mutex;
	mutex_enter(ppaddr_mutex);

	hat_mempte(pp, PROT_READ|PROT_WRITE, &pte, caddr2);
	*caddr2_pte = *(u_int *)&pte;
	mmu_tlbflush_entry(caddr2);

	bzero(caddr2 + off, len);

	mutex_exit(ppaddr_mutex);
	kpreempt_enable();
}

/*
 * Map the page pointed to by pp into the kernel virtual address space.
 * This routine is used by the rootnexus.
 */
void
i86_pp_map(page_t *pp, caddr_t kaddr)
{
	segkmem_mapin(&kvseg, kaddr, MMU_PAGESIZE,
	    PROT_READ|PROT_WRITE|HAT_NOSYNC,
	    page_pptonum(pp), HAT_LOAD);
}

/*
 * Map the page containing the virtual address into the kernel virtual address
 * space.  This routine is used by the rootnexus.
 */
void
i86_va_map(caddr_t vaddr, struct as *asp, caddr_t kaddr)
{
	u_int pfnum;

	pfnum = hat_getpfnum(asp->a_hat, vaddr);
	segkmem_mapin(&kvseg, kaddr, MMU_PAGESIZE,
	    PROT_READ|PROT_WRITE|HAT_NOSYNC, pfnum, HAT_LOAD);
}
