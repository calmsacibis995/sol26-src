/*
 * Copyright (c) 1990-1992, 1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)ppage.c	1.9	96/06/07 SMI"

#include <sys/t_lock.h>
#include <sys/map.h>
#include <sys/mman.h>
#include <sys/vm.h>
#include <sys/mmu.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/systm.h>
#include <vm/as.h>
#include <vm/hat_ppcmmu.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

/*
 * Architecures with a vac use this routine to quickly make
 * a vac-aligned mapping.  We don't have a vac on PowerPC, so we don't
 * care about that - just make this simple.
 */
caddr_t
ppmapin(pp, vprot, avoid)
	register struct page *pp;
	register u_int vprot;
	register caddr_t avoid;
{
	u_long a;
	caddr_t va;
#ifdef lint
	avoid = avoid;
#endif

	a = rmalloc_wait(kernelmap, (long)CLSIZE);
	va = kmxtob(a);

	hat_memload(kas.a_hat, va, pp, vprot | HAT_NOSYNC, HAT_LOAD_LOCK);

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
 * that represented by "topp".
 */
void
ppcopy(page_t *frompp, page_t *topp)
{
	register caddr_t from_va, to_va;
	void locked_pgcopy(caddr_t, caddr_t);

	ASSERT(se_assert(&frompp->p_selock));
	ASSERT(se_assert(&topp->p_selock));

	from_va = ppmapin(frompp, PROT_READ, (caddr_t)-1);
	to_va   = ppmapin(topp, PROT_READ | PROT_WRITE, from_va);
	locked_pgcopy(from_va, to_va);
	ppmapout(from_va);
	ppmapout(to_va);
}

/*
 * Zero the physical page from off to off + len given by `pp'
 * without changing the reference and modified bits of page.
 */
void
pagezero(pp, off, len)
	page_t *pp;
	u_int off, len;
{
	register caddr_t va;

	ASSERT((int)len > 0 && (int)off >= 0 && off + len <= PAGESIZE);
	ASSERT(se_assert(&pp->p_selock));

	va = ppmapin(pp, PROT_READ | PROT_WRITE, (caddr_t)-1);
	bzero(va + off, len);
	ppmapout(va);
}

/*
 * Map the page containing the virtual address into the kernel virtual address
 * space.  This routine is used by the rootnexus.
 */
void
ppc_va_map(caddr_t vaddr, struct as *asp, caddr_t kaddr)
{
	u_int pfnum;

	pfnum = ppcmmu_getpfnum(asp, vaddr);

	ASSERT(pfnum != (u_int)-1);

	segkmem_mapin(&kvseg, kaddr, MMU_PAGESIZE,
	    PROT_READ | PROT_WRITE | HAT_NOSYNC, pfnum, 0);
}
