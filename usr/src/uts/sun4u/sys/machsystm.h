/*
 * Copyright (c) 1993, by Sun Microsystems, Inc.
 */

#ifndef _SYS_MACHSYSTM_H
#define	_SYS_MACHSYSTM_H

#pragma ident	"@(#)machsystm.h	1.27	96/09/09 SMI"

/*
 * Numerous platform-dependent interfaces that don't seem to belong
 * in any other header file.
 *
 * This file should not be included by code that purports to be
 * platform-independent.
 */

#include <sys/types.h>
#include <sys/scb.h>
#include <sys/varargs.h>
#include <sys/machparam.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL

extern void mp_halt(char *);

extern int splzs(void);
#ifndef splimp
/* XXX go fix kobj.c so we can kill splimp altogether! */
extern int splimp(void);
#endif

extern void setintrenable(int);

extern unsigned int vac_mask;

extern void vac_flushall(void);

extern int obpdebug;

struct cpu;
extern void init_intr_threads(struct cpu *);

struct _kthread;
extern struct _kthread *clock_thread;
extern void init_clock_thread(void);

extern struct scb *set_tbr(struct scb *);
extern void reestablish_curthread(void);
extern int setup_panic(char *, va_list);

extern void send_dirint(int, int);
extern void setsoftint(u_int);
extern void siron(void);
extern uint32_t swapl(uint32_t *, uint32_t);
#ifdef	XXX
extern void set_interrupt_target(int);
#endif	XXX

/*
 * The following enum types determine how interrupts are distributed
 * on a sun4u system.
 *
 *	INTR_CURRENT_CPU - Target interrupt at the CPU running the
 *	add_intrspec thread. Also used to target all interrupts at
 *	the panicing CPU.
 *	INTR_BOOT_CPU - Target all interrupts at the boot CPU.
 *	INTR_FLAT_DIST - Flat distribution of all interrupts.
 */
enum intr_policies {INTR_CURRENT_CPU = 0, INTR_BOOT_CPU,
	INTR_FLAT_DIST};
extern u_int intr_add_cpu(void (*func)(void *, int, u_int),
	void *, int, int);
extern void intr_rem_cpu(int);
extern void intr_redist_all_cpus(enum intr_policies);

/*
 * Structure that defines the interrupt distribution list. It contains
 * enough info about the interrupt so that it can callback the parent
 * nexus driver and retarget the interrupt to a different CPU.
 */
struct intr_dist {
	struct intr_dist *next;		/* link to next in list */
	void (*func)(void *, int, u_int);	/* Callback function */
	void *dip;		/* Nexus parent callback arg 1 */
	int mondo;		/* Nexus parennt callback arg 2 */
	int mask_flag;		/* Mask off lower 3 bits when searching? */
};

extern int getprocessorid(void);
extern caddr_t set_trap_table(void);
extern void get_asyncflt(volatile u_longlong_t *afsr);
extern void set_asyncflt(volatile u_longlong_t *afsr);
extern void get_asyncaddr(volatile u_longlong_t *afar);
extern void reset_ecc(caddr_t vaddr);

extern void stphys(u_longlong_t physaddr, int value);
extern int ldphys(u_longlong_t physaddr);

extern void stdphys(u_longlong_t physaddr, u_longlong_t value);
extern u_longlong_t lddphys(u_longlong_t physaddr);

extern void scrubphys(caddr_t vaddr);

struct regs;

extern void kern_setup1(void);
extern void startup(void);
extern void post_startup(void);

extern int vac;
extern int cache;
extern int use_cache;
extern int use_ic;
extern int use_dc;
extern int use_ec;
extern int use_mp;
extern int do_pg_coloring;
extern int use_page_coloring;
extern int pokefault;
extern u_int module_wb_flush;
extern volatile u_int aflt_ignored;
extern int use_hw_bcopy;
extern int use_hw_copyio;
extern int use_hw_bzero;
extern u_longlong_t ecache_flushaddr;

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_MACHSYSTM_H */
