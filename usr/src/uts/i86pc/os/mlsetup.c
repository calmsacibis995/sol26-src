/*
 * Copyright (c) 1990-1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)mlsetup.c	1.18	96/05/20 SMI"

#include <sys/types.h>
#include <sys/disp.h>
#include <sys/msgbuf.h>
#include <sys/promif.h>
#include <sys/clock.h>
#include <sys/pte.h>
#include <sys/mmu.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/stack.h>
#include <vm/as.h>
#include <vm/hat_i86.h>
#include <sys/reboot.h>
#include <sys/avintr.h>
#include <sys/vtrace.h>
#include <sys/proc.h>
#include <sys/cpupart.h>
#include <sys/pset.h>

/*
 * External Routines:
 */

extern void bootflags(void);

/*
 * External Data:
 */
extern struct _kthread t0;

/*
 * Global Routines:
 * mlsetup()
 */

/*
 * Global Data:
 */

struct _klwp	lwp0;
struct proc	p0;
struct plock	p0lock;			/* persistent p_lock for p0 */

/*
 * Setup routine called right before main(). Interposing this function
 * before main() allows us to call it in a machine-independent fashion.
 */
void
mlsetup(rp)
	struct regs *rp;
{
	extern struct classfuncs sys_classfuncs;
	extern pri_t maxclsyspri;

	extern struct tss386 ktss;
	extern struct seg_desc ldt_default[];
	extern struct seg_desc gdt[];
	extern struct gate_desc idt[];
	extern struct seg_desc default_ldt_desc;
	extern void bootflags(void);
	extern int kadb_is_running;

	/*
	 * initialize t0
	 */
	t0.t_stk = (caddr_t)rp - MINFRAME;
	t0.t_pri = maxclsyspri - 3;
	t0.t_schedflag = TS_LOAD | TS_DONT_SWAP;
	t0.t_procp = &p0;
	t0.t_plockp = &p0lock.pl_lock;
	t0.t_lwp = &lwp0;
	t0.t_forw = &t0;
	t0.t_back = &t0;
	t0.t_next = &t0;
	t0.t_prev = &t0;
	t0.t_cpu = cpu[0];
	t0.t_disp_queue = &cpu[0]->cpu_disp;
	t0.t_bind_cpu = PBIND_NONE;
	t0.t_bind_pset = PS_NONE;
	t0.t_cpupart = &cp_default;
	t0.t_clfuncs = &sys_classfuncs.thread;
	THREAD_ONPROC(&t0, CPU);

	lwp0.lwp_thread = &t0;
	lwp0.lwp_regs = (void *) rp;
	lwp0.lwp_procp = &p0;
	t0.t_tid = p0.p_lwpcnt = p0.p_lwprcnt = p0.p_lwptotal = 1;

	p0.p_exec = NULL;
	p0.p_stat = SRUN;
	p0.p_flag = SSYS;
	p0.p_tlist = &t0;
	p0.p_stksize = 2*PAGESIZE;
	p0.p_as = &kas;
	p0.p_lockp = &p0lock;
	sigorset(&p0.p_ignore, &ignoredefault);

	CPU->cpu_thread = &t0;
	CPU->cpu_dispthread = &t0;
	CPU->cpu_idle_thread = &t0;
	CPU->cpu_disp.disp_cpu = CPU;
	CPU->cpu_flags = CPU_READY | CPU_RUNNING | CPU_EXISTS | CPU_ENABLE;

	CPU->cpu_mask = 1;
	CPU->cpu_id = 0;

	CPU->cpu_tss = &ktss;

	CPU->cpu_ldt = ldt_default;	/* default LDT */
	CPU->cpu_gdt = gdt;

	default_ldt_desc = CPU->cpu_gdt[seltoi(LDTSEL)];
	p0.p_ldt_desc = default_ldt_desc;

	CPU->cpu_idt = idt;		/* kernel IDT */

	/*
	 * Initialize lists of available and active CPUs.
	 */
	cpu_list_init(CPU);

	rp->r_ebp = 0;	/* terminate kernel stack traces! */

#ifdef  TRACE
	cpu[0]->cpu_trace.event_map = null_event_map;
#endif  TRACE

	prom_init("Kernel", (void *)0);

	bootflags();

	if (kadb_is_running)
		boothowto |= RB_DEBUG;

}
