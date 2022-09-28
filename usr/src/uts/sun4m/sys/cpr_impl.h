/*
 * Copyright (c) 1992 - 1996, by Sun Microsystems Inc.
 * All rights reserved.
 */

#ifndef	_SYS_CPR_IMPL_H
#define	_SYS_CPR_IMPL_H

#pragma ident	"@(#)cpr_impl.h	1.14	96/09/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file contains machine dependent information for CPR
 */

/*
 * CPR Machine Dependent Information Format:
 *
 *	Machdep Info: Anything needed to support a particular platform,
 *		things like the thread ptr and other registers, also
 *		for the mmu. Examples are the pmeg allocation for 4c or
 *		the PTE table for 4m, the internal structure of this block
 *		is only understood by the PSM since it could be different
 *		for differnt machines, all we require is a header telling
 *		us the size of the structure.
 */

#define	CPR_MACHDEP_MAGIC	'MaDp'
#define	CPRBOOT			"cprboot"

/*
 * Sun4m MMU info that need to be saved in the state file for resume.
 */
struct sun4m_machdep {
	u_int	mmu_ctp;
	u_int	mmu_ctx;
	u_int	mmu_ctl;
};

#define	PATOPTP_SHIFT	4
#define	PN_TO_ADDR(pn)	(((pn) << MMU_STD_PAGESHIFT) & MMU_STD_PAGEMASK)
#define	ADDR_TO_PN(a)	(((a) >> MMU_STD_PAGESHIFT) & MMU_STD_ROOTMASK)
#define	BASE_ADDR	MMU_L3_BASE
#define	CPR_MAXCONTIG	8

typedef	ulong	physaddr_t;

#define	prom_map_plat(addr, pa, size, va) \
		if (prom_map(addr, 0, pa, size) == 0) { \
			errp("PROM_MAP failed vaddr = %x, paddr =%x\n", \
			va, pa); \
			return (-1); \
		}

extern u_int i_cpr_va_to_pfn(caddr_t);
extern int i_cpr_write_machdep(vnode_t *);
extern void i_cpr_machdep_setup(void);
extern void i_cpr_save_machdep_info(void);
extern void i_cpr_enable_intr(void);
extern void i_cpr_enable_clkintr(void);
extern void i_cpr_disable_clkintr(void);
extern void i_cpr_set_tbr(void);
extern void i_cpr_stop_intr(void);
extern void i_cpr_vac_ctxflush(void);
extern void i_cpr_restore_hwregs(u_int, u_int, caddr_t, caddr_t, u_int);
extern void i_cpr_handle_xc(u_int);
extern timestruc_t i_cpr_todget();

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CPR_IMPL_H */
