/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */

#ifndef	_SYS_XC_IMPL_H
#define	_SYS_XC_IMPL_H

#pragma ident	"@(#)xc_impl.h	1.24	96/08/15 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _ASM

#include <sys/cpu_module.h>

int xc_serv_inum; /* software interrupt number for xc_serv() */
int xc_loop_inum; /* software interrupt number for xc_loop() */

kmutex_t xc_sys_mutex;	/* 1 per system; protect xcall session and xc_mbox */
int xc_spl_enter[NCPU];	/* 1 per cpu; protect sending x-call */
int xc_holder = -1;	/* the cpuid who initiates xc_attention, 0 is valid */

/*
 * Mail box for handshaking and xcall request; protected by xc_sys_mutex
 */
struct xc_mbox {
	u_int	(*xc_func)();
	u_int	xc_arg1;
	u_int	xc_arg2;
	cpuset_t xc_cpuset;
	u_int	xc_state;
} xc_mbox[NCPU];

extern cpuset_t cpu_ready_set;	/* cpus ready for x-call */
extern void send_self_xcall(struct cpu *, u_int, u_int, u_int, u_int, u_int);
extern int xc_loop(void);
extern int xc_serv(void);
extern void xc_stop(struct regs *);
extern greg_t getpil(void);
extern void xc_trace(u_int, cpuset_t, u_int, u_int, u_int, u_int);
extern void flush_windows(void);
extern int splx(int);
extern int splr(int);

/*
 * XC_MONDOTIME is the timeout value for the mondo to be received by the
 * target cpu once it has been sent by the initiating cpu.
 *
 * XC_FUNCTIME is the timeout value for the xcall function to be executed
 * on the target cpu. This timeout is big and depends on what how long
 * the function will take to execute. For now, 500000000 seems large enough.
 */
#define	XC_MONDOTIME	100000
#define	XC_FUNCTIME	500000000

/*
 * Protect the dispatching of the mondo vector
 */

#define	XC_SPL_ENTER(cpuid, opl)					\
{									\
	opl = splr(XCALL_PIL);						\
	cpuid = CPU->cpu_id;						\
	if (xc_spl_enter[cpuid] && !panicstr)				\
		cmn_err(CE_PANIC, "XC SPL ENTER already entered (0x%x)]",\
		cpuid);							\
	xc_spl_enter[cpuid] = 1;					\
}

#define	XC_SPL_EXIT(cpuid, opl)				\
{							\
	ASSERT(xc_spl_enter[cpuid] != 0);		\
	xc_spl_enter[cpuid] = 0;			\
	(void) splx(opl);				\
}

/*
 * send out the mondo to cpus in the cpuset
 */
#define	SEND_MONDO_ONLY(xc_cpuset) 			\
{							\
	int pix;					\
	cpuset_t  tmpset = xc_cpuset;			\
	for (pix = 0; pix < NCPU; pix++) {		\
		if (CPU_IN_SET(tmpset, pix)) {		\
			send_mondo(pix);		\
			CPUSET_DEL(tmpset, pix);	\
			CPU_STAT_ADDQ(CPU, cpu_sysinfo.xcalls, 1);	\
			if (CPUSET_ISNULL(tmpset)) {	\
				break;			\
			}				\
		}					\
	}						\
}

/*
 * set up a x-call request
 */
#define	XC_SETUP(cpuid, func, arg1, arg2)		\
{							\
	xc_mbox[cpuid].xc_func = func;			\
	xc_mbox[cpuid].xc_arg1 = arg1;			\
	xc_mbox[cpuid].xc_arg2 = arg2;			\
	xc_mbox[cpuid].xc_state = XC_DOIT;		\
}

/*
 * set up x-call requests to the cpuset
 */
#define	SEND_MBOX_ONLY(xc_cpuset, func, arg1, arg2, lcx, state)		\
{									\
	int pix;							\
	cpuset_t  tmpset = xc_cpuset;					\
	for (pix = 0; pix < NCPU; pix++) {				\
		if (CPU_IN_SET(tmpset, pix)) {				\
			ASSERT(MUTEX_HELD(&xc_sys_mutex));		\
			ASSERT(CPU_IN_SET(xc_mbox[lcx].xc_cpuset, pix));\
			ASSERT(xc_mbox[pix].xc_state == state);		\
			XC_SETUP(pix, func, arg1, arg2);		\
			membar_stld();					\
			CPUSET_DEL(tmpset, pix);			\
			CPU_STAT_ADDQ(CPU, cpu_sysinfo.xcalls, 1);	\
			if (CPUSET_ISNULL(tmpset)) {			\
				break;					\
			}						\
		}							\
	}								\
}

/*
 * set up and notify a x-call request to the cpuset
 */
#define	SEND_MBOX_MONDO(xc_cpuset, func, arg1, arg2, state)	\
{								\
	int pix;						\
	cpuset_t  tmpset = xc_cpuset;				\
	for (pix = 0; pix < NCPU; pix++) {			\
		if (CPU_IN_SET(tmpset, pix)) {			\
			ASSERT(xc_mbox[pix].xc_state == state);	\
			XC_SETUP(pix, func, arg1, arg2);	\
			membar_stld();				\
			send_mondo(pix);			\
			CPUSET_DEL(tmpset, pix);		\
			CPU_STAT_ADDQ(CPU, cpu_sysinfo.xcalls, 1);	\
			if (CPUSET_ISNULL(tmpset)) {		\
				break;				\
			}					\
		}						\
	}							\
}

/*
 * wait x-call requests to be completed
 */
#define	WAIT_MBOX_DONE(xc_cpuset, lcx, state)				\
{									\
	int pix;							\
	int loop_cnt = 0;						\
	cpuset_t tmpset;						\
	cpuset_t  recv_cpuset;						\
	CPUSET_ZERO(recv_cpuset);					\
	while (!CPUSET_ISEQUAL(recv_cpuset, xc_cpuset)) {		\
		tmpset = xc_cpuset;					\
		for (pix = 0; pix < NCPU; pix++) {			\
			if (CPU_IN_SET(tmpset, pix)) {			\
				if (xc_mbox[pix].xc_state == state) {	\
					CPUSET_ADD(recv_cpuset, pix);	\
				}					\
			}						\
			CPUSET_DEL(tmpset, pix);			\
			if (CPUSET_ISNULL(tmpset)) {			\
				break;					\
			}						\
		}							\
		if (loop_cnt++ > XC_FUNCTIME) {				\
			panic("WAIT_MBOX_DONE() timeout, "		\
				"recv_cpuset 0x%x, xc cpuset 0x%x ",	\
				recv_cpuset, xc_cpuset);		\
		}							\
	}								\
}

/*
 * xc_state flags
 */
#define	XC_IDLE		0 /* not in the xc_loop(); set by xc_loop */
#define	XC_ENTER	1 /* entering xc_loop(); set by xc_attention */
#define	XC_WAIT		2 /* entered xc_loop(); set by xc_loop */
#define	XC_DOIT		3 /* xcall request; set by xc_one, xc_some, or xc_all */
#define	XC_EXIT		4 /* exiting xc_loop(); set by xc_dismissed */

/*
 * user provided handlers must be pc aligned
 */
#define	PC_ALIGN 4

#ifdef	DEBUG
/*
 * get some statistics when xc/xt routines are called
 */

#define	XC_TRACE(a, b, c, d, e, f) xc_trace(a, b, c, d, e, f)
#define	XC_STAT_INC(a)	(a)++;
#define	XC_CPUID	0

#define	XT_ONE_SELF	1
#define	XT_ONE_OTHER	2
#define	XT_SOME_SELF	3
#define	XT_SOME_OTHER	4
#define	XT_ALL_SELF	5
#define	XT_ALL_OTHER	6
#define	XC_ONE_SELF	7
#define	XC_ONE_OTHER	8
#define	XC_ONE_OTHER_H	9
#define	XC_SOME_SELF	10
#define	XC_SOME_OTHER	11
#define	XC_SOME_OTHER_H	12
#define	XC_ALL_SELF	13
#define	XC_ALL_OTHER	14
#define	XC_ALL_OTHER_H	15
#define	XC_ATTENTION	16
#define	XC_DISMISSED	17
#define	XC_LOOP_ENTER	18
#define	XC_LOOP_DOIT	19
#define	XC_LOOP_EXIT	20

extern	u_int x_dstat[NCPU][XC_LOOP_EXIT+1];
extern	u_int x_rstat[NCPU][4];
#define	XC_LOOP		1
#define	XC_SERV		2

#define	XC_STAT_INIT(cpuid) 				\
{							\
	x_dstat[cpuid][XC_CPUID] = 0xffffff00 | cpuid;	\
	x_rstat[cpuid][XC_CPUID] = 0xffffff00 | cpuid;	\
}

#else	DEBUG
#define	XC_TRACE(a, b, c, d, e, f)
#define	XC_STAT_INIT(cpuid)
#define	XC_STAT_INC(a)
#define	XC_ATTENTION_CPUSET(x)
#define	XC_DISMISSED_CPUSET(x)
#endif	DEBUG

#endif	/* !_ASM */

/*
 * Maximum delay in milliseconds to wait for send_mondo to complete
 */
#define	XC_SEND_MONDO_MSEC	1000

/* These are currently used by kadb (remove when 1231551 fixed) */
#define	XC_BUSY_COUNT	15000000 /* ~8 instr in busy check's loop */
#define	XC_NACK_COUNT	1000000	/* each count is 1usec in nack check's loop */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_XC_IMPL_H */
