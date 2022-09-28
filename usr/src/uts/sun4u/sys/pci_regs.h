/*
 * Copyright (c) 1991-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_PCI_REGS_H
#define	_SYS_PCI_REGS_H

#pragma ident	"@(#)pci_regs.h	1.20	96/04/01 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * offsets for PBM registers:
 */
#define	PCI_CNTRL_REG_OFFSET		0x0000
#define	PCI_AFSR_OFFSET			0x0010
#define	PCI_AFAR_OFFSET			0x0018
#define	PCI_DIAG_OFFSET			0x0020
#define	SBUF_CTRL_REG_OFFSET		0x0800
#define	SBUF_INVL_REG_OFFSET		0x0808
#define	SBUF_SYNC_REG_OFFSET		0x0810
#define	SBUF_A_TAG_DIAG_ACC_OFFSET	0xB800
#define	SBUF_A_DATA_DIAG_ACC_OFFSET	0xB900
#define	SBUF_B_TAG_DIAG_ACC_OFFSET	0xC800
#define	SBUF_B_DATA_DIAG_ACC_OFFSET	0xC900

/*
 * offsets for Psycho/IOMMMU registers:
 */
#define	PSYCHO_CNTRL_REG_OFFSET		0x0010
#define	ECC_CNTRL_REG_OFFSET		0x0020
#define	UE_AFSR_OFFSET			0x0030
#define	UE_AFAR_OFFSET			0x0038
#define	CE_AFSR_OFFSET			0x0040
#define	CE_AFAR_OFFSET			0x0048
#define	PERF_MON_CNTRL_REG_OFFSET	0x0100
#define	PERF_COUNTER_REG_OFFSET		0x0108
#define	IOMMU_CTRL_REG_OFFSET		0x0200
#define	TSB_BASE_REG_OFFSET		0x0208
#define	IOMMU_FLUSH_REG_OFFSET		0x0210
#define	IMAP_REG_OFFSET			0x0C00
#define	OBIO_IMAP_REG_OFFSET		0x1000
#define	OBIO_GRAPHICS_IMAP_OFFSET	0x1098
#define	OBIO_EXPANSION_IMAP_OFFSET	0x10A0
#define	CLEARI_REG_OFFSET		0x1400
#define	OBIO_CLEARI_REG_OFFSET		0x1800
#define	INTR_RETRY_TIMER_OFFSET		0x1A00
#define	TLB_TAG_DIAG_ACC_OFFSET		0xA580
#define	TLB_DATA_DIAG_ACC_OFFSET	0xA600
#define	PCI_INT_STATE_DIAG_REG		0xA800
#define	OBIO_INT_STATE_DIAG_REG		0xA808

/*
 * offsets of PCI address spaces from base address:
 */
#define	PCI_CONFIG			0x001000000ull
#define	PCI_A_IO			0x002000000ull
#define	PCI_B_IO			0x002010000ull
#define	PCI_A_MEMORY			0x100000000ull
#define	PCI_B_MEMORY			0x180000000ull
#define	PCI_IO_SIZE			0x000010000ull
#define	PCI_MEM_SIZE			0x080000000ull

/*
 * PBM control register bit definitions:
 */
#define	PCI_CNTRL_SBH_ERR		0x0000000800000000ull
#define	PCI_CNTRL_SERR			0x0000000400000000ull
#define	PCI_CNTRL_SPEED			0x0000000200000000ull
#define	PCI_CNTRL_ARB_PARK		0x0000000000200000ull
#define	PCI_CNTRL_CPU_PRIO		0x0000000000100000ull
#define	PCI_CNTRL_ARB_PRIO_MASK		0x00000000000f0000ull
#define	PCI_CNTRL_SBH_INT_EN		0x0000000000000400ull
#define	PCI_CNTRL_WAKEUP_EN		0x0000000000000200ull
#define	PCI_CNTRL_ERR_INT_EN		0x0000000000000100ull
#define	PCI_CNTRL_ARB_EN_MASK		0x000000000000000full

/*
 * streaming cache control register bit definitions:
 */
#define	SBUF_CNTRL_SBUF_ENABLE		0x0000000000000001ull
#define	SBUF_CNTRL_DIAG_ENABLE		0x0000000000000002ull
#define	SBUF_CNTRL_RR_DISABLE		0x0000000000000004ull
#define	SBUF_CNTRL_LRU_LE		0x0000000000000008ull

/*
 * PBM asynchronous fault status register bit definitions:
 */
#define	PCI_AFSR_PE_SHIFT		60
#define	PCI_AFSR_SE_SHIFT		56
#define	PCI_AFSR_E_MA			0x8
#define	PCI_AFSR_E_TA			0x4
#define	PCI_AFSR_E_RTRY			0x2
#define	PCI_AFSR_E_PERR			0x1
#define	PCI_AFSR_E_MASK			0xf
#define	PCI_AFSR_BYTEMASK		0x0000ffff00000000ull
#define	PCI_AFSR_BYTEMASK_SHIFT		32
#define	PCI_AFSR_BLK			0x0000000080000000ull
#define	PCI_AFSR_MID			0x000000003e000000ull
#define	PCI_AFSR_MID_SHIFT		25

/*
 * PBM asynchronous fault status register bit definitions:
 */
#define	PCI_AFAR_MASK			0x000000ffffffffffull

/*
 * PBM diagnostic register bit definitions:
 */
#define	PCI_DIAG_DIS_RETRY		0x0000000000000040ull
#define	PCI_DIAG_DIS_INTSYNC		0x0000000000000020ull
#define	PCI_DIAG_DIS_DWSYNC		0x0000000000000010ull

/*
 * PSYCHO control register bit definitions:
 */
#define	PSYCHO_CNTRL_IMPL		0xf000000000000000ull
#define	PSYCHO_CNTRL_IMPL_SHIFT		60
#define	PSYCHO_CNTRL_VER		0x0f00000000000000ull
#define	PSYCHO_CNTRL_VER_SHIFT		56
#define	PSYCHO_CNTRL_IGN		0x0007c00000000000ull
#define	PSYCHO_CNTRL_IGN_SHIFT		46
#define	PSYCHO_CNTRL_APCKEN		0x0000000000000008ull
#define	PSYCHO_CNTRL_APERR		0x0000000000000004ull
#define	PSYCHO_CNTRL_IAP		0x0000000000000002ull
#define	PSYCHO_CNTRL_MODE		0x0000000000000001ull

/*
 * PSYCHO ECC control register bit definitions:
 */
#define	PSYCHO_ECCCR_ECC_EN		0x8000000000000000ull
#define	PSYCHO_ECCCR_UE_INTEN		0x4000000000000000ull
#define	PSYCHO_ECCCR_CE_INTEN		0x2000000000000000ull

/*
 * PSYCHO interrupt mapping register bit definitions (bits and shifts):
 */
#define	PSYCHO_IMR_VALID		0x80000000ull
#define	PSYCHO_IMR_TID			0x7C000000ull
#define	PSYCHO_IMR_IGN			0x000007C0ull
#define	PSYCHO_IMR_TID_SHIFT		26
#define	PSYCHO_IMR_IGN_SHIFT		6

/*
 * PSYCHO clear interrupt register bit definitions:
 */
#define	PSYCHO_CIR_MASK			0x3ull
#define	PSYCHO_CIR_IDLE			0x0ull
#define	PSYCHO_CIR_RECEIVED		0x1ull
#define	PSYCHO_CIR_RSVD			0x2ull
#define	PSYCHO_CIR_PENDING		0x3ull

/*
 * PSYCHO UE AFSR bit definitions:
 */
#define	PSYCHO_UE_AFSR_PE_SHIFT		61
#define	PSYCHO_UE_AFSR_SE_SHIFT		58
#define	PSYCHO_UE_AFSR_E_MASK		0x7
#define	PSYCHO_UE_AFSR_E_PIO		0x4
#define	PSYCHO_UE_AFSR_E_DRD		0x2
#define	PSYCHO_UE_AFSR_E_DWR		0x1
#define	PSYCHO_UE_AFSR_BYTEMASK		0x0000ffff00000000ull
#define	PSYCHO_UE_AFSR_BYTEMASK_SHIFT	32
#define	PSYCHO_UE_AFSR_DW_OFFSET	0x00000000e0000000ull
#define	PSYCHO_UE_AFSR_DW_OFFSET_SHIFT	29
#define	PSYCHO_UE_AFSR_MID		0x000000001f000000ull
#define	PSYCHO_UE_AFSR_MID_SHIFT	24
#define	PSYCHO_UE_AFSR_BLK		0x0000000000800000ull

/*
 * PSYCHO CE AFSR bit definitions:
 */
#define	PSYCHO_CE_AFSR_PE_SHIFT		61
#define	PSYCHO_CE_AFSR_SE_SHIFT		58
#define	PSYCHO_CE_AFSR_E_MASK		0x7
#define	PSYCHO_CE_AFSR_E_PIO		0x4
#define	PSYCHO_CE_AFSR_E_DRD		0x2
#define	PSYCHO_CE_AFSR_E_DWR		0x1
#define	PSYCHO_CE_AFSR_SYND		0x00ff000000000000ull
#define	PSYCHO_CE_AFSR_SYND_SHIFT	48
#define	PSYCHO_CE_AFSR_BYTEMASK		0x0000ffff00000000ull
#define	PSYCHO_CE_AFSR_BYTEMASK_SHIFT	32
#define	PSYCHO_CE_AFSR_DW_OFFSET	0x00000000e0000000ull
#define	PSYCHO_CE_AFSR_DW_OFFSET_SHIFT	29
#define	PSYCHO_CE_AFSR_MID		0x000000001f000000ull
#define	PSYCHO_CE_AFSR_MID_SHIFT	24
#define	PSYCHO_CE_AFSR_BLK		0x0000000000800000ull

/*
 * PSYCHO IOMMU control register bit definitions:
 */
#define	PSYCHO_IOMMU_CTRL_ENABLE	0x0000000000000001ull
#define	PSYCHO_IOMMU_DIAG_ENABLE	0x0000000000000002ull
#define	PSYCHO_IOMMU_CTRL_TSB_SZ_SHIFT	16
#define	PSYCHO_IOMMU_CTRL_TBW_SZ_SHIFT	2
#define	PSYCHO_IOMMU_CTRL_LCK_ENABLE	0x0000000000800000ull

/*
 * PSYCHO performance monitor control register bit definitions:
 */
#define	PSYCHO_PERF_MON_CNTRL_CLR1	0x0000000000008000ull
#define	PSYCHO_PERF_MON_CNTRL_SEL1	0x0000000000001f00ull
#define	PSYCHO_PERF_MON_CNTRL_CLR0	0x0000000000000080ull
#define	PSYCHO_PERF_MON_CNTRL_SEL0	0x000000000000001full

/*
 * Bits in configuration header command register:
 *
 * Generic PCI configuration command register bits are defined in
 * the file uts/common/sys/pci.h.
 */

/*
 * Bits in configuration header command register:
 *
 * Generic PCI configuration status register bits are defined in
 * the file uts/common/sys/pci.h.
 */
#define	PCI_CONF_ERROR_MASK						\
		(PCI_STAT_PERROR|PCI_STAT_S_SYSERR|PCI_STAT_R_MAST_AB	\
			PCI_STAT_R_TARG_AB|PCI_STAT_S_TARG_AB|		\
			PCI_STAT_S_PERROR)

/*
 * These flags should be moved to uts/common/sys/pci.h:
 */
#ifndef	PCI_BCNF_BCNTRL
#define	PCI_BCNF_BCNTRL			0x3e
#define	PCI_BCNF_BCNTRL_PARITY_ENABLE	0x0001
#define	PCI_BCNF_BCNTRL_SERR_ENABLE	0x0002
#define	PCI_BCNF_BCNTRL_MAST_AB_MODE	0x0020
#endif
#ifndef	PCI_BCNF_LATENCY_TIMER
#define	PCI_BCNF_LATENCY_TIMER		0x1b
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCI_REGS_H */
