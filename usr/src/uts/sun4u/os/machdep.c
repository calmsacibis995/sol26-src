/*
 * Copyright (c) 1992-1996, by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)machdep.c	1.111	96/09/24 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <sys/user.h>
#include <sys/map.h>
#include <sys/mman.h>
#include <sys/vm.h>

#include <sys/disp.h>
#include <sys/class.h>

#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/kmem.h>

#include <sys/reboot.h>
#include <sys/uadmin.h>

#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/callo.h>
#include <sys/msgbuf.h>

#include <sys/procfs.h>
#include <sys/acct.h>

#include <sys/vfs.h>
#include <sys/dnlc.h>
#include <sys/var.h>
#include <sys/cmn_err.h>
#include <sys/utsname.h>
#include <sys/debug.h>
#include <sys/debug/debug.h>

#include <sys/dumphdr.h>
#include <sys/bootconf.h>
#include <sys/async.h>
#include <sys/varargs.h>
#include <sys/promif.h>
#include <sys/prom_isa.h>
#include <sys/prom_plat.h>
#include <sys/comvec.h>
#include <sys/modctl.h>		/* for "procfs" hack */
#include <sys/kvtopdata.h>

#include <sys/consdev.h>
#include <sys/frame.h>

#define	SUNDDI_IMPL		/* so sunddi.h will not redefine splx() et al */
#include <sys/sunddi.h>
#include <sys/ddidmareq.h>
#include <sys/ddi.h>
#include <sys/clock.h>
#include <sys/pte.h>
#include <sys/scb.h>
#include <sys/stack.h>
#include <sys/eeprom.h>
#include <sys/intreg.h>
#include <sys/trap.h>
#include <sys/x_call.h>
#include <sys/spitregs.h>
#include <sys/membar.h>

#ifdef	IOC
#include <sys/iocache.h>
#endif /* IOC */

#include <vm/hat.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/seg_kp.h>
#include <sys/swap.h>
#include <sys/thread.h>
#include <sys/sysconf.h>
#include <sys/machparam.h>
#include <sys/vm_machparam.h>
#include <sys/iommu.h>
#include <sys/vtrace.h>
#include <sys/autoconf.h>
#include <sys/instance.h>
#include <sys/avintr.h>
#include <sys/mem.h>
#include <sys/machpcb.h>
#include <vm/hat_sfmmu.h>

#include <sys/prom_debug.h>
#ifdef	TRAPTRACE
#include <sys/traptrace.h>
u_longlong_t panic_tick;
#endif /* TRAPTRACE */

#ifdef MP
static void idle_other_cpus(void);
static void resume_other_cpus(void);
static void stop_other_cpus(void);
extern void idle_stop_xcall();
#endif /* MP */

#ifdef MP
extern void set_idle_cpu(int);
extern void unset_idle_cpu(int);
#endif /* MP */

int __cg92_used;	/* sastisfy C compiler reference for -xcg92 option */

extern int pf_is_memory(u_int);
void	power_down(char *);
void	reboot(char *);
static void complete_panic(void);
static char *convert_boot_device_name(char *);

/*
 * do not use msgbuf until it is fixed.
 */
int msgbufinit = 1;	/* message buffer has been initialized, ok to printf */

/*
 * Miscellaneous hardware feature override flags
 */
int use_ic = 1;
int use_dc = 1;
int use_ec = 1;

/*
 * These variables are set by module specific config routines.
 * They are only set by modules which will use physical cache page coloring
 * and/or virtual cache page coloring.
 */
int do_pg_coloring = 0;
int do_virtual_coloring = 0;

/*
 * These variables can be conveniently patched at kernel load time to
 * prevent do_pg_coloring or do_virtual_coloring from being enabled by
 * module specific config routines.
 */
int use_page_coloring = 1;
int use_virtual_coloring = 1;

/* #ifndef	MP */
int Cpudelay = 0;		/* delay loop count/usec */
/* #endif MP */

extern int cache;		/* address cache type (none = 0) */

extern	u_int	shm_alignment;	/* VAC address consistency modulus */

/*
 * Boot hands us the romp.
 */
union sunromvec *romp = (union sunromvec *)0;
struct debugvec *dvec = (struct debugvec *)0;

int maxphys = MMU_PAGESIZE * 16;	/* 128k */
int klustsize = MMU_PAGESIZE * 16;	/* 128k */

int vac = 0;		/* virtual address cache type (none == 0) */

#ifdef	IOC
int ioc = 1;			/* I/O cache type (none == 0) */
#endif /* IOC */

#ifdef	MP
int	procset = 1;
#endif /* MP */

int	pokefault = 0;

/* Flag for testing directed interrupts */
int	test_dirint = 0;

#ifdef	MP

/*
 * MP configurations may reserve additional interrupt request entries.
 * intr_add_{div,max} can be modified to tune memory usage.
 */

int	intr_add_div = 1;			/* 1=worst case memory usage */
int	intr_add_max = (NCPU * INTR_POOL_SIZE);	/* (32*512)=16k bytes max */

/* intr_add_{pools,head,tail} calculated based on intr_add_{div,max} */

int	intr_add_pools = 0;			/* additional pools per cpu */
struct intr_req	*intr_add_head = (struct intr_req *)NULL;
#ifdef	DEBUG
struct intr_req	*intr_add_tail = (struct intr_req *)NULL;
#endif	/* DEBUG */
#endif	/* MP */

kthread_id_t clock_thread;	/* clock interrupt thread pointer */
kmutex_t memseg_lock;		/* lock for searching memory segments */

static int nopanicdebug = 0; /* 0 = call debugger/obp on panic, 1 = reboot */

char	*panicstr = 0;	/* contains argument to the first call to panic */
va_list panicargs;

/*
 * This is the state of the world before the file systems are sync'd and
 * system state is dumped.  Should be put in some panic data structure.
 */
label_t		panic_regs;		/* adb looks at these */
kthread_id_t	panic_thread;
kthread_id_t	panic_clock_thread;
struct cpu	panic_cpu;
kmutex_t	panic_lock;

kthread_id_t	panic_thread2;


#ifdef	DEBUG
unsigned int	panic_debug;
#define	PANIC_DEBUG_LOG(log)		panic_debug |= (log);
#define	PANIC_DEBUG_NEW_CLOCK_THREAD	0x0100
#define	PANIC_DEBUG_NO_INTR_THREAD	0x0200
#define	PANIC_DEBUG_NO_CLOCK_THREAD	0x0400
#define	PANIC_DEBUG_NEW_INTR_STACK	0x0800
#define	PANIC_DEBUG_SYNC_FAILED		0x1000
#define	PANIC_DEBUG_DUMP_FAILED		0x2000
#else
#define	PANIC_DEBUG_LOG(log)
#endif

/*
 * Panic is called on unresolvable fatal errors. It prints "panic: mesg",
 * attempts to sync the file systems, dumps core and finally reboots the
 * system.
 */
void
panic(char *fmt, ...)
{
	va_list adx;

	va_start(adx, fmt);
	do_panic(fmt, adx);
	va_end(adx);
}

/*
 * The panic functionality has been separated into 2 parts to avoid
 * unecessary double panics. For example, in trap, we often took a
 * recursive mutex panic because we needed a lock that was needed to
 * print out the register windows. see die() in trap.c.
 */
void
do_panic(char *fmt, va_list adx)
{
	int rv, i = 0;
	short maxtl;

#ifdef TRAPTRACE
	if (!panic_tick)
		panic_tick = gettick();
	TRAPTRACE_FREEZE;
#endif

	rv = setup_panic(fmt, adx);

	printf("panic[cpu%d]/thread=0x%x: ", CPU->cpu_id, curthread);
	prf(fmt, adx, (vnode_t *)NULL, PRW_CONS | PRW_BUF);
	if (ptl1_panic_cpu)
		printf(", trap reason 0x%x", ptl1_panic_tr);
	printf("\n");
#ifdef DEBUG
	if (rv != 0)
		printf("panic_debug = 0x%x\n", panic_debug);
#endif
	if (ptl1_panic_cpu) {
		maxtl = ptl1_dat[i].d.ptl1_tl;
		for (i = maxtl - 1; i >= 0; i--) {
			printf("TL=0x%x TT=0x%x TICK=0x%llx\n",
				ptl1_dat[i].d.ptl1_tl, ptl1_dat[i].d.ptl1_tt,
				ptl1_dat[i].d.ptl1_tick);
			printf("\tTPC=0x%llx TnPC=0x%llx TSTATE=0x%llx\n",
				ptl1_dat[i].d.ptl1_tpc, ptl1_dat[i].d.ptl1_tnpc,
				ptl1_dat[i].d.ptl1_tstate);
		}
	}

	if (((boothowto & RB_DEBUG) || obpdebug) && (nopanicdebug == 0))
		debug_enter((char *)NULL);

	if (rv == 0)
		complete_panic();

	mdboot(A_REBOOT, AD_BOOT, NULL);
}

int
setup_panic(char *fmt, va_list adx)
{
	kthread_id_t tp;
#ifdef	MP
	int pstate_save;
	extern int disable_vec_intr();
	extern void enable_vec_intr();
	extern void clkswitch(struct cpu *);
	extern int ncpus;
#endif	/* MP */

#ifndef LOCKNEST
	/*
	 * Allow only threads on panicking cpu to blow through locks.
	 * Other cpus would be caught by panic_hook, or get stopped
	 * via stop_other_cpus() in this function.
	 */
	mutex_enter(&panic_lock);
	if (panicstr) {
		/*
		 * this is the case when we are called twice and we bypassed
		 * the above mutex lock via panic_hook(). flush/save this new
		 * panic thread state as much as possible; adb may not show
		 * correct trace for panic_thread2 if the memory involved
		 * here has been dumped already. We try anyway.
		 */
		panic_thread2 = curthread;
		flush_windows();
		return (0);
	}

	conslogging = 0;
	panic_cpu = *curthread->t_cpu;
	panic_thread = curthread;
	panicstr = fmt;
	panicargs = adx;

	/*
	 * Insure that we are neither preempted nor swapped out
	 */
	thread_lock(curthread);
	curthread->t_bound_cpu = CPU;
	curthread->t_schedflag |= TS_DONT_SWAP;
	curthread->t_preempt++;
	thread_unlock(curthread);

	/*
	 * Note that this can only be done once, the above panicstr check
	 * is already an effective block for recursive panics.
	 */
	error_disable();

	flush_windows();

#ifdef MP
	if (ncpus > 1) {
		/*
		 * force all the interrupts to go to the panic cpu.
		 * This routine is very crude, sunfire may want to be nicer.
		 * The reset_func's are supposed to change their mapping regs
		 * to send interrupts to the panic_cpu, if necessary.
		 */
		pstate_save = disable_vec_intr();

		intr_redist_all_cpus(INTR_CURRENT_CPU);

		if (clock_thread)
			clock_thread->t_bound_cpu = CPU;

		enable_vec_intr(pstate_save);

		stop_other_cpus();

		clkswitch(CPU);
	}
#endif /* MP */

	/*
	 * Pass this point if clockthread is runnable but not ON_PROC,
	 * it will then start taking interrupts on panic_cpu from now.
	 * Otherwise we create a new clock thread to run on panic_cpu.
	 */
	if (clock_thread &&
	    (clock_thread->t_state == TS_SLEEP || clock_thread->t_lock)) {
		if (CPU->cpu_intr_thread) {
			tp = CPU->cpu_intr_thread;
			CPU->cpu_intr_thread = tp->t_link;
			tp->t_pri = clock_thread->t_pri;
			tp->t_cpu = CPU;
			tp->t_bound_cpu = CPU;
			CPU->cpu_intr_stack = tp->t_stk -= SA(MINFRAME);
			panic_clock_thread = clock_thread;
			clock_thread = tp;
			PANIC_DEBUG_LOG(PANIC_DEBUG_NEW_CLOCK_THREAD);
		} else {
			PANIC_DEBUG_LOG(PANIC_DEBUG_NO_INTR_THREAD);
			return (-1);
		}
	}

	if (clock_thread == NULL) {
		PANIC_DEBUG_LOG(PANIC_DEBUG_NO_CLOCK_THREAD);
		return (-1);
	}

	/*
	 * If current_thread (i.e panic_thread) is running on interrupt
	 * stack, allocate a new interrupt stack for handling future
	 * interrupts (coming at level > LOCK_LEVEL).
	 */
	if (CPU->cpu_on_intr) {
		if (CPU->cpu_intr_thread) {
			tp = CPU->cpu_intr_thread;
			CPU->cpu_intr_thread = tp->t_link;
			CPU->cpu_intr_stack = tp->t_stk -= SA(MINFRAME);
			CPU->cpu_on_intr = 0;
			PANIC_DEBUG_LOG(PANIC_DEBUG_NEW_INTR_STACK);
		} else {
			PANIC_DEBUG_LOG(PANIC_DEBUG_NO_INTR_THREAD);
			return (-1);
		}
	}

	if (curthread->t_cred == NULL)
		curthread->t_cred = kcred;

	return (0);
#endif /* LOCKNEST */
}

static int in_sync = 0;
static int in_dumpsys = 0;

static void
complete_panic(void)
{
	u_int ipl, old_ipl;
	extern int sync_timeout;

	if (curthread->t_cred == NULL)
		curthread->t_cred = kcred;

	/*
	 * save the thread stack pointer for dump. We do this here so
	 * that panic_regs reflects the state before memory is dumped.
	 */
	(void) setjmp(&curthread->t_pcb);
	if (curthread == panic_thread)
		panic_regs = panic_thread->t_pcb;

	if (!in_sync) {
		in_sync = 1;

		/*
		 * If the panic cpu comes in at an IPL that's masking clock
		 * interrupts (e.g. a sync from the PROM) drop it to
		 * CLOCK_LEVEL - 1, so that sync timeouts work. If we are
		 * above driver interrupts, spl0()/splx() in panic_hook()
		 * will allow them to get through.
		 */
		old_ipl = ipl = getpil();
		PANIC_DEBUG_LOG(ipl);
		if (old_ipl >= CLOCK_LEVEL) {
			ipl = CLOCK_LEVEL - 1;
			(void) splx(ipltospl(ipl));
		}
		vfs_syncall();
		in_sync = 2;

		/* Switch back the IPL if we changed it */
		if (old_ipl != ipl) {
			(void) splx(ipltospl(old_ipl));
		}
	} else {
		/*
		 * We paniced while syncing. Reset sync_timeout, otherwise
		 * we will get another panic when the timeout expires.
		 * We proceed to dump core even after this sync failure.
		 */
		PANIC_DEBUG_LOG(PANIC_DEBUG_SYNC_FAILED);
		sync_timeout = 0;
	}

	/*
	 * We've made it through most of the panic shutdown.  Go
	 * ahead and disable the watchdog timer now.
	 */
	if (watchdog_activated)
		tod_suspendwatchdog();

	if (!in_dumpsys) {
		in_dumpsys = 1;

		/*
		 * Don't take anymore i/o interrupt. This protects us from
		 * taking another interrupt introduced panic. We let clock
		 * run so that timeout would work and allow L1-A/break.
		 */
		old_ipl = ipl = getpil();
		if (ipl != (CLOCK_LEVEL - 1)) {
			ipl = CLOCK_LEVEL - 1;
			(void) splx(ipltospl(ipl));
		}
		dumpsys();
		in_dumpsys = 2;

		if (ipl != old_ipl) {
			(void) splx(ipltospl(old_ipl));
		}
	} else {
		/*
		 * We paniced while dumping.
		 */
		PANIC_DEBUG_LOG(PANIC_DEBUG_DUMP_FAILED);
		printf("Dump Aborted.\n");
		prom_enter_mon();
	}
}


/*
 * panic_hook gets called when panicstr is set. Allow interrupt threads
 * to run, but don't allow them to nest. Allow non interrupt threads to
 * continue only in the case of panic cpu. This function is effective in
 * blocking other cpus until the panic_cpu invokes stop_other_cpus().
 */
void
panic_hook(void)
{
	int s;

	if (CPU->cpu_id != panic_cpu.cpu_id && !(CPU->cpu_on_intr)) {
		/*
		 * we stop here if we are not on panic cpu and if we are
		 * not servicing any high level interrupt. XXX we could stop
		 * even in the case of high level interrupt, but not sure
		 * if that would affect something critical.
		 */
		for (;;);
	}

	if (curthread != panic_thread)
		return;

	if (!in_dumpsys) {
		/*
		 * don't allow the interrupts to nest. We don't worry
		 * about this while dumping as it is done in polled mode.
		 */
		s = spl0();
		(void) splx(s);
	}
}
/*
 * If bootstring contains a device path, we need to convert to a format
 * the prom will understand and return this new bootstring in buf.
 */
static char *
convert_boot_device_name(char *cur_path) {
	char *ret = cur_path;
	char *ptr, *buf;

	if ((buf = (char *)kmem_alloc(MAXPATHLEN, KM_NOSLEEP)) == NULL)
		return (cur_path);
	if ((ptr = strchr(cur_path, ' ')) != NULL)
		*ptr = '\0';
	if (i_devname_to_promname(cur_path, buf) == 0) {
		/* the conversion worked */
		if (ptr != NULL) {
			*ptr = ' ';
			(void) strcat(buf, ptr);
			ptr = NULL;
		}
		ret = buf;
	} else if (ptr != NULL) {	 /* the conversion failed */
		kmem_free((void *)buf, MAXPATHLEN);
		*ptr = ' ';
		ret = cur_path;
	}
	return (ret);
}

/*
 * Machine dependent code to reboot.
 * "mdep" is interpreted as a character pointer; if non-null, it is a pointer
 * to a string to be used as the argument string when rebooting.
 */
/*ARGSUSED*/
void
mdboot(int cmd, int fcn, char *bootstr)
{
	int s;

	/*
	 * XXX - rconsvp is set to NULL to ensure that output messages
	 * are sent to the underlying "hardware" device using the
	 * monitor's printf routine since we are in the process of
	 * either rebooting or halting the machine.
	 */
	rconsvp = NULL;

	s = spl6();
	reset_leaves(); 		/* try and reset leaf devices */
	if (fcn == AD_HALT) {
		halt((char *)NULL);
		fcn &= ~RB_HALT;
		/* MAYBE REACHED */
	} else if (fcn == AD_POWEROFF) {
		power_down((char *)NULL);
		/* NOTREACHED */
	} else {
		if (bootstr == NULL) {
			switch (fcn) {

			case AD_BOOT:
				bootstr = "";
				break;

			case AD_IBOOT:
				bootstr = "-a";
				break;

			case AD_SBOOT:
				bootstr = "-s";
				break;

			case AD_SIBOOT:
				bootstr = "-sa";
				break;
			default:
				cmn_err(CE_WARN,
				    "mdboot: invalid function %d", fcn);
				bootstr = "";
				break;
			}
		} else if (*bootstr == '/') {
			/* take care of any devfs->prom device name mappings */
			bootstr = convert_boot_device_name(bootstr);
		}
		reboot(bootstr);
		/*NOTREACHED*/
	}
	(void) splx(s);
}

/*
 * Machine-dependent portion of dump-checking;
 * verify that a physical address is valid.
 */
int
dump_checksetbit_machdep(u_longlong_t addr)
{
	struct memlist	*pmem;

	for (pmem = phys_install; pmem; pmem = pmem->next) {
		if (pmem->address <= addr &&
		    addr < (pmem->address + pmem->size))
			return (1);
	}
	return (0);
}

/*
 * Temporary address we use to map things when dumping them.
 */
caddr_t	dump_addr;
caddr_t	cur_dump_addr;

static	int	npgs = 0;
static	int	dbn = 0;

/*
 * Dump a page frame. Only setup the mapping until we have enough pages
 * to dump. "DUMPPAGES" increases the dump rate by more than 10 times!
 */
int
dump_page(struct vnode *vp, int pg, int bn)
{
	register caddr_t addr;
	register int err = 0;

	/*
	 * First setup the mapping for this page.
	 */
	addr = cur_dump_addr;
	hat_devload(kas.a_hat, addr, MMU_PAGESIZE, pg, PROT_READ,
	    HAT_LOAD_NOCONSIST);
	cur_dump_addr += MMU_PAGESIZE;

	if (npgs == 0) {
		/*
		 * Remember were we have to start writing on the disk.
		 */
		dbn = bn;
	}
	npgs++;
	if (npgs == DUMPPAGES) {

		/*
		 * Write to the disk only if we have enough pages to flush.
		 * The dumpsys_doit() procedure will call dump_final() at the
		 * end where we will write out the last remaining set of pages
		 * to the disk
		 */

		addr = dump_addr;
		err = VOP_DUMP(vp, addr, dbn, ctod(npgs));

		/*
		 * Destroy the page mappings that we had setup.
		 */
		hat_unload(kas.a_hat, addr, MMU_PAGESIZE * npgs, HAT_UNLOAD);

		/*
		 * Restore the virtual address and pages that we have cached.
		 */

		npgs = 0;
		cur_dump_addr = dump_addr;
	}
	return (err);
}


int
dump_final(struct vnode *vp)
{
	int err;
	caddr_t addr = dump_addr;

	if (npgs == 0)
		return (0);	/* no left over pages cached by dump_page() */

	err = VOP_DUMP(vp, addr, dbn, ctod(npgs));
	hat_unload(kas.a_hat, addr, MMU_PAGESIZE * npgs, HAT_UNLOAD);
	npgs = 0;
	cur_dump_addr = dump_addr;
	return (err);
}

/*
 * XXX This is grotty, but for historical reasons, xxdump() routines
 * XXX (called by spec_dump) expect to be called with an object already
 * XXX in DVMA space, and break if it isn't.  This doesn't matter for
 * XXX nfs_dump, but to be general we do it for everyone.
 *
 * Dump an arbitrary kernel-space object.
 * We do this by mapping this object into DVMA space a page-worth's
 * of bytes at a time, since this object may not be on a page boundary
 * and may span multiple pages.  We must be careful because the object
 * (or trailing portion thereof) may not span a page boundary and the
 * next virtual address may map to i/o space, which could cause
 * heartache.
 * We assume that dvmasize is at least two pages.
 */
/* ARGSUSED */
int
dump_kaddr(struct vnode *vp, caddr_t kaddr, int bn, int count)
{
	register int err = 0;
	register caddr_t addr, addr2;
	register int offset;
	int pf;

	offset = (u_int)kaddr & MMU_PAGEOFFSET;
	addr = dump_addr;
	addr2 = dump_addr + MMU_PAGESIZE;

	while (count > 0 && !err) {
		pf = va_to_pfn(kaddr);
		hat_devload(kas.a_hat, addr, MMU_PAGESIZE, pf, PROT_READ,
		    HAT_LOAD_NOCONSIST);
		/*
		 * XXX - why page 0 when non-obmem?
		 */
		pf = va_to_pfn(kaddr + MMU_PAGESIZE);
		if (pf == -1 || !pf_is_memory(pf))
			pf = 0;
		hat_devload(kas.a_hat, addr2, MMU_PAGESIZE, pf, PROT_READ,
		    HAT_LOAD_NOCONSIST);
		err = VOP_DUMP(vp, addr + offset, bn, ctod(1));
		bn += ctod(1);
		count -= ctod(1);
		kaddr += MMU_PAGESIZE;
		hat_unload(kas.a_hat, addr, 2 * MMU_PAGESIZE, HAT_UNLOAD);
	}
	return (err);
}

#ifdef	MP
static cpuset_t cpu_idle_set;
static kmutex_t cpu_idle_lock;

/*
 * Initialize the idlestop mutex
 */
void
idlestop_init(void)
{
	mutex_init(&cpu_idle_lock, "cpu idle/resume lock",
	    MUTEX_SPIN, (void *)ipltospl(PIL_15));
}


/*
 * flags to determine if the PROM routines
 * should be used to idle/resume/stop cpus
 */
int use_prom_stop = 0;
static int kern_idle[NCPU];		/* kernel's idle loop */
extern void debug_flush_windows();

static void
cpu_idle_self(void)
{
	u_int s;
	label_t save;

	s = spl8();
	debug_flush_windows();

	CPU->cpu_m.in_prom = 1;
	membar_stld();

	save = curthread->t_pcb;
	(void) setjmp(&curthread->t_pcb);

	kern_idle[CPU->cpu_id] = 1;
	while (kern_idle[CPU->cpu_id])
		/* SPIN */;

	CPU->cpu_m.in_prom = 0;
	membar_stld();

	curthread->t_pcb = save;
	(void) splx(s);
}

/*ARGSUSED*/
static void
cpu_stop_self(void)
{
	(void) spl8();
	debug_flush_windows();

	CPU->cpu_m.in_prom = 1;
	membar_stld();

	(void) setjmp(&curthread->t_pcb);

	if (use_prom_stop) {
		(void) prom_stop_self();
	} else {
		kern_idle[CPU->cpu_id] = 1;
		while (kern_idle[CPU->cpu_id])
			/* SPIN */;
	}
	/* shouldn't have gotten here */
	if (!panicstr) {
		cmn_err(CE_PANIC,
			"cpu_stop_self: return from prom_stop_self");
	} else {
		cmn_err(CE_WARN,
			"cpu_stop_self: return from prom_stop_self");
		/*CONSTCOND*/
		while (1)
			/* SPIN */;
	}
}

static void
idle_other_cpus(void)
{
	int i, cpuid, ntries;
	int failed = 0;

	if (ncpus == 1)
		return;

	mutex_enter(&cpu_idle_lock);

	cpuid = CPU->cpu_id;
	ASSERT(cpuid < NCPU);

	cpu_idle_set = cpu_ready_set;
	CPUSET_DEL(cpu_idle_set, cpuid);

	if (CPUSET_ISNULL(cpu_idle_set))
		return;

	xt_some(cpu_idle_set, (u_int)idle_stop_xcall,
	    (u_int)cpu_idle_self, NULL, NULL, NULL);

	for (i = 0; i < NCPU; i++) {
		if (!CPU_IN_SET(cpu_idle_set, i))
			continue;

		ntries = 0x10000;
		while (!cpu[i]->cpu_m.in_prom && ntries) {
			DELAY(50);
			ntries--;
		}

		/*
		 * A cpu failing to idle is an error condition, since
		 * we can't be sure anymore of its state.
		 */
		if (!cpu[i]->cpu_m.in_prom) {
			cmn_err(CE_WARN, "cpuid 0x%x failed to idle", i);
			failed++;
		}
	}

	if (failed) {
		mutex_exit(&cpu_idle_lock);
		cmn_err(CE_PANIC, "idle_other_cpus: not all cpus idled");
	}
}

static void
resume_other_cpus(void)
{
	int i, ntries;
	int cpuid = CPU->cpu_id;
	boolean_t failed = B_FALSE;

	if (ncpus == 1)
		return;

	ASSERT(cpuid < NCPU);
	ASSERT(MUTEX_HELD(&cpu_idle_lock));

	for (i = 0; i < NCPU; i++) {
		if (!CPU_IN_SET(cpu_idle_set, i))
			continue;

		kern_idle[i] = 0;
		membar_stld();
	}

	for (i = 0; i < NCPU; i++) {
		if (!CPU_IN_SET(cpu_idle_set, i))
			continue;

		ntries = 0x10000;
		while (cpu[i]->cpu_m.in_prom && ntries) {
			DELAY(50);
			ntries--;
		}

		/*
		 * A cpu failing to resume is an error condition, since
		 * intrs may have been directed there.
		 */
		if (cpu[i]->cpu_m.in_prom) {
			cmn_err(CE_WARN, "cpuid 0x%x failed to resume", i);
			continue;
		}
		CPUSET_DEL(cpu_idle_set, i);
	}

	failed = !CPUSET_ISNULL(cpu_idle_set);

	mutex_exit(&cpu_idle_lock);

	/*
	 * Non-zero if a cpu failed to resume
	 */
	if (failed)
		cmn_err(CE_PANIC, "resume_other_cpus: not all cpus resumed");

}


static void
stop_other_cpus(void)
{
	int i, cpuid, ntries;
	cpuset_t cpu_stop_set;
	boolean_t failed = B_FALSE;

	if (ncpus == 1)
		return;

	mutex_enter(&cpu_lock); /* for playing with cpu_flags */
	mutex_enter(&cpu_idle_lock);

	cpuid = CPU->cpu_id;
	ASSERT(cpuid < NCPU);

	cpu_stop_set = cpu_ready_set;
	CPUSET_DEL(cpu_stop_set, cpuid);

	if (CPUSET_ISNULL(cpu_stop_set)) {
		mutex_exit(&cpu_idle_lock);
		mutex_exit(&cpu_lock);
		return;
	}

	xt_some(cpu_stop_set, (u_int)idle_stop_xcall,
	    (u_int)cpu_stop_self, NULL, NULL, NULL);

	for (i = 0; i < NCPU; i++) {
		if (!CPU_IN_SET(cpu_stop_set, i))
			continue;

		/*
		 * Make sure the stopped cpu looks quiesced  and is
		 * prevented from receiving further xcalls by removing
		 * it from the cpu_ready_set.
		 */
		cpu[i]->cpu_flags &= ~(CPU_READY | CPU_EXISTS);
		CPUSET_DEL(cpu_ready_set, i);

		ntries = 0x10000;
		while (!cpu[i]->cpu_m.in_prom && ntries) {
			DELAY(50);
			ntries--;
		}

		/*
		 * A cpu failing to stop is an error condition, since
		 * we can't be sure anymore of its state.
		 */
		if (!cpu[i]->cpu_m.in_prom) {
			cmn_err(CE_WARN, "cpuid 0x%x failed to stop", i);
			continue;
		}
		CPUSET_DEL(cpu_stop_set, i);
	}

	failed = !CPUSET_ISNULL(cpu_stop_set);

	mutex_exit(&cpu_idle_lock);
	mutex_exit(&cpu_lock);

	/*
	 * Non-zero if a cpu failed to stop
	 */
	if (failed && !panicstr)
		cmn_err(CE_PANIC, "stop_other_cpus: not all cpus stopped");
}

#endif	/* MP */

void (*abort_seq_handler)(char *) = debug_enter;

/*
 *	Machine dependent abort sequence handling
 */
void
abort_sequence_enter(char *msg)
{
	if (abort_enable != 0)
		(*abort_seq_handler)(msg);
}

static int vx_entered;

/*
 * Enter debugger.  Called when the user types L1-A or break or whenever
 * code wants to enter the debugger and possibly resume later.
 * If the debugger isn't present, enter the PROM monitor.
 */
void
debug_enter(char *msg)
{
	int s;
	label_t debug_save;

	if (msg)
		prom_printf("%s\n", msg);

	s = splzs();

	debug_save = curthread->t_pcb;
	(void) setjmp(&curthread->t_pcb);

	if ((vx_entered == 0) && boothowto & RB_DEBUG) {
		{ func_t callit = (func_t)dvec; (*callit)(); }
	} else {
		prom_enter_mon();
	}
	curthread->t_pcb = debug_save;

	(void) splx(s);
}

/*
 * Halt the machine and return to the monitor
 */
void
halt(char *s)
{
	flush_windows();
	stop_other_cpus();		/* send stop signal to other CPUs */

	if (s)
		prom_printf("(%s) ", s);

	prom_exit_to_mon();
	/*NOTREACHED*/
}

/*
 * Halt the machine and then reboot with the device
 * and arguments specified in bootstr.
 */
void
reboot(char *bootstr)
{
	flush_windows();
	stop_other_cpus();		/* send stop signal to other CPUs */
	prom_printf("rebooting...\n");
	prom_reboot(bootstr);
	/*NOTREACHED*/
}

/*
 * Halt the machine and power off the system.
 */
void
power_down(char *s)
{
	flush_windows();
	stop_other_cpus();		/* send stop signal to other CPUs */

	if (s)
		prom_printf("(%s) ", s);

	prom_power_off();
	/*
	 * If here is reached, for some reason prom's power-off command failed.
	 * Prom should have already printed out error messages. Exit to
	 * firmware.
	 */
	prom_exit_to_mon();
	/*NOTREACHED*/
}


/*
 * Enter monitor.  Called via cross-call from stop_cpus().
 */
void
mp_halt(char *msg)
{
	if (msg)
		prom_printf("%s\n", msg);
	prom_exit_to_mon();
}

#ifdef	DEBUGGING_MEM

static int dset;

#define	STOP()			if (dset) prom_enter_mon()
#define	DPRINTF(str)		if (dset) prom_printf((str))
#define	DPRINTF1(str, a)	if (dset) prom_printf((str), (a))
#define	DPRINTF2(str, a, b)	if (dset) prom_printf((str), (a), (b))
#define	DPRINTF3(str, a, b, c)	if (dset) prom_printf((str), (a), (b), (c))
#else	DEBUGGING_MEM
#define	STOP()
#define	DPRINTF(str)
#define	DPRINTF1(str, a)
#define	DPRINTF2(str, a, b)
#define	DPRINTF3(str, a, b, c)
#endif	DEBUGGING_MEM


/*
 * Console put and get character routines.
 */
void
cnputs(char *buf, u_int bufsize, int device_in_use)
{
#ifdef MP
	if (device_in_use) {
		int s;
		/*
		 * This means that some other CPU may have a mapping
		 * to the device (framebuffer) that the OBP is about
		 * to render onto.  Some of the fancier framebuffers get upset
		 * about being accessed by more than one CPU - so stop
		 * the others in their tracks.
		 *
		 * This is a somewhat unfortunate 'hackaround' to the general
		 * problem of sharing a device between the OBP and userland.
		 *
		 * This should happen -very- rarely on a running system
		 * provided you have a console window redirecting console
		 * output when running your favourite window system ;-)
		 */
		s = splhi();
		idle_other_cpus();
		prom_writestr(buf, bufsize);
		resume_other_cpus();
		(void) splx(s);
	} else
#endif /* MP */
		prom_writestr(buf, bufsize);
}

void
cnputc(register int c, int device_in_use)
{
#ifdef MP
	int s;

	if (device_in_use) {
		s = splhi();
		idle_other_cpus();
	}
#endif /* MP */

	if (c == '\n')
		prom_putchar('\r');
	prom_putchar(c);

#ifdef MP
	if (device_in_use) {
		resume_other_cpus();
		(void) splx(s);
	}
#endif /* MP */
}

static int
cngetc(void)
{
	return ((int)prom_getchar());
}

/*
 * Get a character from the console.
 *
 * XXX	There's no need for both cngetc() and getchar() -- merge 'em
 */
static int
getchar(void)
{
	register c;

	c = cngetc();
	if (c == '\r')
		c = '\n';
	cnputc(c, 0);
	return (c);
}

/*
 * Get a line from the console.
 */
void
gets(char *cp)
{
	register char *lp;
	register c;

	lp = cp;
	for (;;) {
		c = getchar() & 0177;
		switch (c) {

		case '\n':
			*lp++ = '\0';
			return;

		case 0177:
			cnputc('\b', 0);
			/*FALLTHROUGH*/
		case '\b':
			cnputc(' ', 0);
			cnputc('\b', 0);
			/*FALLTHROUGH*/
		case '#':
			lp--;
			if (lp < cp)
				lp = cp;
			continue;

		case 'u'&037:
			lp = cp;
			cnputc('\n', 0);
			continue;

		case '@':	/* used in physical device names! */
		default:
			*lp++ = (char)c;
		}
	}
}


#if defined(DEBUG) || defined(lint)
/*
 * Called by assembler files that want to debug_enter
 */
void
call_debug_from_asm(void)
{
	int s;

	s = splzs();		/* protect against re-entrancy */
	(void) setjmp(&curthread->t_pcb); /* for kadb */
	debug_enter((char *)NULL);
	(void) splx(s);
}
#endif /* DEBUG || lint */

/*
 * Allocate threads and stacks for interrupt handling.
 */
#define	NINTR_THREADS	(LOCK_LEVEL-1)	/* number of interrupt threads */
#ifdef REDCHECK
#define	INTR_STACK_SIZE	(roundup(8192 + PAGESIZE, PAGESIZE))
#else
#define	INTR_STACK_SIZE	(roundup(8192, PAGESIZE))
#endif /* REDCHECK */

void
init_intr_threads(struct cpu *cp)
{
	int i;

	for (i = 0; i < NINTR_THREADS; i++)
		(void) thread_create_intr(cp);

	cp->cpu_intr_stack = (caddr_t)segkp_get(segkp, INTR_STACK_SIZE,
		KPD_HASREDZONE | KPD_NO_ANON | KPD_LOCKED) +
		INTR_STACK_SIZE - SA(MINFRAME);
}

void
init_clock_thread(void)
{
	kthread_id_t tp;

	/*
	 * Create clock interrupt thread.
	 * The state is initially TS_FREE.  Think of this thread on
	 * a private free list until it runs.
	 */
	tp = thread_create(NULL, INTR_STACK_SIZE, init_clock_thread, NULL, 0,
		&p0, TS_FREE, 0);
	tp->t_flag |= T_INTR_THREAD;	/* for clock()'s tick checking */
	clock_thread = tp;
}


/*
 * Called from dumpsys() to ensure the kvtopdata is in the dump.
 * Also does msgbuf since it's no longer in ktextseg.
 *
 * XXX  Not entirely convinced we need to do this specially ..
 */
void
dump_kvtopdata(void)
{
	caddr_t		i, j;

	i = (caddr_t)(((u_int)&kvtopdata) & MMU_PAGEMASK);
	for (j = (caddr_t)&kvtopdata + sizeof (kvtopdata); i < j;
	    i += MMU_PAGESIZE) {
		dump_addpage(va_to_pfn(i));
	}
	i = (caddr_t)&msgbuf;
	j = i + sizeof (msgbuf);
	while (i < j) {
		dump_addpage(va_to_pfn(i));
		i += MMU_PAGESIZE;
	}
}

/*
 * Callback handlers for "sync", "callback" and other callback code
 * from the IEEE 1275 boot firmware user interface.
 *
 * We are called with a pointer to a cell-sized argument array.
 * The service name (the first element of the argument array) is
 * the name of the callback being invoked.  When called, we are
 * running on the firmwares trap table as a trusted subroutine
 * of the firmware.
 *
 * We define entry points to allow callback handlers to be dynamically
 * added and removed, to support obpsym, which is a separate module
 * and can be dynamically loaded and unloaded and registers its
 * callback handlers dynamically. We define code here, to support
 * the "sync" callback.
 *
 * Note: The actual callback handler we register, is the assembly lang.
 * glue, callback_handler, which takes care of switching from a 64
 * bit stack and environment to a 32 bit stack and environment, and
 * back again, if the callback handler returns. callback_handler calls
 * vx_handler to process the callback.
 */

static void vx_sync(cell_t *);

#define	VX_CMD_MAX	4

static struct vx_cmd {
	char	*service;	/* Service name */
	int	take_tba;	/* If Non-zero we take over the tba */
	void	(*func)(cell_t *argument_array);
} vx_cmd[VX_CMD_MAX+1] = {
	{ "sync", 1, vx_sync }
};

#define	ENDADDR(a)	&a[sizeof (a) / sizeof (a[0])]
#define	vx_cmd_end	((struct vx_cmd *)(ENDADDR(vx_cmd)))

static kmutex_t vx_cmd_lock;	/* protect vx_cmd table */

void
init_vx_handler(void)
{
	/*
	 * initialize the lock protecting additions and deletions from
	 * the vx_cmd table.  At callback time we don't need to grab
	 * this lock.  Callback handlers do not need to modify the
	 * callback handler table.
	 */
	mutex_init(&vx_cmd_lock, "vx_cmd_lock", MUTEX_DEFAULT, NULL);
}

/*
 * Add a kernel callback handler to the kernel's list.
 * The table is static, so if you add a callback handler, increase
 * the value of VX_CMD_MAX. Find the first empty slot and use it.
 */
void
add_vx_handler(char *name, int flag, void (*func)(cell_t *))
{
	struct vx_cmd *vp;

	mutex_enter(&vx_cmd_lock);
	for (vp = vx_cmd; vp < vx_cmd_end; vp++) {
		if (vp->service == NULL) {
			vp->service = name;
			vp->take_tba = flag;
			vp->func = func;
			mutex_exit(&vx_cmd_lock);
			return;
		}
	}
	mutex_exit(&vx_cmd_lock);

#ifdef	DEBUG

	/*
	 * There must be enough entries to handle all callback entries.
	 * Increase VX_CMD_MAX if this happens. This shouldn't happen.
	 */
	cmn_err(CE_PANIC, "add_vx_handler <%s>", name);
	/* NOTREACHED */

#else	/* DEBUG */

	cmn_err(CE_WARN, "add_vx_handler: Can't add callback hander <%s>",
	    name);

#endif	/* DEBUG */

}

/*
 * Remove a vx_handler function -- find the name string in the table,
 * and clear it.
 */
void
remove_vx_handler(char *name)
{
	struct vx_cmd *vp;

	mutex_enter(&vx_cmd_lock);
	for (vp = vx_cmd; vp < vx_cmd_end; vp++) {
		if (vp->service == NULL)
			continue;
		if (strcmp(vp->service, name) != 0)
			continue;
		vp->service = 0;
		vp->take_tba = 0;
		vp->func = 0;
		mutex_exit(&vx_cmd_lock);
		return;
	}
	mutex_exit(&vx_cmd_lock);
	cmn_err(CE_WARN, "remove_vx_handler: <%s> not found", name);
}

/*ARGSUSED*/
static void
vx_sync(cell_t *argument_array)
{
	int i;
	extern int enable_interrupts(void);

	/*
	 * Avoid trying to talk to the other CPUs since they are
	 * already sitting in the prom and won't reply.
	 *
	 * This means that the vac_flushall() call in the panic
	 * code doesn't flush the caches on the other CPUs. This
	 * is not a problem since we know that idle_other_cpus()
	 * has already done so when we first got into the prom.
	 */
	for (i = 0; i < NCPU; i++) {
		if ((i != CPU->cpu_id) && CPU_XCALL_READY(i)) {
			cpu[i]->cpu_flags &= ~CPU_READY;
			cpu[i]->cpu_flags |= CPU_QUIESCED;
			CPUSET_DEL(cpu_ready_set, cpu[i]->cpu_id);
		}
	}
	(void) enable_interrupts();
	nopanicdebug = 1;	/* don't try to bkpt to kadb/prom */
	vx_entered++;		/* allow interruption of core dump */
	cmn_err(CE_PANIC, "zero");
	/*NOTREACHED*/
}

int
vx_handler(cell_t *argument_array)
{
	char *name;
	struct vx_cmd *vp;
	void *old_tba;
	extern struct scb trap_table;
	extern u_int tba_taken_over;
	extern void *set_tba(void *);

	name = p1275_cell2ptr(*argument_array);

	for (vp = vx_cmd; vp < vx_cmd_end; vp++) {
		if (vp->service == (char *)0)
			continue;
		if (strcmp(vp->service, name) != 0)
			continue;
		if (vp->take_tba != 0)  {
			reestablish_curthread();
			if (tba_taken_over != 0)
				old_tba = set_tba((void *)&trap_table);
		}
		vp->func(argument_array);
		if ((vp->take_tba != 0) && (tba_taken_over != 0))
			(void) set_tba(old_tba);
		return (0);	/* Service name was known */
	}

	return (-1);		/* Service name unknown */
}

/*
 * This only gets called when we crash.
 * No point in pushing it down into
 * assembly code.
 */
void
vac_flushall(void)
{
	PRM_INFO("vac_flushall: displacement flush all caches?!");
}

/*
 * This routine has now been implemented in assembly, but keep it
 * around anyway for a while for debugging purposes.
 */
#ifdef	MP
#if 0 /* XXX */
static u_int	last_idle_cpu;	/* set by set_idle_cpu() */

void
new_interrupt_target(void)
{
	u_int cur_cpu, new_target;
	u_int saved_cpu, idle_cpu;
	struct cpu *cpup;

	if (ncpus == 1)
		return;

	/*
	 * Note that save_cpu indicates the current CPU which
	 * the ITR is set to, since this routine is run when
	 * an interrupt occurrs.
	 */
	saved_cpu = cur_cpu = getprocessorid();
	idle_cpu = last_idle_cpu;

	/*
	 * The strategy is to try to make the most recently
	 * idle CPU be the target of interrupts, with the
	 * exception that the current CPU is not to be chosen
	 * as a target because this could result in race
	 * conditions.  In that case, we start searching for
	 * the next CPU that exists, and let it be the new
	 * target.
	 */
	if (idle_cpu != cur_cpu)
		new_target = idle_cpu;
	else {
		while ((new_target = (cur_cpu++) % NCPU) != cur_cpu) {
			cpup = cpu[new_target];
			if (cpup != NULL && (cpup->cpu_flags & CPU_ENABLE))
				break;
		}
	}

	/*
	 * Only need to set the ITR when it will be set to a
	 * different target.
	 */
	if (new_target != saved_cpu)
		set_interrupt_target(new_target);
}
#endif	/* XXX */
#endif MP

#ifdef  MP
/*
 * set_idle_cpu is called from idle() when a CPU becomes idle.
 */
/*ARGSUSED*/
void
set_idle_cpu(int cpun)
{}

/*
 * unset_idle_cpu is called from idle() when a CPU is no longer idle.
 */
/*ARGSUSED*/
void
unset_idle_cpu(int cpun)
{}
#endif /* MP */

/*
 * XXX These probably ought to live somewhere else - how about vm_machdep.c?
 * XXX They are called from mem.c
 */

/*
 * Convert page frame number to an OBMEM page frame number
 * (i.e. put in the type bits -- zero for this implementation)
 */
int
impl_obmem_pfnum(int pf)
{
	return (pf);
}


/*
 * Use physmax to determine the highest physical page of DRAM memory
 * It is assumed that any physical addresses above physmax is in IO space.
 * We don't bother checking the low end because we assume that memory space
 * begins at physical page frame 0.
 *
 * Return 1 if the page frame is onboard DRAM memory, else 0.
 * Returns 0 for nvram so it won't be cached.
 */
int
pf_is_memory(u_int pf)
{
	extern int physmax;

	/* We must be IO space */
	if (pf > (u_int) physmax)
		return (0);

	/* We must be memory space */
	return (1);
}

/*
 * Allocate cpus struct from the nucleus data memory
 * when the system has multiple processors.
 */
caddr_t
ndata_alloc_cpus(caddr_t cpus_alloc_base)
{
	int real_sz;

	extern struct cpu *cpus;
	extern int ncpunode;
	extern int niobus;
	extern int ecache_linesize;

	/*
	 * allocate cpus struct if we have more than one cpunodes
	 */
	if (ncpunode > 1) {
		real_sz = sizeof (struct cpu) * (ncpunode - 1);
		cpus = (struct cpu *)cpus_alloc_base;
		cpus_alloc_base += real_sz;
		PRM_DEBUG(cpus);
	}

	if (niobus > 1) {

		/*
		 * Allocate additional intr_req entries if we have more than
		 * one io bus.  The memory to allocate is calculated from four
		 * variables: niobus, ncpunode, intr_add_div, and intr_add_max.
		 * Allocate multiple of INTR_POOL_SIZE bytes (512).  Each cpu
		 * already reserves 512 byes in its machcpu structure, so the
		 * worst case is (512 * (niobus - 1) * ncpunode) add'l bytes.
		 *
		 * While niobus and ncpunode reflect the h/w, the following
		 * may be tuned (before boot):
		 *
		 *	intr_add_div -	divisor for scaling the number of
		 *			additional intr_req entries. use '1'
		 *			for worst case memory, '2' for half,
		 *			etc.
		 *
		 *   intr_add_max - upper limit on bytes of memory to reserve
		 */

		cpus_alloc_base = (caddr_t)roundup((u_int)cpus_alloc_base,
			ecache_linesize);

		real_sz = INTR_POOL_SIZE * (niobus - 1) * ncpunode;

		/* tune memory usage by applying divisor and maximum */

		real_sz = min(intr_add_max, real_sz / max(intr_add_div, 1));

		/* round down to multiple of (ncpunode * INTR_POOL_SIZE) */

		intr_add_pools = real_sz / (ncpunode * INTR_POOL_SIZE);
		real_sz = intr_add_pools * (ncpunode * INTR_POOL_SIZE);

		/* actually reserve the space */

		intr_add_head = (struct intr_req *)cpus_alloc_base;
		cpus_alloc_base += real_sz;
		PRM_DEBUG(intr_add_head);
#ifdef	DEBUG
		intr_add_tail = (struct intr_req *)cpus_alloc_base;
#endif	/* DEBUG */
	}

	return (cpus_alloc_base);
}

/*
 * Send a directed interrupt of specified interrupt number id to a cpu.
 */

void
send_dirint(cpuix, intr_id)
int	cpuix;			/* cpu to be interrupted */
int	intr_id;		/* interrupt number id */
{
	xt_one(cpuix, intr_id, 0, 0, 0, 0);
}


/*
 * Initialize kernel thread's stack.
 */
caddr_t
thread_stk_init(caddr_t stk)
{
	struct v9_fpu *fp;
	int align;

	/* allocate extra space for floating point state */
	stk -= SA(sizeof (struct v9_fpu) + GSR_SIZE);
	align = (u_int)stk & 0x3f;
	stk -= align;		/* force v9_fpu to be 16 byte aligned */
	fp = (struct v9_fpu *)stk;
	fp->fpu_fprs = 0;

	stk -= SA(MINFRAME);
	return (stk);
}

/*
 * Initialize lwp's kernel stack.
 * Note that now that the floating point register save area (struct v9_fpu)
 * has been broken out from machpcb and aligned on a 64 byte boundary so that
 * we can do block load/stores to/from it, there are a couple of potential
 * optimizations to save stack space. 1. The floating point register save
 * area could be aligned on a 16 byte boundary, and the floating point code
 * changed to (a) check the alignment and (b) use different save/restore
 * macros depending upon the alignment. 2. The lwp_stk_init code below
 * could be changed to calculate if less space would be wasted if machpcb
 * was first instead of second. However there is a REGOFF macro used in
 * locore, syscall_trap, machdep and mlsetup that assumes that the saved
 * register area is a fixed distance from the %sp, and would have to be
 * changed to a pointer or something...JJ said later.
 */
caddr_t
lwp_stk_init(klwp_t *lwp, caddr_t stk)
{
	struct machpcb *mpcb;
	struct machpcb *caller;
	struct v9_fpu *fp;
	u_int aln;

	stk -= SA(sizeof (struct v9_fpu) + GSR_SIZE);
	aln = (u_int)stk & 0x3F;
	stk -= aln;
	fp = (struct v9_fpu *)stk;
	stk -= SA(sizeof (struct machpcb));
	mpcb = (struct machpcb *)stk;
	bzero((caddr_t)mpcb, sizeof (struct machpcb));
	bzero((caddr_t)fp, sizeof (struct v9_fpu) + GSR_SIZE);
	lwp->lwp_regs = (void *)&mpcb->mpcb_regs;
	lwp->lwp_fpu = (void *)fp;
	mpcb->mpcb_fpu = fp;
	mpcb->mpcb_fpu->fpu_q = mpcb->mpcb_fpu_q;
	mpcb->mpcb_thread = lwp->lwp_thread;
	/*
	 * FIX_ALIGNMENT flag should be inherited from the
	 * caller.
	 */
	caller = (struct machpcb *)curthread->t_stk;
	if (caller->mpcb_flags & FIX_ALIGNMENT)
		mpcb->mpcb_flags = FIX_ALIGNMENT;
	return (stk);
}

/*
 * Copy regs from parent to child.
 */
void
lwp_forkregs(klwp_t *lwp, klwp_t *clwp)
{
	kthread_t *t;
	struct machpcb *mpcb = lwptompcb(clwp);
	struct v9_fpu *fp;
	volatile unsigned fprs;
	extern unsigned _fp_read_fprs();
	extern void fp_fork();
	extern void save_gsr(struct v9_fpu *);

	t = mpcb->mpcb_thread;
	/*
	 * remember fp since it will get erased during the bcopy.
	 */
	fp = mpcb->mpcb_fpu;
	/*
	 * Don't copy mpcb_frame since we hand-crafted it
	 * in thread_load().
	 */
	bcopy((caddr_t)lwp->lwp_regs, (caddr_t)clwp->lwp_regs,
		sizeof (struct machpcb) - REGOFF);
	mpcb->mpcb_thread = t;
	mpcb->mpcb_fpu = fp;
	fp->fpu_q = mpcb->mpcb_fpu_q;

	fprs = _fp_read_fprs();
	if ((fprs & FPRS_FEF) == FPRS_FEF) {
		save_gsr(clwp->lwp_fpu);
		fp_fork(lwp, clwp, fprs);
	}
}

/*
 * Free lwp fpu regs.
 */
void
lwp_freeregs(klwp_t *lwp)
{
	volatile u_int fprs;
	struct v9_fpu *fp = lwptofpu(lwp);
	struct machpcb *mpcb = lwptompcb(lwp);
	extern u_int _fp_read_fprs();
	extern void fp_free(struct v9_fpu *);

	bzero((caddr_t)mpcb->mpcb_traps, UTRAP_V8P_MAXTRAPS);
	fprs = _fp_read_fprs();
	if ((fprs & FPRS_FEF) == FPRS_FEF)
		fp_free(fp);
}

/*
 * fill in the extra register state area specified with the
 * specified lwp's platform-dependent non-floating-point extra
 * register state information
 */
/* ARGSUSED */
void
xregs_getgfiller(klwp_id_t lwp, caddr_t xrp)
{
	/* for sun4u nothing to do here, added for symmetry */
}

/*
 * fill in the extra register state area specified with the specified lwp's
 * platform-dependent floating-point extra register state information.
 * NOTE:  'lwp' might not correspond to 'curthread' since this is
 * called from code in /proc to get the registers of another lwp.
 */
void
xregs_getfpfiller(klwp_id_t lwp, caddr_t xrp)
{
	extern void save_gsr(struct v9_fpu *);
	extern void get_gsr(struct v9_fpu *, caddr_t);
	extern int fpu_exists;
	register prxregset_t *xregs = (prxregset_t *)xrp;
	register kfpu_t *fp = lwptofpu(lwp);

	/*
	 * fp_fksave() does not flush the GSR register into
	 * the lwp area, so do it now
	 */
	kpreempt_disable();
	if (fpu_exists && ttolwp(curthread) == lwp)
		save_gsr(fp);
	kpreempt_enable();
	get_gsr(fp, (caddr_t)xregs->pr_un.pr_v8p.pr_filler);
}

/*
 * set the specified lwp's platform-dependent non-floating-point
 * extra register state based on the specified input
 */
/* ARGSUSED */
void
xregs_setgfiller(klwp_id_t lwp, caddr_t xrp)
{
	/* for sun4u nothing to do here, added for symmetry */
}

/*
 * set the specified lwp's platform-dependent floating-point
 * extra register state based on the specified input
 */
void
xregs_setfpfiller(klwp_id_t lwp, caddr_t xrp)
{
	extern int fpu_exists;
	extern void set_gsr(caddr_t, struct v9_fpu *);
	extern void restore_gsr(struct v9_fpu *);
	register prxregset_t *xregs = (prxregset_t *)xrp;

	set_gsr((caddr_t)xregs->pr_un.pr_v8p.pr_filler, lwptofpu(lwp));

	if ((lwp == ttolwp(curthread)) && fpu_exists) {
		restore_gsr(lwptofpu(lwp));
	}
}


static struct upa_dma_pfns {
	u_int hipfn;
	u_int lopfn;
};

static struct upa_dma_pfns upa_dma_pfn_array[MAX_UPA];
static int upa_dma_pfn_ndx = 0;
/*
 * Certain UPA busses cannot accept dma transactions from any other source
 * except for memory due to livelock conditions in their hardware. (e.g. sbus
 * and PCI). These routines allow devices or busses on the UPA to register
 * a physical address block within it's own register space where DMA can be
 * performed.  Currently, the FFB is the only such device which supports
 * device DMA on the UPA.
 */
void
pf_set_dmacapable(u_int hipfn, u_int lopfn)
{
	int i = upa_dma_pfn_ndx;
	upa_dma_pfn_ndx++;

	upa_dma_pfn_array[i].hipfn = hipfn;
	upa_dma_pfn_array[i].lopfn = lopfn;
}

/*
 * This routine should only be called using a pfn that is known to reside
 * in IO space.  The function pf_is_memory() can be used to determine this.
 */
int
pf_is_dmacapable(u_int pfn)
{
	int i, j;
	extern int physmax;

	/* If the caller passed in a memory pfn, return true. */
	if (pfn <= (u_int) physmax)
		return (1);

	for (i = upa_dma_pfn_ndx, j = 0; j < i; j++)
		if (pfn <= upa_dma_pfn_array[j].hipfn &&
		    pfn >= upa_dma_pfn_array[j].lopfn)
			return (1);

	return (0);
}


static u_longlong_t *intr_map_reg[32];
/*
 * Routines to set/get UPA slave only device interrupt mapping registers.
 * set_intr_mapping_reg() is called by the UPA master to register the address
 * of an interrupt mapping register. The upa id is that of the master. If
 * this routine is called on behalf of a slave device, the framework
 * determines the upa id of the slave based on that supplied by the master.
 *
 * get_intr_mapping_reg() is called by the UPA nexus driver on behalf
 * of a child device to get and program the interrupt mapping register of
 * one of it's child nodes.  It uses the upa id of the child device to
 * index into a table of mapping registers.  If the routine is called on
 * behalf of a slave device and the mapping register has not been set,
 * the framework determines the devinfo node of the corresponding master
 * nexus which owns the mapping register of the slave and installs that
 * driver.  The device driver which owns the mapping register must call
 * set_intr_mapping_reg() in its attach routine to register the slaves
 * mapping register with the system.
 */
void
set_intr_mapping_reg(int upaid, u_longlong_t *addr, int slave)
{
	int affin_upaid;

	/* For UPA master devices, set the mapping reg addr and we're done */
	if (slave == 0) {
		intr_map_reg[upaid] = addr;
		return;
	}

	/*
	 * If we get here, we're adding an entry for a UPA slave only device.
	 * The UPA id of the device which has affinity with that requesting,
	 * will be the device with the same UPA id minus the slave number.
	 */
	affin_upaid = upaid - slave;

	/*
	 * Load the address of the mapping register in the correct slot
	 * for the slave device.
	 */
	intr_map_reg[affin_upaid] = addr;
}

u_longlong_t *
get_intr_mapping_reg(int upaid, int slave)
{
	int affin_upaid;
	dev_info_t *affin_dip;
	u_longlong_t *addr = intr_map_reg[upaid];

	/* If we're a UPA master, or we have a valid mapping register. */
	if (!slave || addr != (u_longlong_t *)0)
		return (addr);

	/*
	 * We only get here if we're a UPA slave only device whose interrupt
	 * mapping register has not been set.
	 * We need to try and install the nexus whose physical address
	 * space is where the slaves mapping register resides.  They
	 * should call set_intr_mapping_reg() in their xxattach() to register
	 * the mapping register with the system.
	 */

	/*
	 * We don't know if a single- or multi-interrupt proxy is fielding
	 * our UPA slave interrupt, we must check both cases.
	 * Start out by assuming the multi-interrupt case.
	 * We assume that single- and multi- interrupters are not
	 * overlapping in UPA portid space.
	 */

	affin_upaid = upaid | 3;

	/*
	 * We start looking for the multi-interrupter affinity node.
	 * We know it's ONLY a child of the root node since the root
	 * node defines UPA space.
	 */
	for (affin_dip = ddi_get_child(ddi_root_node()); affin_dip;
	    affin_dip = ddi_get_next_sibling(affin_dip))
		if (ddi_prop_get_int(DDI_DEV_T_ANY, affin_dip,
		    DDI_PROP_DONTPASS, "upa-portid", -1) == affin_upaid)
			break;

	if (affin_dip) {
		if (ddi_install_driver(ddi_get_name(affin_dip))
			== DDI_SUCCESS) {
				/* try again to get the mapping register. */
				addr = intr_map_reg[upaid];
		}
	}

	/*
	 * If we still don't have a mapping register try single -interrupter
	 * case.
	 */
	if (addr == (u_longlong_t *)0) {

		affin_upaid = upaid | 1;

		for (affin_dip = ddi_get_child(ddi_root_node()); affin_dip;
		    affin_dip = ddi_get_next_sibling(affin_dip))
			if (ddi_prop_get_int(DDI_DEV_T_ANY, affin_dip,
			    DDI_PROP_DONTPASS, "upa-portid", -1) == affin_upaid)
				break;

		if (affin_dip) {
			if (ddi_install_driver(ddi_get_name(affin_dip))
			    == DDI_SUCCESS) {
				/* try again to get the mapping register. */
				addr = intr_map_reg[upaid];
			}
		}
	}
	return (addr);
}

void
do_shutdown()
{
	register proc_t *initpp;

	/*
	 * If we're still booting and init(1) isn't set up yet, simply halt.
	 */
	mutex_enter(&pidlock);
	initpp = prfind(P_INITPID);
	mutex_exit(&pidlock);
	if (initpp == NULL) {
		extern void halt(char *);
		prom_power_off();
		halt("Power off the System");	/* just in case */
	}

	/*
	 * else, graceful shutdown with inittab and all getting involved
	 */
	psignal(initpp, SIGPWR);
}

/*
 * For CPR. When resuming, the PROM state is wiped out; in order for
 * kadb to work, the defer word that allows kadb's trap mechanism to
 * work must be downloaded to the PROM.
 */
void
kadb_promsync(int cmd, void *arg)
{
	/*
	    dv_scbsync is being overloaded to call into kadb_promsync.
	    This lets us support kadb in cpr without making any changes
	    to common code.
	*/
	func_t callit = (func_t)dvec->dv_scbsync;
	(*callit)(cmd, arg);
}
