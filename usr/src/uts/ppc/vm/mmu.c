/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
#pragma ident	"@(#)mmu.c	1.12	96/06/07 SMI"

/*
 * VM - PowerPC MMU low-level routines.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/vm.h>
#include <sys/mman.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/debug.h>

#include <vm/page.h>
#include <vm/seg.h>
#include <vm/as.h>

#include <sys/cmn_err.h>
#include <sys/cpu.h>
#include <sys/pte.h>
#include <sys/mmu.h>
#include <sys/stack.h>
#include <vm/hat_ppcmmu.h>
#include <vm/seg_kmem.h>

/* Functions here are only for compatibility */

static u_int mmu_ptov_prot(register struct pte *ppte);

void
mmu_setpte(caddr_t base, struct pte pte)
{
	ASSERT(base >= (caddr_t)KERNELBASE);
	ppcmmu_devload(kas.a_hat, &kas, base, NULL, pte.pte_ppn,
		mmu_ptov_prot(&pte) | HAT_NOSYNC, HAT_LOAD);
}


void
mmu_getpte(caddr_t base, struct pte *ppte)
{
	hwpte_t *hwpte;
	struct proc *p = curproc;

	hat_enter(p->p_as->a_hat);
	hwpte = ppcmmu_ptefind(p->p_as, base, PTEGP_NOLOCK);
	if (hwpte == NULL) {
#if defined(_NO_LONGLONG)
		*(u_long *)ppte = MMU_INVALID_PTE;
		*((u_long *)ppte+1) = MMU_INVALID_PTE;
#else
		*(u_longlong_t *)ppte = MMU_INVALID_PTE;
#endif
	} else
		hwpte_to_pte(hwpte, ppte);
	hat_exit(p->p_as->a_hat);
}

void
mmu_getkpte(caddr_t base, struct pte *ppte)
{
	hwpte_t *hwpte;

	ASSERT(base >= (caddr_t)KERNELBASE);
	hat_enter(kas.a_hat);
	hwpte = ppcmmu_ptefind(&kas, base, PTEGP_NOLOCK);
	if (hwpte == NULL) {
#if defined(_NO_LONGLONG)
		*(u_long *)ppte = MMU_INVALID_PTE;
		*((u_long *)ppte+1) = MMU_INVALID_PTE;
#else
		*(u_longlong_t *)ppte = MMU_INVALID_PTE;
#endif
	} else
		hwpte_to_pte(hwpte, ppte);
	hat_exit(kas.a_hat);
}

static u_int
mmu_ptov_prot(register struct pte *ppte)
{
	register u_int pprot;

	if (ppte->pte_valid == 0) {
		pprot = 0;
	} else {

		switch (ppte->pte_pp) {
		case MMU_STD_SRXURX:
			pprot = PROT_READ | PROT_EXEC | PROT_USER;
			break;
		case MMU_STD_SRWXURX:
			pprot = PROT_READ | PROT_WRITE | PROT_EXEC | PROT_USER;
			break;
		case MMU_STD_SRWXURWX:
			pprot = PROT_READ | PROT_WRITE | PROT_EXEC | PROT_USER;
			break;
		case MMU_STD_SRWX:
			pprot = PROT_READ | PROT_WRITE | PROT_EXEC;
			break;
		default:
			pprot = 0;
			break;
		}
	}
	return (pprot);
}
