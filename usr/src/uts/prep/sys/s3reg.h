/*
 * Copyright 1988-1989, Sun Microsystems, Inc.
 */

#ifndef	_SYS_S3REG_H
#define	_SYS_S3REG_H

#pragma ident	"@(#)s3reg.h	1.14	96/03/15 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * S3 frame buffer hardware definitions.
 */

#define	S3_DEPTH	8

/*
 * These are relative to their register set, which
 * the 3c0-3df set.
 */
#define	S3_DAC_BASE	0x06
#define	S3_CRTC_ADR	0x14
#define	S3_CRTC_DATA	0x15

#define	S3_H_TOTAL	0x00
#define	S3_H_D_END	0x01
#define	S3_V_TOTAL	0x06
#define	S3_OVFL_REG	0x07
#define		S3_OVFL_REG_VT8		0
#define		S3_OVFL_REG_VDE8	1
#define		S3_OVFL_REG_VRS8	2
#define		S3_OVFL_REG_SVB8	3
#define		S3_OVFL_REG_LCM8	4
#define		S3_OVFL_REG_VT9		5
#define		S3_OVFL_REG_VDE9	6
#define		S3_OVFL_REG_VRS9	7
#define	S3_VDE		0x12
#define	S3_CHIP_ID_REV	0x30
#define	S3_BKWD_2	0x33
#define		S3_BK2_DIS_VDE		0x02
#define		S3_BK2_VCLK_INV_DCLK	0x08
#define		S3_BK2_LOCK_DACW	0x10
#define		S3_BK2_BDR_SEL		0x20
#define		S3_BK2_LOCK_PLTW	0x40
#define		S3_BK2_DISA_FLKR	0x80
#define	S3_CONFG_REG1	0x36
#define		S3_CFG1_BUS_MASK	0x03
#define			S3_CFG1_BUS_EISA	0x00
#define			S3_CFG1_BUS_LOCAL	0x01
#define			S3_CFG1_BUS_ISA		0x11
#define		S3_CFG1_BUSWIDTH_MASK	0x04
#define			S3_CFG1_BUSWIDTH_16	0x00
#define			S3_CFG1_BUSWIDTH_8	0x04
#define		S3_CFG1_BIOSENABLE_MASK	0x08
#define		S3_CFG1_MEMCS16_MASK	0x10
#define		S3_CFG1_MEMSIZE_MASK	0xe0
#define			S3_CFG1_MEMSIZE_SHIFT	5
#define			S3_CFG1_MEMSIZE(v)	\
		(8-(((v) & S3_CFG1_MEMSIZE_MASK) >> S3_CFG1_MEMSIZE_SHIFT))
#define			S3_CFG1_MEMSIZE_SCALE	(512L*1024)
#define	S3_CNFG_REG2	0x37
#define		S3_CFG2_SETUP_MASK	0x01
#define			S3_CFG2_SETUP_5		0x00
#define			S3_CFG2_SETUP_4		0x01
#define		S3_CFG2_RESERVED_MASK	0x02
#define		S3_CFG2_EXT_MON_ID_MASK	0x04
#define		S3_CFG2_NOWS_MASK	0x08
#define		S3_CFG2_MEMCS16_MASK	0x10
#define		S3_CFG2_MON_ID_MASK	0xe0
#define	S3_MODE_CTL	0x42
#define		S3_MODE_CTL_DOT_CLOCK_SELECT_MASK	0x0f
#define		S3_MODE_CTL_DOT_CLOCK_SELECT_SHIFT	0
#define		S3_MODE_CTL_INTL_MODE_MASK		0x20
#define		S3_MODE_CTL_INTL_MODE_SHIFT		5
#define	S3_HGC_MODE	0x45
#define		S3_HGC_MODE_HWGC_ENB			0x01
#define	S3_HWGC_ORGXH	0x46
#define	S3_HWGC_ORGXL	0x47
#define		S3_HWGC_ORGX_MASK			0x07ff
#define	S3_HWGC_ORGYH	0x48
#define	S3_HWGC_ORGYL	0x49
#define		S3_HWGC_ORGY_MASK			0x07ff
#define	S3_EX_SCTL_1	0x50
#define		S3_EX_SCTL_1_ENB_BREQ		0x04
#define		S3_EX_SCTL_1_DIS_LOCA_SRDY	0x08
#define		S3_EX_SCTL_1_PXL_LNGTH		0x30
#define			S3_EX_SCTL_1_PXL_LNGTH_8	0x00
#define			S3_EX_SCTL_1_PXL_LNGTH_16	0x10
#define			S3_EX_SCTL_1_PXL_LNGTH_32	0x30
#define		S3_EX_SCTL_1_GE_SCR_W		0xc0
#define			S3_EX_SCTL_1_GE_SCR_W_1024	0x00
#define			S3_EX_SCTL_1_GE_SCR_W_640	0x40
#define			S3_EX_SCTL_1_GE_SCR_W_800	0x80
#define			S3_EX_SCTL_1_GE_SCR_W_1280	0xc0
#define	S3_EX_DAC_CT	0x55
#define		S3_864_EX_DAC_CT_DAC_R_SEL_MASK		0x01
#define		S3_EX_DAC_CT_DAC_R_SEL_MASK		0x03
#define		S3_EX_DAC_CT_DAC_R_SEL_SHIFT		0
#define	S3_EXT_H_OVF	0x5d
#define		S3_EXT_H_OVF_HT8	0
#define		S3_EXT_H_OVF_HDE8	1
#define		S3_EXT_H_OVF_SHB8	2
/* reserved				3 */
#define		S3_EXT_H_OVF_SHS8	4
/* reserved				5 */
#define		S3_EXT_H_OVF_DXP8	6
#define		S3_EXT_H_OVF_BGT8	7
#define	S3_EXT_V_OVF	0x5e
#define		S3_EXT_V_OVF_VT10	0
#define		S3_EXT_V_OVF_VDE10	1
#define		S3_EXT_V_OVF_SVB10	2
/* reserved				3 */
#define		S3_EXT_V_OVF_VRS10	4
/* reserved				5 */
#define		S3_EXT_V_OVF_LCM10	6
/* reserved				7 */

/*
 * Virtual (mmap offsets) addresses
 */
#define	S3_VADDR_FB	0
#define	S3_VADDR_MMIO	0x10000000

#define	S3_MMIO_SZ	0x20000


#ifdef	__cplusplus
}
#endif

#endif /* _SYS_S3REG_H */
