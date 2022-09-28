/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_MACH_PPCMMU_H
#define	_SYS_MACH_PPCMMU_H

#pragma ident	"@(#)mach_ppcmmu.h	1.2	94/12/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(_KERNEL) && !defined(_ASM)

/*
 * Format of BAT Registers for MPC601.
 */
#ifdef _BIT_FIELDS_LTOH
typedef struct batu_601 {
	/* BAT Upper */
	unsigned bat_601_pp:2;
	unsigned bat_601_ku:1;
	unsigned bat_601_ks:1;
	unsigned bat_601_wim:3;
	unsigned bat_601_reserved_u:10;
	unsigned bat_601_blpi:15;
} batu_601_t;

typedef struct batl_601 {
	/* BAT Lower */
	unsigned bat_601_bsm:6;
	unsigned bat_601_v:1;
	unsigned bat_601_reserved_l:10;
	unsigned bat_601_pbn:15;
} batl_601_t;
#endif

#ifdef  _BIT_FIELDS_HTOL
typedef struct batu_601 {
	/* BAT Upper */
	unsigned bat_601_blpi:15;
	unsigned bat_601_reserved_u:10;
	unsigned bat_601_wim:3;
	unsigned bat_601_ks:1;
	unsigned bat_601_ku:1;
	unsigned bat_601_pp:2;
} batu_601_t;

typedef struct batl_601 {
	/* BAT Lower */
	unsigned bat_601_pbn:15;
	unsigned bat_601_reserved_l:10;
	unsigned bat_601_v:1;
	unsigned bat_601_bsm:6;
} batl_601_t;
#endif

/*
 * Format of BAT Registers for PowerPC (non 601).
 */
#ifdef _BIT_FIELDS_LTOH
typedef struct batu {
	/* BAT Upper */
	unsigned bat_vs:1;
	unsigned bat_vp:1;
	unsigned bat_bl:11;
	unsigned bat_reserved_u:4;
	unsigned bat_bepi:15;
} batu_t;

typedef struct batl {
	/* BAT Lower */
	unsigned bat_pp:2;
	unsigned bat_reserved_l1:1;
	unsigned bat_wimg:4;
	unsigned bat_reserved_l2:10;
	unsigned bat_brpn:15;
} batl_t;
#endif

#ifdef  _BIT_FIELDS_HTOL
typedef struct batu {
	/* BAT Upper */
	unsigned bat_bepi:15;
	unsigned bat_reserved_u:4;
	unsigned bat_bl:11;
	unsigned bat_vs:1;
	unsigned bat_vp:1;
} batu_t;

typedef struct batl {
	/* BAT Lower */
	unsigned bat_brpn:15;
	unsigned bat_reserved_l2:10;
	unsigned bat_wimg:4;
	unsigned bat_reserved_l1:1;
	unsigned bat_pp:2;
} batl_t;
#endif

#endif /* defined(_KERNEL) && !defined(_ASM) */

/* bit masks in BAT registers (MPC601) */
#define	BAT_601_PP		0x00000003
#define	BAT_601_KU		0x00000004
#define	BAT_601_KS		0x00000008
#define	BAT_601_WIM		0x00000070
#define	BAT_601_BLPI		0xFFFE0000
#define	BAT_601_PBN		0xFFFE0000
#define	BAT_601_V		0x00000040
#define	BAT_601_BSM		0x0000003F
#define	BAT_601_PBN_SHIFT	17
#define	BAT_601_BLPI_SHIFT	17
#define	BAT_601_BLOCK_SHIFT	17

/* bit masks in IBAT/DBAT registers (non 601) */
#define	BAT_PP		0x00000003
#define	BAT_WIMG	0x00000078
#define	BAT_BRPN	0xFFFE0000
#define	BAT_VS		0x00000001
#define	BAT_VP		0x00000002
#define	BAT_BL		0x00001FFC
#define	BAT_BEPI	0xFFFE0000
#define	BAT_BL_SHIFT	15

#ifdef	__cplusplus
}
#endif

#endif /* !_SYS_MACH_PPCMMU_H */
