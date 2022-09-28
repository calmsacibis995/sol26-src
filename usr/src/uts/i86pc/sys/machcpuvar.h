/*
 * Copyright 1990, 1993, 1994 Sun Microsystems,  Inc.
 */

#ifndef	_SYS_MACHCPUVAR_H
#define	_SYS_MACHCPUVAR_H

#pragma ident	"@(#)machcpuvar.h	1.23	96/04/17 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/xc_levels.h>
#include <sys/pte.h>
#include <sys/segment.h>
#include <sys/tss.h>
#include <sys/rm_platter.h>

/*
 * XXX This needs to be cleaned up.  Right now this seems
 * the best place to put this.  It is only used in common/os/mutex.c
 * and the function exists for MP syncronization.
 */
#define	mem_sync()

#ifndef	_ASM
/*
 * Machine specific fields of the cpu struct
 * defined in common/sys/cpuvar.h.
 *
 * Note:  This is kinda kludgy but seems to be the best
 * of our alternatives.
 */
typedef void *cpu_pri_lev_t;
struct	machcpu {
	/* define	all the x_call stuff */
	int	xc_pend[X_CALL_LEVELS];
	int	xc_wait[X_CALL_LEVELS];
	int	xc_ack[X_CALL_LEVELS];
	int	xc_state[X_CALL_LEVELS];
	int	xc_retval[X_CALL_LEVELS];

	int		mcpu_pri;		/* CPU priority */
	cpu_pri_lev_t	mcpu_pri_data;		/* ptr to machine dependent */
						/* data for setting priority */
						/* level */

	u_int		mcpu_mask;	/* bitmask for this cpu (1<<cpu_id) */
	struct pte	*mcpu_pagedir;	/* virtual address of page directory */
	struct hat	*mcpu_current_hat; /* current hat in pagedir */

	lock_t		mcpu_pt_lock;	/* used in switching FPU context */
					/* (i.e cpu_fpu pointer) */

	/* i86 hardware table addresses that cannot be shared */
	u_int		 mcpu_cr3;	/* CR3 */
	struct seg_desc *mcpu_gdt;	/* GDT */
	struct gate_desc *mcpu_idt;	/* IDT */
	struct tss386   *mcpu_tss;	/* TSS */
	struct seg_desc *mcpu_ldt;	/* LDT XXX - needed? */
	struct	cpu_tables *mcpu_cp_tables;	/* pointer to space acquired */
						/* while starting up */
						/* auxillary processors */
	kmutex_t	mcpu_ppaddr_mutex;

	caddr_t		mcpu_caddr1;	/* per cpu CADDR1 */
	caddr_t		mcpu_caddr2;	/* per cpu CADDR2 */
	u_int		*mcpu_caddr1pte; /* pte pointer for CADDR1 */
	u_int		*mcpu_caddr2pte; /* pte pointer for CADDR2 */

#ifdef _VPIX
	/* Flag to indicate v86 mode thread */
	int		mcpu_v86procflag;
#endif
};
#endif	/* _ASM */

#define	cpu_pri cpu_m.mcpu_pri
#define	cpu_pri_data cpu_m.mcpu_pri_data
#define	cpu_mask cpu_m.mcpu_mask
#define	cpu_pagedir cpu_m.mcpu_pagedir
#define	cpu_current_hat cpu_m.mcpu_current_hat
#define	cpu_ppaddr_mutex cpu_m.mcpu_ppaddr_mutex
#define	cpu_cr3 cpu_m.mcpu_cr3
#define	cpu_gdt cpu_m.mcpu_gdt
#define	cpu_idt cpu_m.mcpu_idt
#define	cpu_tss cpu_m.mcpu_tss
#define	cpu_ldt cpu_m.mcpu_ldt
#define	cpu_pt_lock cpu_m.mcpu_pt_lock
#define	cpu_caddr1 cpu_m.mcpu_caddr1
#define	cpu_caddr2 cpu_m.mcpu_caddr2
#define	cpu_caddr1pte cpu_m.mcpu_caddr1pte
#define	cpu_caddr2pte cpu_m.mcpu_caddr2pte
#ifdef _VPIX
#define	cpu_v86procflag cpu_m.mcpu_v86procflag
#endif


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MACHCPUVAR_H */
