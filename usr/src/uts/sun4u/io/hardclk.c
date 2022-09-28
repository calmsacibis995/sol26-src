/*
 * Copyright (c) 1990, 1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)hardclk.c	1.58	96/10/21 SMI"

#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/clock.h>
#include <sys/intreg.h>
#include <sys/x_call.h>
#include <sys/cpuvar.h>
#include <sys/promif.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <vm/as.h>
#include <vm/hat.h>
#include <sys/ivintr.h>
#include <sys/sysiosbus.h>
#include <sys/machsystm.h>
#include <sys/reboot.h>

extern intrfunc promclk();
extern intrfunc level10();
extern void add_ivintr();
extern void rem_ivintr();
extern void tickint_hndlr1();
extern u_int tickint_disabled();
extern u_int disable_vec_intr();
extern void enable_vec_intr();
extern void tickcmpr_set();
extern void tickcmpr_reset();
extern void enqueue_tickint_req();
extern void dequeue_tickint_req();
extern void flush_windows();

/*
 * Machine-dependent clock routines.
 */

#define	CLOCK_RES	1000		/* 1 microsec in nanosecs */

#define	PC_SAMPLE_INTERVAL 10000		/* microsecs */
/*
 * dependent on hz, which may be set in /etc/system
 */
#define	SYS_CLK_INTERVAL	(1000000/hz)

u_int	tickint_inum;
kmutex_t tickint_lock;
void tickint_clnt_add();
void tickint_clnt_sub();
void tickint_program();
void (*profile_handler)();

int cpu_clock_mhz = 0;
int	clock_res = CLOCK_RES;
struct tick_clk_info {
	u_int	clk_inum;
	u_char	clk_use;
	u_char	clk_cpuid;
	u_char	clk_level;
} tick_clk, switch_clk;
u_int	cpu_tick_freq = 0;
u_int	scaled_clock_mhz = 0;
u_int	nsec_scale = 0;
u_longlong_t	nsec_drift = 0;
char	clock_started = 0;

/*
 * Hardware watchdog parameters and knobs
 */
int	watchdog_enable = 0;		/* user knob */
int	watchdog_available = 0;		/* system has a watchdog */
int	watchdog_activated = 0;		/* the watchdog is armed */
u_char	watchdog_timeout_seconds = CLK_WATCHDOG_DEFAULT;

#ifdef DEBUG
int callpromclk = 0;
#endif DEBUG

extern int Cpudelay;	/* delay loop count/usec */
extern struct cpu cpu0;	/* the boot CPU structure */

/*
 * preset the delay constant for drv_usecwait(). This is done for early
 * use of the le or scsi drivers in the kernel. The default contant
 * might be too high early on. We can get a pretty good approximation
 * of this by setting it as:
 *
 * 	cpu_clock_mhz = (cpunode.clock_freq + 500000) / 1000000
 *	cpudelay = cpu_clock_mhz / 2;
 *	cpudelay -= cpudelay/20;
 *
 * The 5% decrease is for overhead in the drv_usecwait() loop code.
 * This code was ported from sun4d.
 */
void
setcpudelay(void)
{
	struct cpu_node *cpunode;
	u_longlong_t tmp = NANOSEC;

	/*
	 * get the clock freq from the cpunodes[]
	 */
	cpunode = &cpunodes[CPU->cpu_id];
	cpu_tick_freq = cpunode->clock_freq;

	/*
	 * the sequence of the calculation affects "nsec_scale"
	 */
	tmp = (tmp << NSEC_SHIFT) / cpu_tick_freq;
	nsec_scale = (u_int) tmp;

	/*
	 * scaled_clock_mhz is a more accurated (ie not rounded-off)
	 * version of cpu_clock_mhz that we used to program the tick
	 * compare register. Just in case cpu_tick_freq is like 142.5 Mhz
	 * instead of some whole number like 143
	 */

	scaled_clock_mhz = (cpu_tick_freq) / 1000;
	cpu_clock_mhz = (cpu_tick_freq + 500000) / 1000000;

	/*
	 *  On Spitfire, because of the pipelining the 2 instruction
	 * loop in drvusec_wait() is grouped together and therefore
	 * only takes 1 cycle instead of 2. Because of this Cpudelay
	 * should be adjusted accordingly.
	 */

	if (cpu_clock_mhz > 0) {
		Cpudelay = cpu_clock_mhz;
		Cpudelay = Cpudelay - 3;
	} else {
		prom_printf("WARNING: Cpu node has invalid "
			"'clock-frequency' property\n");
	}
}

/*
 * We don't share the trap table with the prom, so we don't need
 * to enable/disable it's clock.
 */
void
start_mon_clock(void)
{
}

void
stop_mon_clock(void)
{
}

/*
 * If (clock-started) then the kernel level10 handler has been installed,
 * and it will update the hrestime variables and call clock.
 *
 * a tick_clk_info, switch_clk, is created for setting level10 handler
 * to the panicking cpu other than the cpu0 (see clkswitch())
 */
void
clkstart()
{
	extern void tickint_clnt_add();
	extern void gen_clk_int();
	register intrfunc f = (intrfunc)level10;
	u_int pil = LEVEL10;

	tick_clk.clk_use = CLK_SYSTEM;
	tick_clk.clk_cpuid = (u_char)cpu0.cpu_id;
	tick_clk.clk_level = (u_short)pil;
	tick_clk.clk_inum = add_softintr(pil, f, (caddr_t)&tick_clk, NULL);
	tickint_clnt_add(gen_clk_int, SYS_CLK_INTERVAL);

	switch_clk.clk_use = CLK_SYSTEM;
	switch_clk.clk_level = (u_short)pil;
	switch_clk.clk_inum = add_softintr(pil, f, (caddr_t)&switch_clk, NULL);

	clock_started = 1;

	/*
	 * Now is a good time to activate the hardware watchdog (if
	 * one exists).  So, if the user enabled it and we have one then
	 * activate.
	 */
	if (watchdog_enable) {
		if (watchdog_available) {
			if (boothowto & RB_DEBUG) {
				cmn_err(CE_WARN, "Hardware watchdog disabled"
					" [debugger]");
			} else {
				tod_setwatchdog(watchdog_timeout_seconds);
				watchdog_activated = 1;
				cmn_err(CE_CONT, "Hardware watchdog enabled\n");
			}
		} else {
			cmn_err(CE_WARN, "Hardware watchdog unavailable");
		}
	}
}

void
gen_clk_int()
{
	/* post a level10 interrupt */

	extern void setsoftint(u_int);

	ASSERT(clock_started);

	/*
	 * Verify that clock interrupts are going to the boot CPU. At
	 * this time, the boot CPU is the only one that is guaranteed
	 * to never be shutdown while the kernel is running.
	 */
	if (cpu0.cpu_id != tick_clk.clk_cpuid)
		return;

	setsoftint(tick_clk.clk_inum);

}


/*
 * the panic cpu, if not cpu0, sets the kernel level10 handler and
 * call clock
 */
void
clkswitch(struct cpu *cpux)
{
	extern void tickint_clnt_add();
	extern void switch_clk_int();

	if (!clock_started)
		return;

	/* if the clock taker is the panicking cpu */
	if (tick_clk.clk_cpuid == (u_char)cpux->cpu_id)
		return;

	switch_clk.clk_cpuid = (u_char) cpux->cpu_id;
	tickint_clnt_add(switch_clk_int, SYS_CLK_INTERVAL);
}

void
switch_clk_int()
{
	/* post a level10 interrupt */

	extern void setsoftint(u_int);

	setsoftint(switch_clk.clk_inum);
}


/*
 * Stop all clocks at level n.
 */
void
clk_level_stop(u_short level)
{
#if	defined(lint)
	level = level;
#endif	/* lint */
}


int
enable_profiling_interrupt(dev_info_t *devi, void (*intr_handler)())
{
#if defined(lint)
	devi = devi;
#endif /* lint */

	profile_handler = intr_handler;
	return (0);

}


int
disable_profiling_interrupt(dev_info_t *devi)
{
#if defined(lint)
	devi = devi;
#endif /* lint */

	return (0);
}




/*
 * invoked from profile_init
 *
 *
 */
int
enable_profiling(int cpuid)
{
	if (profile_handler) {
		xc_one(cpuid, (u_int (*)())tickint_clnt_add,
			(u_int)profile_handler, PC_SAMPLE_INTERVAL);
	}
	return (0);
}


void
disable_profiling(int cpuid)
{
	if (profile_handler) {
		xc_one(cpuid, (u_int (*)())tickint_clnt_sub,
			(u_int)profile_handler, 0);
	}
}


void
clear_profiling_intr(int cpuid)
{
#if	defined(lint)
	cpuid = cpuid;
#endif	/* lint */
}

static int olbolt[NCPU];

/*
 * This function is called from the level14 clock handler to
 * make sure the level10 clock handler is still running. If
 * it isn't, we are probably deadlocked somewhere so enter
 * the debugger.
 *
 * We use CPU 0's level 14 clock for this purpose.
 *
 * While making a panic crash dump, interrupts are masked
 * off for a long time.  Wait at least a minute before
 * deciding the system is deadlocked.
 */

#define	DEADMAN_DELAY	30

void
deadman(void)
{
	long delta;


	delta = olbolt[CPU->cpu_id] - lbolt;

	if (delta < 0)				/* Normal */
		olbolt[CPU->cpu_id] = lbolt;
	else if (delta < DEADMAN_DELAY)		/* Count down */
		olbolt[CPU->cpu_id]++;
	else {
		/*
		 * Hang detected!  Save processor state and then try
		 * to enter debugger.  Delay allows other running cpus
		 * to get into their deadman() routines as well.
		 */
		flush_windows();

		DELAY(1000000);

		debug_enter("deadman");		/* We're dead */

		olbolt[CPU->cpu_id] = lbolt;	/* reset timer */
	}
}


/*
 * Write the specified time into the clock chip.
 * Must be called with tod_lock held.
 */
/* ARGSUSED */
void
tod_set(timestruc_t ts)
{
#ifndef	MPSAS
	todinfo_t tod = utc_to_tod(ts.tv_sec);

	ASSERT(MUTEX_HELD(&tod_lock));

	/*
	 * The eeprom (which also contains the tod clock) is normally
	 * marked ro; change it to rw temporarily to update todc.
	 * This must be done every time the todc is written since the
	 * prom changes the todc mapping back to ro when it changes
	 * nvram variables (e.g. the eeprom cmd).
	 */
	hat_setattr(kas.a_hat, (caddr_t)((u_long)CLOCK & PAGEMASK), PAGESIZE,
		PROT_WRITE);

	CLOCK->clk_ctrl |= CLK_CTRL_WRITE;	/* allow writes */
	CLOCK->clk_year		= BYTE_TO_BCD(tod.tod_year - YRBASE);
	CLOCK->clk_month	= BYTE_TO_BCD(tod.tod_month);
	CLOCK->clk_day		= BYTE_TO_BCD(tod.tod_day);
	CLOCK->clk_weekday	= BYTE_TO_BCD(tod.tod_dow);
	CLOCK->clk_hour		= BYTE_TO_BCD(tod.tod_hour);
	CLOCK->clk_min		= BYTE_TO_BCD(tod.tod_min);
	CLOCK->clk_sec		= BYTE_TO_BCD(tod.tod_sec);
	CLOCK->clk_ctrl &= ~CLK_CTRL_WRITE;	/* load values */

	/*
	 * Now write protect it, preserving the new modify/ref bits
	 */
	hat_clrattr(kas.a_hat, (caddr_t)((u_long)CLOCK & PAGEMASK), PAGESIZE,
		PROT_WRITE);
#endif
}

/*
 * Aparently the m48t59 doesn't quite do what the spec sheet says.
 * The spec says read the WRD to reset the timer but that doesn't work.
 * So, we need to _reload_ the timeout each time we want to reset the timer.
 * This data is protected by tod_lock.
 */
static u_char watchdog_bits = 0;

/*
 * Program the watchdog timer shadow register with the specified value.
 */
void
tod_setwatchdog(u_char timeoutval)
{
	mutex_enter(&tod_lock);
	watchdog_bits = CLK_WATCHDOG_BITS(timeoutval);
	mutex_exit(&tod_lock);
}

/*
 * Just zero out the WDR for the places that need to suspend clock
 * operations.  The next routine call to tod_get() will re-enable
 * the WDR.
 */
void
tod_suspendwatchdog(void)
{
#ifndef	MPSAS
	mutex_enter(&tod_lock);
	/*
	 * The eeprom (which also contains the tod clock) is normally
	 * marked ro; change it to rw temporarily to update todc.
	 * This must be done every time the todc is written since the
	 * prom changes the todc mapping back to ro when it changes
	 * nvram variables (e.g. the eeprom cmd).
	 */
	hat_setattr(kas.a_hat, (caddr_t)((u_long)CLOCK & PAGEMASK),
		PAGESIZE, PROT_WRITE);

	CLOCK->clk_watchdog = 0;

	/*
	 * Now write protect it, preserving the new modify/ref bits
	 */
	hat_clrattr(kas.a_hat, (caddr_t)((u_long)CLOCK & PAGEMASK),
		PAGESIZE, PROT_WRITE);
	mutex_exit(&tod_lock);
#endif /* MPSAS */
}

/*
 * Read the current time from the clock chip and convert to UNIX form.
 * Assumes that the year in the clock chip is valid.
 * Must be called with tod_lock held.
 */
timestruc_t
tod_get(void)
{
	timestruc_t ts;
#ifndef	MPSAS
	todinfo_t tod;

	ASSERT(MUTEX_HELD(&tod_lock));

	hat_setattr(kas.a_hat, (caddr_t)((u_long)CLOCK & PAGEMASK), PAGESIZE,
		PROT_WRITE);

	CLOCK->clk_ctrl |= CLK_CTRL_READ;
	tod.tod_year	= BCD_TO_BYTE(CLOCK->clk_year) + YRBASE;
	tod.tod_month	= BCD_TO_BYTE(CLOCK->clk_month & 0x1f);
	tod.tod_day	= BCD_TO_BYTE(CLOCK->clk_day & 0x3f);
	tod.tod_dow	= BCD_TO_BYTE(CLOCK->clk_weekday & 0x7);
	tod.tod_hour	= BCD_TO_BYTE(CLOCK->clk_hour & 0x3f);
	tod.tod_min	= BCD_TO_BYTE(CLOCK->clk_min & 0x7f);
	tod.tod_sec	= BCD_TO_BYTE(CLOCK->clk_sec & 0x7f);
	CLOCK->clk_ctrl &= ~CLK_CTRL_READ;

	/* re-write the WDR register to 'reset' the watchdog timer */
	CLOCK->clk_watchdog = watchdog_bits;

	hat_clrattr(kas.a_hat, (caddr_t)((u_long)CLOCK & PAGEMASK), PAGESIZE,
		PROT_WRITE);

	ts.tv_sec = tod_to_utc(tod);
	ts.tv_nsec = 0;
#else
	ts.tv_sec = 0;
	ts.tv_nsec = 0;
#endif
	return (ts);
}

/*
 * The following wrappers have been added so that locking
 * can be exported to platform-independent clock routines
 * (ie adjtime(), clock_setttime()), via a functional interface.
 */
int
hr_clock_lock()
{
	return (CLOCK_LOCK());
}

void
hr_clock_unlock(int s)
{
	CLOCK_UNLOCK(s);
}


/*
 * TICK_INT routines
 */


/*
 * THIS IS A CPU-SPECIFIC ROUTINE
 *
 * Add client to TICK_INT client array.  The proper service interval is
 * provided for by the skip value which is the ratio of service interval to
 * current TICK_INT interval.
 */
void
tickint_clnt_add(void (*handler)(), u_int intrvl)
{
	struct cpu *cp = CPU;
	struct tick_info *p;
	struct tick_info *end_p;
	struct tick_info *free_p;
	long long min_intrvl = 0;
	long long interval;
	int found_match = 0;
	int found_free = 0;


	mutex_enter(&tickint_lock);

	/* convert from usec to clock cycles */
	interval = (long long) (scaled_clock_mhz * intrvl) / 1000;

	p = (struct tick_info *)&cp->cpu_m.tick_clnt[0];
	end_p = (struct tick_info *)&cp->cpu_m.tick_clnt[TICK_CLNTS -1];

	/* Search for first free entry; disallow duplicate entry */
	for (; p <= end_p;  p++) {
		if (p->handler == handler) {
			found_match++;
			printf("WARNING: TICK_INT handler at ");
			printf("address %x already registerd\n", handler);
			break;
		} else if (!found_free) {
			if (!p->handler) {
				found_free++;
				free_p = p;
			}
		}
	}

	if (!found_match) {
		if (found_free) {
			/* check for new minimum tickint interval */
			min_intrvl = cp->cpu_m.tickint_intrvl;
			if (!min_intrvl)
				min_intrvl = interval;
			else
				min_intrvl = MIN(min_intrvl, interval);
			cp->cpu_m.tickint_intrvl = min_intrvl;

			/* update new entry and adjust all skip values  */
			p = (struct tick_info *)&cp->cpu_m.tick_clnt[0];
			for (; p <= end_p;  p++) {
				if (p == free_p) {
					p->handler = handler;
					p->interval = interval;
				}
				p->skip =  p->interval / min_intrvl;
			}

			tickint_program(min_intrvl);
		} else	{
			printf("WARNING: TICK_INT Client Limit Exceeded, ");
			printf("handler at address %x rejected\n", handler);
		}
	}

	mutex_exit(&tickint_lock);
}

/*
 * THIS IS A CPU-SPECIFIC ROUTINE
 *
 * Remove handler from TICK_INT client array.  Adjust TICK_INT interval
 * and client skip values.
 */
void
tickint_clnt_sub(void (*handler)())
{
	struct cpu *cp = CPU;
	struct tick_info *p;
	struct tick_info *end_p;
	long long min_intrvl = 0;
	int clnt_found = 0;

	mutex_enter(&tickint_lock);

	p = (struct tick_info *)&cp->cpu_m.tick_clnt[0];
	end_p = (struct tick_info *)&cp->cpu_m.tick_clnt[TICK_CLNTS -1];

	/* scan array for match */
	for (; p <= end_p;  p++) {
		if (p->handler) {
			if (p->handler == handler) {
				clnt_found++;
				p->handler = 0;
				p->interval = 0;
				p->skip = 0;
				break;
			}
		}
	}

	if (clnt_found) {
		/* determine new tickint interval */
		p = (struct tick_info *)&cp->cpu_m.tick_clnt[0];
		for (; p <= end_p;  p++) {
			if (p->handler) {
				if (!min_intrvl)
					min_intrvl = p->interval;
				else
					min_intrvl = MIN(min_intrvl,
								p->interval);
			}
		}
		cp->cpu_m.tickint_intrvl = min_intrvl;

		/* update the skip value in all entries */
		p = (struct tick_info *)&cp->cpu_m.tick_clnt[0];
		for (; p <= end_p;  p++) {
			if (p->handler)
				p->skip = p->interval / min_intrvl;
		}

		tickint_program(min_intrvl);
	}

	mutex_exit(&tickint_lock);
}



/*
 * Called from tickint_hndlr1.  All TICK_INT clients are processed.
 * When skip count becomes zero, the client's handler is called.
 *
 */
void
tickint_hndlr2()
{
	struct cpu *cp = CPU;
	struct tick_info *p;
	struct tick_info *end_p;


	p = (struct tick_info *)&cp->cpu_m.tick_clnt[0];
	end_p = (struct tick_info *)&cp->cpu_m.tick_clnt[TICK_CLNTS -1];

	/* call valid client handlers */
	for (; p <= end_p; p++) {
		if (p->handler && (--p->skip == 0)) {
			p->skip = p->interval / cp->cpu_m.tickint_intrvl;
			(*p->handler)();
		}
	}


}



/*
 * The TICK_Compare register is programmed to interrupt after an interval
 * of tick_intrvl (here in units of clock cycles).  A zero interval
 * disables the interrupt.  Before programming the interval, an interrupt
 * service request structure is enqueued on the PIL_14 queue.
 */
void
tickint_program(long long tick_intrvl)
{
	int	pstate_save;


	pstate_save = disable_vec_intr();
	if (tick_intrvl) {
		if (tickint_disabled())
			enqueue_tickint_req();
		tickcmpr_set(tick_intrvl);
	} else {
		tickcmpr_set(tick_intrvl);
		dequeue_tickint_req();
	}
	enable_vec_intr(pstate_save);

}


void
tickint_init()
{
	/* insure that INT_DIS bit is set in TICK_Compare register */
	tickcmpr_reset();

	tickint_inum = add_softintr(PIL_14, tickint_hndlr1, 0, 0);

	mutex_init(&tickint_lock, "tickint_lock", MUTEX_SPIN_DEFAULT,
	    (void *)PIL_14);
}
