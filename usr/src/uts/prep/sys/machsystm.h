/*
 * Copyright (c) 1993, by Sun Microsystems, Inc.
 */

#ifndef _SYS_MACHSYSTM_H
#define	_SYS_MACHSYSTM_H

#pragma ident	"@(#)machsystm.h	1.3	94/11/15 SMI"

/*
 * Numerous platform-dependent interfaces that don't seem to belong
 * in any other header file.
 *
 * This file should not be included by code that purports to be
 * platform-independent.
 *
 */

#include <sys/varargs.h>

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

extern int Cpudelay;
extern void setcpudelay(void);

struct cpu;
extern void init_intr_threads(struct cpu *);

struct _kthread;
extern struct _kthread *clock_thread;
extern void init_clock_thread(void);

extern int setup_panic(char *, va_list);

extern void siron(void);

extern void return_instr(void);

extern int pf_is_memory(u_int);

extern int noprintf;
extern int do_pg_coloring;
extern int use_page_coloring;
extern int pokefault;

extern u_int cpu_nodeid[];

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_MACHSYSTM_H */
