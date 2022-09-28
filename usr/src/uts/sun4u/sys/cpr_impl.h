/*
 * Copyright (c) 1994 - 1996, by Sun Microsystems Inc.
 * All rights reserved.
 */

#ifndef	_SYS_CPR_IMPL_H
#define	_SYS_CPR_IMPL_H

#pragma ident	"@(#)cpr_impl.h	1.22	96/09/20 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/machparam.h>
#include <sys/vnode.h>
#include <sys/pte.h>
/*
 * This file contains machine dependent information for CPR
 */

extern int itlb_entries;
extern int dtlb_entries;

struct sun4u_tlb {
	int	no;			/* tlb entry no */
	caddr_t va_tag;
	int	ctx;
	tte_t	tte;
};

/*
 * Information about the pages allocated via prom_retain().
 * Increase the number of CPR_PROM_RETAIN_CNT if more prom_retain() are
 * called.
 */

#define	CPR_PROM_RETAIN_CNT	1
#define	CPR_MSGBUF		0	/* prom_retain() for msgbuf */
struct prom_retain_info {
	u_int	svaddr;
	u_int	spfn;
	u_int	cnt;
};


/*
 * This structure defines the fixed-length machine dependent data for
 * the sun4u platform.  It is followed in the state file by a variable
 * length section consisting of a sequence of null-terminated strings.
 * The strings currently in use are:
 *
 *	string 1: Definitions of Forth words used by the prom to
 *		  translate kernel mappings
 *
 * Note: The total length (fixed plus variable) of the machine dependent
 * section is stored in the md_size field of cpr_machdep_desc, the
 * machine independent structure which precedes this one in the state file.
 */
struct sun4u_machdep {
	struct sun4u_tlb	dtte[64];
	int	dtte_cnt;
	struct sun4u_tlb	itte[64];
	int	itte_cnt;
	caddr_t tra_va;
	caddr_t mapbuf_va;		/* to store prom mappings */
	u_int	mapbuf_pfn;
	u_int	mapbuf_size;
	u_int	curt_pfn;		/* pfn of curthread */
	u_int	qsav_pfn;		/* qsav pfn */
	int	mmu_ctx_pri;
	int	mmu_ctx_sec;
	int	test_mode;
	struct prom_retain_info	retain[CPR_PROM_RETAIN_CNT];
};

/* Moved from cpr.h */
#define	CPR_MAXCONTIG	4
#define	CPR_MAX_TRANSIZE MMU_PAGESIZE

#define	CPRBOOT	"-F cprbooter cprboot"
#define	CPRBOOT_PATH "/platform/sun4u/cprboot"
#define	CPRBOOT_START 0xedd00000 /* must match value in mapfile.cprboot */

#define	PN_TO_ADDR(pn)  (((unsigned long long)(pn) << MMU_PAGESHIFT))
#define	ADDR_TO_PN(pa)	((pa) >> MMU_PAGESHIFT)
#define	BASE_ADDR(a)	((a) & MMU_PAGEMASK)

#define	prom_map_plat(addr, pa, size, va) \
		if (prom_map(addr, pa, size) == 0) { \
			errp("PROM_MAP failed: vaddr=%x, paddr=%x%8.8x\n", \
			va, (int)(pa >> 32), (int) (pa)); \
			return (-1); \
		}

typedef	u_longlong_t	physaddr_t;

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
extern void i_cpr_handle_xc(int);
extern void i_cpr_restore_hwregs(caddr_t, caddr_t, u_int, u_int, caddr_t,
	caddr_t, u_int);
extern timestruc_t i_cpr_todget();
extern void cpr_restore_mmu(u_int, u_int, u_int, u_int, caddr_t, caddr_t);
extern void cpr_bzero(register char *, register int);



#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CPR_IMPL_H */
