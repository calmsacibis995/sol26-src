/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mlsetup.c	1.29	96/10/17 SMI"

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
#include <vm/hat_ppcmmu.h>
#include <sys/reboot.h>
#include <sys/avintr.h>
#include <sys/vtrace.h>
#include <sys/systeminfo.h>	/* for init_cpu_info */
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/cpupart.h>
#include <sys/pset.h>

/*
 * External Data:
 */
extern struct _kthread t0;

/*
 * Global Routines:
 * mlsetup()
 */
void init_cpu_info(struct cpu *cp);	/* called by start_other_cpus on MP */

/*
 * Global Data:
 */

struct _klwp	lwp0;
struct proc	p0;
struct plock	p0lock;			/* persistent p_lock for p0 */

int	dcache_size;
int	dcache_blocksize;
int	dcache_sets;
int	icache_size;
int	icache_blocksize;
int	icache_sets;
int	unified_cache;
int	clock_frequency;
int	timebase_frequency;

/*
 * Static Routines:
 */
static void fiximp_obp(void);

/*
 * Setup routine called right before main(). Interposing this function
 * before main() allows us to call it in a machine-independent fashion.
 */
void
mlsetup(struct regs *rp, void *cif)
{
	extern struct classfuncs sys_classfuncs;
	extern pri_t maxclsyspri;
	extern void setcputype(void);

	/*
	 * initialize t0
	 */
	t0.t_stk = (caddr_t)rp - SA(MINFRAME);
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
	lwp0.lwp_regs = (void *)rp;
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

	/*
	 * Initialize lists of available and active CPUs.
	 */
	cpu_list_init(CPU);

#ifdef  TRACE
	cpu[0]->cpu_trace.event_map = null_event_map;
#endif  TRACE


	prom_init("kernel", cif);

	setcputype();
	fiximp_obp();
	init_cpu_info(CPU);

#ifdef XXXPPC
	map_wellknown_devices();
	setcpudelay();
#endif

	bootflags();
}

extern int swap_int(int *);

/*
 * Set the magic constants of the implementation
 */
static void
fiximp_obp(void)
{
	dnode_t rootnode, cpunode;
	auto dnode_t sp[OBP_STACKDEPTH];
	int i, a;
	int	dummy;
	pstack_t *stk;
	static struct {
		char	*name;
		int	*var;
	} prop[] = {
		"d-cache-size",		&dcache_size,
		"d-cache-block-size",	&dcache_blocksize,
		"d-cache-sets",		&dcache_sets,
		"i-cache-size",		&icache_size,
		"i-cache-block-size",	&icache_blocksize,
		"i-cache-sets",		&icache_sets,
		"clock-frequency",	&clock_frequency,
		"timebase-frequency",	&timebase_frequency,
	};

	rootnode = prom_rootnode();
	/*
	 * Find the first 'cpu' node - assume that all the
	 * modules are the same type - at least when we're
	 * determining magic constants.
	 */
	stk = prom_stack_init(sp, OBP_STACKDEPTH);
	cpunode = prom_findnode_bydevtype(rootnode, "cpu", stk);
	prom_stack_fini(stk);

	/*
	 * cache-unified is a boolean property: true if present.
	 */
	if (prom_getprop(cpunode, "cache-unified", (caddr_t)&dummy) != -1)
		unified_cache = 1;

	/*
	 * Read integer properties.
	 */
	for (i = 0; i < sizeof (prop) / sizeof (prop[0]); i++) {
		if (prom_getintprop(cpunode, prop[i].name, &a) != -1) {
			*prop[i].var = a;
		}
	}

	if (timebase_frequency > 0) {
		dec_incr_per_tick = timebase_frequency / hz;
		timebase_period = (int)
		    ((1000000000LL << NSEC_SHIFT) / timebase_frequency);
		tbticks_per_10usec = timebase_frequency / 100000;
	}
}

/*
 * Init CPU info - get CPU type info for processor_info system call.
 */
void
init_cpu_info(struct cpu *cp)
{
	register processor_info_t *pi = &cp->cpu_type_info;
	unsigned	clock_freq = 0;
#ifdef MP
	dnode_t		root;
	static	char	freq[] = "clock-frequency";
	dnode_t		nodeid;
	unsigned	who;

	/*
	 * Get clock-frequency property for the CPU.
	 * Find node for CPU.  The first CPU is initialized before the
	 * cpu_nodeid table is setup.
	 */
	root = prom_nextnode((dnode_t)0);	/* root node */
	for (nodeid = prom_childnode(root);
	    (nodeid != OBP_NONODE) && (nodeid != OBP_BADNODE);
	    nodeid = prom_nextnode(nodeid)) {
		if ((prom_getproplen(nodeid, "mid") == sizeof (who)) &&
		    (prom_getprop(nodeid, "mid", (caddr_t)&who) != -1)) {

			if (who == cp->cpu_id) {
				/*
				 * get clock frequency from CPU or root node.
				 * This will be zero if the property is
				 * not there.
				 */
				if (prom_getintprop(nodeid, freq,
				    &clock_freq) ==
				    sizeof (clock_freq))
					break;
			}
		}
	}
#else /* uniprocessor */
	clock_freq = clock_frequency;	/* use value found by fiximp_obp() */
#endif /* MP */

	/*
	 * Round to nearest megahertz.
	 */
	pi->pi_clock = (clock_freq + 500000) / 1000000;

	strcpy(pi->pi_processor_type, architecture);
	strcpy(pi->pi_fputypes, architecture);
}
