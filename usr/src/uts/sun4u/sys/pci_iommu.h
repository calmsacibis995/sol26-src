/*
 * Copyright (c) 1991-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_PCI_IOMMU_H
#define	_SYS_PCI_IOMMU_H

#pragma ident	"@(#)pci_iommu.h	1.12	95/10/26 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Generic psycho iommu definitions and types:
 */
#define	PSYCHO_IOMMU_TLB_SIZE	16

typedef enum { IOMMU_XLATE, IOMMU_BYPASS, PCI_PEER_TO_PEER } psycho_dma_t;

/*
 * The following macros define the Constant DVMA and DVMA bypass
 * addesses.
 *
 * The base dvma address is configurable by patching the variable
 * pci_iommu_tsb_size in sun4u/os/startup.c.
 */
#define	IOMMU_DVMA_END			0xffffffff
#define	IOMMU_BYPASS_BASE		0xFFF3000000000000ull
#define	IOMMU_BYPASS_END		0xFFFFFFFFFFFFFFFFull

/*
 * The following macros define the iommu page size and
 * related parameters.
 */
#define	PSYCHO_IOMMU_CTRL_TBW_SIZE	0	/* 8k pages */
#define	IOMMU_PAGE_SHIFT		13
#define	IOMMU_PAGE_SIZE			(1 << IOMMU_PAGE_SHIFT)
#define	IOMMU_PAGE_MASK			~(IOMMU_PAGE_SIZE - 1)
#define	IOMMU_PAGE_OFFSET		(IOMMU_PAGE_SIZE - 1)

/*
 * The following macros are converting iommu pages to bytes
 * and back.
 */
#define	IOMMU_PTOB(x)	((x) << IOMMU_PAGE_SHIFT)
#define	IOMMU_BTOP(x)	(((u_long) (x)) >> IOMMU_PAGE_SHIFT)
#define	IOMMU_BTOPR(x)	((((u_long) (x) + IOMMU_PAGE_OFFSET) >> \
					IOMMU_PAGE_SHIFT))

/*
 * The following macro for calculates the iommu tsb index of
 * a given dvma address.  The psycho soft state pointer must
 * also be provided.
 */
#define	IOMMU_TSB_INDEX(p, a)	\
		IOMMU_BTOP((a) - (p)->iommu_dvma_base)

/*
 * The following macros are for loading and unloading iotte
 * entries.
 */
#define	IOMMU_TTE_SIZE		8
#define	IOMMU_TTE_V		0x8000000000000000ull
#define	IOMMU_TTE_S		0x1000000000000000ull
#define	IOMMU_TTE_C		0x0000000000000010ull
#define	IOMMU_TTE_W		0x0000000000000002ull
#define	IOMMU_INVALID_TTE	0x0000000000000000ull

extern int pf_is_memory(u_int pf);

#define	IOMMU_MAKE_TTE(mp, pfn)						\
	IOMMU_TTE_V | ((u_longlong_t)(pfn) << IOMMU_PAGE_SHIFT) |	\
	(pf_is_memory(pfn) ? IOMMU_TTE_C : 0) |				\
	(((mp)->dmai_rflags & DDI_DMA_READ) ? IOMMU_TTE_W : 0) |	\
	(((mp)->dmai_rflags & DDI_DMA_CONSISTENT) ? 0 : IOMMU_TTE_S)

#define	IOMMU_LOAD_TTE(tsb_vaddr, index, tte) {				\
	volatile u_longlong_t *iotte_ptr;				\
	iotte_ptr = (u_longlong_t *)(tsb_vaddr) + (int)(index);		\
	*iotte_ptr = (u_longlong_t)(tte);				\
}

#define	IOMMU_UNLOAD_TTE(tsb_vaddr, index) {				\
	volatile u_longlong_t *iotte_ptr;				\
	iotte_ptr = (u_longlong_t *)(tsb_vaddr) + (int)(index);		\
	*iotte_ptr = IOMMU_INVALID_TTE;					\
}

#define	IOMMU_TLB_FLUSH(psycho_p, dvma_addr)				\
	*(psycho_p)->iommu_flush_reg = ((dvma_addr) & IOMMU_PAGE_MASK)


/*
 * The following macros constructs a bypass dvma address from
 * a physical page frame number and offset.
 */
#define	IOMMU_BYPASS_ADDR(pfn, offset)			\
	(IOMMU_BYPASS_BASE + ((u_longlong_t)(pfn) << MMU_PAGESHIFT) + 	\
	(offset))

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCI_IOMMU_H */
