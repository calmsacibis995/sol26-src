/*
 * Copyright 1994, Sun Microsystems,  Inc.
 */

#ifndef	_SYS_MACHCPUVAR_H
#define	_SYS_MACHCPUVAR_H

#pragma ident	"@(#)machcpuvar.h	1.22	95/03/25 SMI"

#include <sys/reg.h>
#include <sys/xc_levels.h>

#ifdef	__cplusplus
extern "C" {
#endif

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
	int		xc_pend[X_CALL_LEVELS];
	int		xc_wait[X_CALL_LEVELS];
	int		xc_ack[X_CALL_LEVELS];
	int		xc_state[X_CALL_LEVELS];
	int		xc_retval[X_CALL_LEVELS];
	long		mcpu_pri;	/* CPU priority */

	/* ptr to machine dependent data for setting priority level */
	cpu_pri_lev_t	mcpu_pri_data;

	u_long		mcpu_mask;	/* bitmask for this cpu (1<<cpu_id) */
	greg_t		mcpu_r3;	/* level 0 save area */
	greg_t		mcpu_srr0;	/* level 0 save area */
	greg_t		mcpu_srr1;	/* level 0 save area */
	greg_t		mcpu_dsisr;	/* level 0 save area */
	greg_t		mcpu_dar;	/* level 0 save area */

	caddr_t		mcpu_cmntrap;	/* level 1 entry point */
	caddr_t		mcpu_trap_kadb;	/* level 1 entry point */
	caddr_t		mcpu_syscall;	/* level 1 entry point */
	caddr_t		mcpu_cmnint;	/* level 1 entry point */
	caddr_t		mcpu_clockintr;	/* level 1 entry point */

	/* level 1 MSRs */
	greg_t		mcpu_msr_enabled;	/* interrupts enabled */
	greg_t		mcpu_msr_disabled;	/* interrupts disabled */
	/* if kadb, interrupts are disabled */
	greg_t		mcpu_msr_kadb;
};

#define	cpu_pri cpu_m.mcpu_pri
#define	cpu_pri_data cpu_m.mcpu_pri_data
#define	cpu_mask cpu_m.mcpu_mask
#define	cpu_r3 cpu_m.mcpu_r3
#define	cpu_srr0 cpu_m.mcpu_srr0
#define	cpu_srr1 cpu_m.mcpu_srr1
#define	cpu_dsisr cpu_m.mcpu_dsisr
#define	cpu_dar cpu_m.mcpu_dar
#define	cpu_cr cpu_m.mcpu_cr
#define	cpu_lr cpu_m.mcpu_lr
#define	cpu_mq cpu_m.mcpu_mq
#define	cpu_ibatl cpu_m.mcpu_ibatl
#define	cpu_ibatu cpu_m.mcpu_ibatu
#define	cpu_cmntrap cpu_m.mcpu_cmntrap
#define	cpu_trap_kadb cpu_m.mcpu_trap_kadb
#define	cpu_syscall cpu_m.mcpu_syscall
#define	cpu_cmnint cpu_m.mcpu_cmnint
#define	cpu_clockintr cpu_m.mcpu_clockintr
#define	cpu_msr_enabled cpu_m.mcpu_msr_enabled
#define	cpu_msr_disabled cpu_m.mcpu_msr_disabled
#define	cpu_msr_kadb cpu_m.mcpu_msr_kadb
#endif	/* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MACHCPUVAR_H */
