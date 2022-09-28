/*
 * Copyright (c) 1990, 1992 by Sun Microsystems, Inc.
 */

#ifndef	_SYS_MMU_H
#define	_SYS_MMU_H

#pragma ident	"@(#)mmu.h	1.13	96/04/17 SMI"

#include <sys/pte.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _ASM
/* Can map all addresses */
#define	good_addr(a)  (1)

extern u_int shm_alignment;	/* VAC address consistency modulus */
#endif /* !_ASM */

/*
 * Definitions for the Intel 80x86 MMU
 */

/*
 * Page fault error code, pushed onto stack on page fault exception
 */
#define	MMU_PFEC_P		0x1	/* Page present */
#define	MMU_PFEC_WRITE		0x2	/* Write access */
#define	MMU_PFEC_USER		0x4	/* User mode access */

/* Access types based on above error codes */
#define	MMU_PFEC_AT_MASK	(MMU_PFEC_USER|MMU_PFEC_WRITE)
#define	MMU_PFEC_AT_UREAD	MMU_PFEC_USER
#define	MMU_PFEC_AT_UWRITE	(MMU_PFEC_USER|MMU_PFEC_WRITE)
#define	MMU_PFEC_AT_SREAD	0
#define	MMU_PFEC_AT_SWRITE	MMU_PFEC_WRITE

#if defined(_KERNEL) && !defined(_ASM)
/*
 * Low level mmu-specific functions for non virtual address cache machines
 */
#define	vac_init()
#define	vac_usrflush()
#define	vac_ctxflush()
#define	vac_segflush(base)
#define	vac_pageflush(base)
#define	vac_flush(base, len)

int	valid_va_range(/* basep, lenp, minlen, dir */);

#endif /* defined(_KERNEL) && !defined(_ASM) */

/*
 * Page directory and physical pte page parameters
 */
#ifndef MMU_PAGESIZE
#define	MMU_PAGESIZE	4096
#endif

#define	NPTEPERPT	1024	/* entries in page table */
#define	NPTESHIFT	10
#define	PTSIZE		(NPTEPERPT * MMU_PAGESIZE)	/* bytes mapped */
#define	PTOFFSET	(PTSIZE - 1)

#define	FOURMB_PAGESIZE		0x400000
#define	FOURMB_PAGEOFFSET	(FOURMB_PAGESIZE - 1)
#define	FOURMB_PAGESHIFT	22

#ifndef _ASM

/* Low-level functions */
extern void mmu_tlbflush_entry(caddr_t);
extern void mmu_getpte(caddr_t, pte_t *);
extern void mmu_getkpte(caddr_t, pte_t *);
void mmu_setpte(caddr_t base, struct pte pte);

extern u_int cr3(void);
extern void  setcr3(u_int);
extern void  invlpg(caddr_t);
extern u_int kernel_only_cr3;

#define	mmu_tlbflush_all()		setcr3(cr3())

#define	mmu_pdeptr_cpu(cpu, addr) \
	&((cpu)->cpu_pagedir[(u_int)(addr) >> 22])
#define	mmu_pdeload_cpu(cpu, addr, value) \
	(*(u_int *)(mmu_pdeptr_cpu(cpu, addr)) = (u_int)(value))

#define	mmu_pdeptr(addr)		mmu_pdeptr_cpu(CPU, addr)
#define	mmu_pdeload(addr, value)	mmu_pdeload_cpu(CPU, addr, value)
#define	mmu_loadcr3(cpu) (((cpu)->cpu_current_hat == NULL) ? \
				setcr3(kernel_only_cr3) : setcr3(cr3()))
#endif /* !_ASM */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_MMU_H */
