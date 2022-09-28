/*
 * Copyright (c) 1992, 1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)mp_startup.c	1.57	96/08/19 SMI"

#include <sys/types.h>
#include <sys/thread.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/class.h>
#include <sys/cmn_err.h>
#include <sys/map.h>
#include <sys/intreg.h>
#include <sys/debug.h>
#include <sys/x_call.h>
#include <sys/vtrace.h>
#include <sys/var.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <vm/hat_sfmmu.h>
#include <vm/as.h>
#include <sys/systm.h>
#include <sys/machsystm.h>
#include <sys/kmem.h>
#include <sys/callb.h>

#ifdef TRAPTRACE
#include <sys/traptrace.h>
#include <sys/bootconf.h>
#endif /* TRAPTRACE */

struct cpu	cpu0;	/* the first cpu data; statically allocate */
struct cpu	*cpus;	/* pointer to other cpus; dynamically allocate */
struct cpu	*cpu[NCPU];		/* pointers to all CPUs */

extern int snooping;
extern u_int snoop_interval;
extern void deadman();
extern void tickcmpr_reset();
extern void tickint_clnt_add();


#ifdef	MP				/* Around almost entire file */

/* bit mask of cpus ready for x-calls, protected by cpu_lock */
cpuset_t cpu_ready_set;

/* bit mask of cpus to bring up */
#ifdef MPSAS
cpuset_t cpu_bringup_set = (u_int)0x1;
#else
cpuset_t cpu_bringup_set = CPUSET_ALL;
#endif

/*
 * Useful for disabling MP bring-up for an MP capable kernel
 * (a kernel that was built with MP defined)
 */
int use_mp = 1;			/* set to come up mp */

int intr_policy = INTR_FLAT_DIST;	/* interrupt distribution policy */

static void	mp_startup(void);

/*
 * Callback routines to support cpr
 */
static void	cprboot_mp_startup(void);
static void	cprboot_mp_startup_init(int);
static void	mpstart_cpr_callb(void *, int code);

/* Global lock which protects the interrupt distribution lists */
kmutex_t intr_dist_lock;

/*
 * Mask to use when searching for a matching mondo. This masks off the
 * lower 3 bits, which are the bits which define level for an external
 * SBus slot mondo. This allows the code to seacrh for unique key properly.
 */
#define	SBUS_LVL_MASK	0x7

/* Head of the interrupt distriubution list */
struct intr_dist *intr_dist_head = NULL;

/*
 * static routine to support interrupt distribution.
 */
static u_int intr_dist_elem(enum intr_policies);

/*
 * Init CPU info - get CPU type info for processor_info system call.
 */
static void
init_cpu_info(struct cpu *cp)
{
	register processor_info_t *pi = &cp->cpu_type_info;
	int	clock_freq = 0;
	int	cpuid;
	struct cpu_node *cpunode;
	extern char cpu_info_buf[NCPU][CPUINFO_SZ];

	cpuid = cp->cpu_id;
	cp->cpu_m.cpu_info = &cpu_info_buf[cpuid][0];

	/*
	 * Get clock-frequency property from cpunodes[] for the CPU.
	 */
	cpunode = &cpunodes[cpuid];
	clock_freq = cpunode->clock_freq;

	pi->pi_clock = (clock_freq + 500000) / 1000000;

	(void) strcpy(pi->pi_processor_type, "sparc");
	(void) strcpy(pi->pi_fputypes, "sparc");
}


#ifdef	TRAPTRACE
/*
 * This function bop allocs traptrace buffers for all cpus
 * other than boot cpu.
 */
caddr_t
trap_trace_alloc(caddr_t base)
{
	caddr_t	vaddr;
	extern int ncpunode;

	if (ncpunode == 1) {
		return (base);
	}

	if ((vaddr = (caddr_t)BOP_ALLOC(bootops, base, (TRAP_TSIZE *
		(ncpunode - 1)), TRAP_TSIZE)) == NULL) {
		cmn_err(CE_PANIC, "traptrace_alloc: can't bop alloc\n");
	}
	ttrace_buf = vaddr;
	ttrace_index = 0;
	return (vaddr + (TRAP_TSIZE * (ncpunode - 1)));
}
#endif	/* TRAPTRACE */

/*
 * Multiprocessor initialization.
 *
 * Allocate and initialize the cpu structure, startup and idle threads
 * for the specified CPU.
 *
 * If cprboot is set, it is called by the cpr callback routine other
 * than normal boot.
 */
static void
mp_startup_init(cpuid, cprboot)
	register int cpuid;
	int cprboot;
{
	struct cpu *cp;
	kthread_id_t tp;
	caddr_t	sp;
	proc_t *procp;
	char buf[100];
	sfmmu_t *sfmmup;
	extern pri_t maxclsyspri;
	extern void idle();
	extern void init_intr_threads(struct cpu *);
	static int cpunum;
#ifdef TRAPTRACE
	TRAP_TRACE_CTL	*ctlp;
	caddr_t	newbuf;
#endif /* TRAPTRACE */

	ASSERT((cpuid < NCPU && cpu[cpuid] == NULL) || cprboot);

	/*
	 * Obtain pointer to the appropriate cpu structure.
	 */
	if (cprboot)
		cp = cpu[cpuid];
	else
		cp = &cpus[cpunum];

	procp = curthread->t_procp;

#ifdef TRAPTRACE
	/*
	 * allocate a traptrace buffer for this CPU.
	 */
	if (!cprboot) {
		ctlp = &trap_trace_ctl[cpuid];
		newbuf = (caddr_t) (ttrace_buf + (ttrace_index * TRAP_TSIZE));
		ctlp->d.vaddr_base = newbuf;
		ttrace_index++;
		ctlp->d.offset = ctlp->d.last_offset = 0;
		ctlp->d.limit = trap_trace_bufsize;
		ctlp->d.paddr_base = va_to_pa(newbuf);
	}
#endif /* TRAPTRACE */

	/*
	 * Allocate and initialize the startup thread for this CPU.
	 */
	tp = thread_create(NULL, NULL, mp_startup_init, NULL, 0, procp,
	    TS_STOPPED, maxclsyspri);
	if (tp == NULL) {
		cmn_err(CE_PANIC,
	"mp_startup_init: Can't create startup thread for cpu: %d", cpuid);
		/*NOTREACHED*/
	}

	/*
	 * Set state to TS_ONPROC since this thread will start running
	 * as soon as the CPU comes online.
	 *
	 * All the other fields of the thread structure are setup by
	 * thread_create().
	 */
	THREAD_ONPROC(tp, cp);
	tp->t_preempt = 1;
	tp->t_bound_cpu = cp;
	tp->t_affinitycnt = 1;
	tp->t_cpu = cp;

	sfmmup = astosfmmu(&kas);
	CPUSET_ADD(sfmmup->sfmmu_cpusran, cpuid);

	/*
	 * Setup thread to start in mp_startup.
	 */
	sp = tp->t_stk;
	if (cprboot)
		tp->t_pc = (u_int)cprboot_mp_startup - 8;
	else
		tp->t_pc = (u_int)mp_startup - 8;
	tp->t_sp = (u_int)((struct rwindow *)sp - 1);

	cp->cpu_id = cpuid;
	cp->cpu_thread = tp;
	cp->cpu_lwp = NULL;
	cp->cpu_dispthread = tp;
#ifdef TRACE
	cp->cpu_trace.event_map = null_event_map;
#endif /* TRACE */

	if (cprboot)
		return;

	/*
	 * Now, initialize per-CPU idle thread for this CPU.
	 */
	tp = thread_create(NULL, NBPG, idle, NULL, 0, procp, TS_ONPROC, -1);
	if (tp == NULL) {
		cmn_err(CE_PANIC,
		"mp_startup_init: Can't create idle thread for cpu: %d", cpuid);
		/*NOTREACHED*/
	}
	cp->cpu_idle_thread = tp;

	tp->t_preempt = 1;
	tp->t_bound_cpu = cp;
	tp->t_affinitycnt = 1;
	tp->t_cpu = cp;

	init_cpu_info(cp);

	/*
	 * Initialize per-CPU statistics locks.
	 */
	sprintf(buf, "cpu %d statistics lock", cpuid);
	mutex_init(&cp->cpu_stat.cpu_stat_lock, buf,
	    MUTEX_DEFAULT, DEFAULT_WT);

	/*
	 * Initialize the interrupt threads for this CPU
	 *
	 * We used to do it in mp_startup() - but that's just wrong
	 * - we might sleep while allocating stuff from segkp - that
	 * would leave us in a yucky state where we'd be handling
	 * interrupts without an interrupt thread .. see 1120597.
	 */
	init_intr_pool(cp);
	init_intr_threads(cp);

	/*
	 * Record that we have another CPU.
	 */
	mutex_enter(&cpu_lock);
	/*
	 * Add CPU to list of available CPUs.  It'll be on the active list
	 * after mp_startup().
	 */
	cpu_add_unit(cp);
	mutex_exit(&cpu_lock);
	cpunum++;
}

/*
 * If cprboot is set, it is called by the cpr callback routine other
 * than normal boot.
 */
void
start_other_cpus(cprboot)
	int cprboot;
{
	extern struct cpu_node cpunodes[];
	extern int use_prom_stop;
	extern int ncpunode;
	dnode_t nodeid;
	int cpuid, mycpuid;
	int delays;
	int i;

	extern caddr_t cpu_startup;
	extern void idlestop_init(void);

	/*
	 * Initialize our own cpu_info.
	 */
	if (!cprboot) {
		init_cpu_info(CPU);
		cmn_err(CE_CONT, "!boot cpu (%d) initialization complete - "
		    "online\n", CPU->cpu_id);
	}

	if (!use_mp) {
		cmn_err(CE_CONT, "?***** Not in MP mode\n");
		return;
	}

#ifdef DEBUG
	if ((ncpunode > 1) && !use_prom_stop)
		cmn_err(CE_NOTE, "Not using PROM interface for cpu stop");
#endif /* DEBUG */

	/*
	 * perform such initialization as is needed
	 * to be able to take CPUs on- and off-line.
	 */
	if (!cprboot) {
		cpu_pause_init();
		xc_init();		/* initialize processor crosscalls */
		idlestop_init();
	}

	mycpuid = getprocessorid();

	for (i = 0; i < NCPU; i++) {
		if ((nodeid = cpunodes[i].nodeid) == (dnode_t)0)
			continue;

		cpuid = UPAID2CPU(cpunodes[i].upaid);

		if (cpuid == mycpuid) {
			if (!CPU_IN_SET(cpu_bringup_set, cpuid)) {
				cmn_err(CE_WARN, "boot cpu not a member "
				    "of cpu_bringup_set, adding it");
				CPUSET_ADD(cpu_bringup_set, cpuid);
			}
			continue;
		}

		if (!CPU_IN_SET(cpu_bringup_set, cpuid))
			continue;

		if (cprboot)
			cprboot_mp_startup_init(cpuid);
		else
			mp_startup_init(cpuid, 0);

		(void) prom_startcpu(nodeid, (caddr_t)&cpu_startup, cpuid);

		DELAY(50);	/* let's give it a little of time */

		delays = 0;
		while (!CPU_IN_SET(cpu_ready_set, cpuid)) {
			DELAY(0x10000);
			delays++;
			if (delays > 20) {
				cmn_err(CE_PANIC,
					"cpu %d node %x failed to start\n",
					cpuid, nodeid);
				break;
			}
		}
	}
	if (!cprboot)
		callb_add(mpstart_cpr_callb, 0, CB_CL_CPR_MPSTART, "mpstart");

	/* redistribute the interrupts to all CPUs. */
	mutex_enter(&cpu_lock);
	intr_redist_all_cpus(intr_policy);
	mutex_exit(&cpu_lock);
}

/*
 * Startup function for 'other' CPUs (besides 0).
 * Resumed from cpu_startup.
 */
static void
mp_startup(void)
{
	struct cpu *cp = CPU;

	cp->cpu_m.mutex_ready = 1;
	cp->cpu_m.poke_cpu_outstanding = B_FALSE;
	(void) spl0();				/* enable interrupts */
	mutex_enter(&cpu_lock);
	CPUSET_ADD(cpu_ready_set, cp->cpu_id);

	cp->cpu_flags |= CPU_RUNNING | CPU_READY |
	    CPU_ENABLE | CPU_EXISTS;		/* ready */
	cpu_add_active(cp);

	mutex_exit(&cpu_lock);

	/*
	 * Because mp_startup() gets fired off after init() starts, we
	 * can't use the '?' trick to do 'boot -v' printing - so we
	 * always direct the 'cpu .. online' messages to the log.
	 */
	cmn_err(CE_CONT, "!cpu %d initialization complete - online\n",
	    cp->cpu_id);

	/* reset TICK_Compare register */
	tickcmpr_reset();
	if (snooping)
		tickint_clnt_add(deadman, snoop_interval);


	/*
	 * Now we are done with the startup thread, so free it up.
	 */
	thread_exit();
	cmn_err(CE_PANIC, "mp_startup: cannot return");
	/*NOTREACHED*/
}

static void
cprboot_mp_startup_init(cpun)
	register int cpun;
{
	struct cpu *cp;
	kthread_id_t tp;
	caddr_t sp;
	extern void idle();

	mp_startup_init(cpun, 1);

	/*
	 * idle thread t_lock is held when the idle thread is suspended.
	 * Manually unlock the t_lock of idle loop so that we can resume
	 * the suspended idle thread.
	 * Also adjust the PC of idle thread for re-retry.
	 */
	cp = cpu[cpun];
	cp->cpu_on_intr = 0;	/* clear the value from previous life */
	cp->cpu_m.mutex_ready = 0; /* we are not ready yet */
	lock_clear(&cp->cpu_idle_thread->t_lock);
	tp = cp->cpu_idle_thread;

	sp = tp->t_stk;
	tp->t_sp = (u_int)((struct rwindow *)sp - 1);
	tp->t_pc = (u_int) idle - 8;
}

static void
cprboot_mp_startup(void)
{
	struct cpu *cp = CPU;

	CPU->cpu_m.mutex_ready = 1;	/* now, we are ready for mutex */

	(void) spl0();		/* enable interrupts */

	mutex_enter(&cpu_lock);
	CPUSET_ADD(cpu_ready_set, cp->cpu_id);

	/*
	 * The cpu was offlined at suspend time. Put it back to the same state.
	 */
	CPU->cpu_flags |= CPU_RUNNING | CPU_READY | CPU_EXISTS
		| CPU_OFFLINE | CPU_QUIESCED;

	mutex_exit(&cpu_lock);

	/*
	 * Now we are done with the startup thread, so free it up and switch
	 * to idle thread. thread_exit() must be used here because this is
	 * the firest thread in the system since boot and normal scheduling
	 * is not ready yet.
	 */
	thread_exit();
	cmn_err(CE_PANIC, "cprboot_mp_startup: cannot return");
	/*NOTREACHED*/
}

/*ARGSUSED*/
void
mpstart_cpr_callb(void *arg, int code)
{
	cpu_t	*cp;

	switch (code) {
	case CB_CODE_CPR_CHKPT:
		break;

	case CB_CODE_CPR_RESUME:
		/*
		 * All of the non-boot cpus are not ready at this moment, yet
		 * the previous kernel image resumed has cpu_flags ready and
		 * other bits set. It's necesssary to clear the cpu_flags to
		 * match the cpu h/w status; otherwise x_calls are not going
		 * to work properly.
		 */
		for (cp = CPU->cpu_next; cp != CPU; cp = cp->cpu_next) {
			cp->cpu_flags = 0;
			CPUSET_DEL(cpu_ready_set, cp->cpu_id);
		}

		start_other_cpus(1);
		break;
	}
}

/*
 * Start CPU on user request.
 */
/* ARGSUSED */
int
mp_cpu_start(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	return (0);			/* nothing special to do on this arch */
}

/*
 * Stop CPU on user request.
 */
/* ARGSUSED */
int
mp_cpu_stop(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	return (0);			/* nothing special to do on this arch */
}

/*
 * Take the specified CPU out of participation in interrupts.
 *	Called by p_online(2) when a processor is being taken off-line.
 *	This allows interrupt threads being handled on the processor to
 *	complete before the processor is idled.
 *
 *	Returns a non-zero error number (EBUSY) if not all interrupts can
 *	be disabled.
 */
int
cpu_disable_intr(struct cpu *cp)
{
	int retval = 0;

	ASSERT(MUTEX_HELD(&cpu_lock));

	/*
	 * Turn off the CPU_ENABLE flag before calling the redistribution
	 * function, since it checks for this in the cpu flags.
	 */
	cp->cpu_flags &= ~CPU_ENABLE;

	if (cp != &cpu0) {
		intr_redist_all_cpus(intr_policy);
	} else {
		retval = EBUSY;
	}

	return (retval);
}

/*
 * Allow the specified CPU to participate in interrupts.
 *	Called by p_online(2) if a processor could not be taken off-line
 *	because of bound threads, in order to resume processing interrupts.
 *	Also called after starting a processor.
 */
void
cpu_enable_intr(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));

	cp->cpu_flags |= CPU_ENABLE;

	if (cp != &cpu0) {
		intr_redist_all_cpus(intr_policy);
	}
}

/*
 * Add the specified interrupt to the interrupt list. Then call the
 * policy funtion to determine the CPU to target the interrupt at.
 * returns the CPU MID. The func entry is a pointer to a
 * callback function into the nexus driver. This along with
 * instance of the nexus driver and the mondo, allows interrupts to be
 * retargeted at different CPUs.
 */
u_int
intr_add_cpu(void (*func)(void *, int, u_int), void *dip, int mondo,
	int mask_flag)
{
	struct intr_dist *ptr;

	ASSERT(MUTEX_HELD(&intr_dist_lock));
	ASSERT(func);
	ASSERT(dip);

	/* Allocate a new interrupt element and fill in all the fields. */
	ptr = (struct intr_dist *) kmem_zalloc(sizeof (struct intr_dist),
		KM_SLEEP);

	ptr->func = func;
	ptr->dip = dip;
	ptr->mondo = mondo;
	ptr->mask_flag = mask_flag;
	ptr->next = intr_dist_head;
	intr_dist_head = ptr;

	/*
	 * Call the policy function to determine a CPU target for this
	 * interrupt.
	 */
	return (intr_dist_elem(intr_policy));
}

/*
 * Search for the interupt distribution structure with the specified
 * mondo vec reg in the interrupt distribution list. If a match is found,
 * then delete the entry from the list. The caller is responsible for
 * modifying the mondo vector registers.
 */
void
intr_rem_cpu(int mondo)
{
	struct intr_dist *iptr;
	struct intr_dist **vect;

	ASSERT(MUTEX_HELD(&intr_dist_lock));

	for (iptr = intr_dist_head,
	    vect = &intr_dist_head; iptr != NULL;
	    vect = &iptr->next, iptr = iptr->next) {
		if (iptr->mask_flag) {
			if ((mondo & ~SBUS_LVL_MASK) ==
			    (iptr->mondo & ~SBUS_LVL_MASK)) {
				*vect = iptr->next;
				kmem_free(iptr, sizeof (struct intr_dist));
				return;
			}
		} else {
			if (mondo == iptr->mondo) {
				*vect = iptr->next;
				kmem_free(iptr, sizeof (struct intr_dist));
				return;
			}
		}
	}
	if (!panicstr) {
		cmn_err(CE_PANIC,
			"Mondo %x not found on interrupt distribution list",
			mondo);
	}
}

/*
 * Redistribute all interrupts
 *
 * This function redistributes all interrupting devices, running the
 * parent callback functions for each node.
 */
void
intr_redist_all_cpus(enum intr_policies policy)
{
	struct intr_dist *iptr;
	u_int cpu_id;

	ASSERT(MUTEX_HELD(&cpu_lock));

	mutex_enter(&intr_dist_lock);

	/* now distribute all interrupts from the list */
	for (iptr = intr_dist_head; iptr != NULL; iptr = iptr->next) {
		/* now distribute it into the new intr array */
		cpu_id = intr_dist_elem(policy);

		/* run the callback and inform the parent */
		if (iptr->func != NULL) {
			iptr->func(iptr->dip, iptr->mondo, cpu_id);
		}
	}

	mutex_exit(&intr_dist_lock);
}

/*
 * Determine what CPU to target, based on the interrupt policy passed
 * in.
 */
static u_int
intr_dist_elem(enum intr_policies policy)
{
	static struct cpu *curr_cpu = &cpu0;
	struct cpu *new_cpu;

	switch (policy) {
	case INTR_CURRENT_CPU:
		curr_cpu = CPU;
		break;

	case INTR_BOOT_CPU:
		curr_cpu = &cpu0;
		break;

	case INTR_FLAT_DIST:
	default:
		/*
		 * move the current CPU to the next one.
		 */
		new_cpu = curr_cpu->cpu_next_onln;
		for (; new_cpu != curr_cpu; new_cpu =
		    new_cpu->cpu_next_onln) {
			/* Is it OK to add interrupts to this CPU? */
			if (new_cpu->cpu_flags & CPU_ENABLE) {
				curr_cpu = new_cpu;
				break;
			}
		}
	}

	return (curr_cpu->cpu_id);
}

#endif	/* MP */
