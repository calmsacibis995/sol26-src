
/*
 * Copyright (c) 1987-1995 by Sun Microsystems, Inc.
 */

#ifndef _SYS_CPU_MODULE_H
#define	_SYS_CPU_MODULE_H

#pragma ident	"@(#)cpu_module.h	1.9	96/09/09 SMI"

#include <sys/pte.h>
#include <sys/async.h>

#ifdef	__cplusplus
extern "C" {
#endif


#ifdef _KERNEL

/*
 * The are functions that are expected of the cpu modules.
 */

extern struct module_ops *moduleops;

/*
 * module initialization
 */
void	cpu_setup(void);

/*
 * virtual demap flushes (tlbs & virtual tag caches)
 */
void	vtag_flushpage(caddr_t addr, u_int ctx);
void	vtag_flushctx(u_int ctx);
void	vtag_flushpage_tl1(caddr_t addr, u_int ctx);
void	vtag_flushctx_tl1(u_int ctx);

/*
 * virtual alias flushes (virtual address caches)
 */
void	vac_flushpage(u_int pf, int color);
void	vac_flushpage_tl1(u_int pf, int color);
void	vac_flushcolor(int color);
void	vac_flushcolor_tl1(int color);

/*
 * sending x-calls
 */
void	init_mondo(u_int func, u_int arg1, u_int arg2, u_int arg3, u_int arg4);
void	send_mondo(int upaid);
void	fini_mondo(void);

/*
 * flush instruction cache if needed
 */
void	flush_instr_mem(caddr_t addr, u_int len);

/*
 * take pending fp traps if fpq present
 * this function is also defined in fpusystm.h
 */
void	syncfpu(void);

/*
 * flush ecache & reenable internal caches after ecc errors
 * other cpu-specific error handling routines
 */
void	scrub_ecc(void);
void	ce_err(void);
void	async_err(void);
void	dis_err_panic1(void);
void	clr_datapath(void);
void	cpu_flush_ecache(void);
u_int	cpu_get_status(struct ecc_flt *ecc);

/*
 * retrieve information from the specified tlb entry. these functions are
 * called by "cpr" module
 */
void	itlb_rd_entry(u_int entry, tte_t *tte, caddr_t *addr, int *ctxnum);
void	dtlb_rd_entry(u_int entry, tte_t *tte, caddr_t *addr, int *ctxnum);

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_CPU_MODULE_H */
