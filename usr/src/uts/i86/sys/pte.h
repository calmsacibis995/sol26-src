/*
 * Copyright (c) 1991, 1992 by Sun Microsystems, Inc.
 */

#ifndef _SYS_PTE_H
#define	_SYS_PTE_H

#pragma ident	"@(#)pte.h	1.11	95/09/09 SMI"

/*
 * Copyright (c) 1991, 1992, Sun Microsystems, Inc. All Rights Reserved.
 * Sun considers its source code as an unpublished, proprietary
 * trade secret, and it is available only under strict license
 * provisions. This copyright notice is placed here only to protect
 * Sun in the event the source is deemed a published work. Disassembly,
 * decompilation, or other means of reducing the object code to human
 * readable form is prohibited by the license agreement under which
 * this code is provided to the user or company in possession of this
 * copy
 *
 * RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the
 * Government is subject to restrictions as set forth in subparagraph
 * (c) (1) (ii) of the Rights in Technical Data and Computeer Software
 * clause at DFARS 52.227-7013 and in similar clauses in the FAR and
 * NASA FAR Supplement
 */

#ifndef _ASM
#include <sys/types.h>
#endif /* _ASM */

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _ASM

/*
 * generate a pte for the specified page frame, with the
 * specified permissions, possibly cacheable.
 */

#define	MRPTEOF(ppn, os, m, r, c, w, a, p)	\
	(((ppn)<<12)|((os)<<9)|((m)<<6)|((r)<<5)|	\
	((!(c))<<4)|((w)<<3)|((a)<<1)|p)

#define	PTEOF(p, a, c)	MRPTEOF(p, 0, 0, 0, c, 0, a, 1)

typedef struct pte {
	u_int Present:1;
	u_int AccessPermissions:2;
	u_int WriteThru:1;
	u_int NonCacheable:1;
	u_int Referenced:1;
	u_int Modified:1;
	u_int MustBeZero:2;
	u_int OSReserved:3;
	u_int PhysicalPageNumber:20;
} pte_t;

union ptes {
	struct pte pte;
	u_int pte_int;
};
#endif /* !_ASM */

#define	PTE_RM_MASK		0x60
#define	PTE_REF_MASK		0x20
#define	PTE_MOD_MASK		0x40
#define	PTE_RM_SHIFT		5
#define	PTE_PERMS(b)		(((b) & 0x3) << 1)
#define	PTE_PERMMASK		((0x3 << 1))
#define	PTE_PERMSHIFT		(1)
#define	PTE_WRITETHRU(w)	(((w) & 0x1) << 3)
#define	PTE_NONCACHEABLE(d)	(((d) & 0x1) << 4)
#define	PTE_REF(r)		(((r) & 0x1) << 5)
#define	PTE_MOD(m)		(((m) & 0x1) << 6)
#define	PTE_OSRESERVED(o)	(((o) & 0x7) << 9)

#define	PTE_PFN(p)		(((u_int)(p)) >> 12)

#define	pte_valid(pte) ((pte)->Present)

#define	pte_konly(pte)		(((*(u_int *)(pte)) & 0x4) == 0)
#define	pte_ronly(pte)		(((*(u_int *)(pte)) & 0x2) == 0)
#define	pte_cacheable(pte)	(((*(u_int *)(pte)) & 0x10) == 0)
#define	pte_writethru(pte)	(((*(u_int *)(pte)) & 0x8) == 0)
#define	pte_accessed(pte)	((*(u_int *)(pte)) & 0x20)
#define	pte_dirty(pte)		((*(u_int *)(pte)) & 0x40)
#define	pte_accdir(pte)		((*(u_int *)(pte)) & 0x60)

#define	MAKE_PFNUM(a)	\
	(((struct pte *)(a))->PhysicalPageNumber)

#define	PG_V			1

/*
 * Definitions for Access Permissions
 * Names taken from SPARC Reference MMU code, but considering
 *   execute and read permissions equivalent.
 */

#define	MMU_STD_SRX		0
#define	MMU_STD_SRWX		1
#define	MMU_STD_SRXURX		2
#define	MMU_STD_SRWXURWX	3

#define	MAKE_PROT(v)		PTE_PERMS(v)
#define	PG_PROT			MAKE_PROT(0x3)
#define	PG_KW			MAKE_PROT(MMU_STD_SRWX)
#define	PG_KR			MAKE_PROT(MMU_STD_SRX)
#define	PG_UW			MAKE_PROT(MMU_STD_SRWXURWX)
#define	PG_URKR			MAKE_PROT(MMU_STD_SRXURX)
#define	PG_UR			MAKE_PROT(MMU_STD_SRXURX)
#define	PG_UPAGE		PG_KW	/* Intel u pages not user readable */

#define	MMU_STD_PAGEMASK	0xFFFFF000
#define	MMU_STD_PAGESHIFT	12
#define	MMU_STD_PAGESIZE	(1 << MMU_STD_PAGESHIFT)

#define	MMU_STD_SEGSHIFT	22

#define	MMU_L1_INDEX(a) (((u_int)(a)) >> 22)
#define	MMU_L2_INDEX(a) (((a) >> 12) & 0x3ff)

#define	MMU_L1_VA(a)	((a) << 22)
#define	MMU_L2_VA(a)	((a) << 12)

#define	four_mb_page(pte)		(*((u_int *)(pte)) & 0x80)
#define	IN_SAME_4MB_PAGE(a, b)	(MMU_L1_INDEX(a) == MMU_L1_INDEX(b))
#define	FOURMB_PDE(a, g,  b, c) \
	(((a) << 12) | ((g) << 8) | 0x80 |(((b) & 0x03) << 1) | (c))

/*
 *	The following can be provided if needed.  Need "vtop(va)" or equiv.
 *	#define	VA_TO_PA(va)
 *	#define VA_TO_PFN(va)
 */

#ifndef _ASM
extern struct pte mmu_pteinvalid;
#endif /* _ASM */
#define	MMU_STD_INVALIDPTE	(0)

#define	MMU_NPTE_ONE	1024		/* 1024 PTEs in first level table */
#define	MMU_NPTE_TWO	1024		/* 1024 PTEs in second level table */

#if !defined(_ASM) && defined(_KERNEL)
extern	pte_t *Sysmap1;
extern	pte_t *Sysmap2;
extern	pte_t *KERNELmap;

#define	pte_memory(pte) (pf_is_memory((pte)->PhysicalPageNumber))

/* these should be going away soon */
extern	char CADDR1[];
extern	pte_t mmap[];
#endif /* !defined(_ASM) && defined(_KERNEL) */

#ifdef	__cplusplus
}
#endif

#endif /* !_SYS_PTE_H */
