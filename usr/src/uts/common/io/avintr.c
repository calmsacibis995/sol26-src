/*
 * Copyright (c) 1992-1996, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)avintr.c	1.31	96/08/15 SMI"

/*
 * Autovectored Interrupt Configuration and Deconfiguration
 */

#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/trap.h>
#include <sys/t_lock.h>
#include <sys/avintr.h>
#include <sys/kmem.h>
#include <sys/machlock.h>
#include <sys/systm.h>
#include <sys/machsystm.h>
#include <sys/sunddi.h>
#include <sys/x_call.h>
#include <sys/cpuvar.h>
#include <sys/smp_impldefs.h>

static int insert_av(void *intr_id, struct av_head *vectp, avfunc f,
	caddr_t arg, kmutex_t *mutex_p, int pri_level, int vect);
static void remove_av(void *intr_id, struct av_head *vectp, avfunc f,
	int pri_level, int vect);

/*
 * Arrange for a driver to be called when a particular
 * auto-vectored interrupt occurs.
 * NOTE: if a device can generate interrupts on more than
 * one level, or if a driver services devices that interrupt
 * on more than one level, then the driver should install
 * itself on each of those levels.
 */
static char *badsoft =
	"add_avintr: bad soft interrupt level %d for driver '%s'\n";
static char *multilevel =
	"!IRQ%d is being shared by drivers with different interrupt levels.\n\
	This may result in reduced system performance.";
static char *multilevel2 =
	"Can not register interrupt for '%s' device at IPL %d because it\n\
	conflicts with another device using the same vector %d with an IPL\n\
	of %d. Reconfigure the conflicting devices to use different vectors.";

#define	MAX_VECT	256
#define	MAX_TRAP	31	/* All idt entries above this assumed intpts */
struct autovec *nmivect = NULL;
struct av_head autovect[MAX_VECT];
struct av_head softvect[LOCK_LEVEL];
struct softint softinfo;

int	maxvects = MAX_VECT;

void
set_pending(int pri)
{
	softinfo.st_pending |= 1 << pri;
}

/*ARGSUSED*/
u_int
nullintr(caddr_t arg)
{
	return (DDI_INTR_UNCLAIMED);
}

/*
 * register nmi interrupt routine. The first arg is used only to order
 * various nmi interrupt service routines in the chain. Higher lvls will
 * be called first
 */
int
add_nmintr(int lvl, avfunc nmintr, char *name, caddr_t arg)
{
	register struct autovec  *mem;
	register struct autovec *p, *prev = NULL;

	if (nmintr == NULL) {
		printf("Attempt to add null vect for %s on nmi\n", name);
		return (0);

	}

	mem = kmem_zalloc(sizeof (struct autovec), KM_SLEEP);
	mem->av_vector = nmintr;
	mem->av_intarg = arg;
	mem->av_mutex = NULL;
	mem->av_intr_id = NULL;
	mem->av_prilevel = lvl;
	mem->av_link = NULL;

	mutex_enter(&av_lock);

	if (!nmivect) {
		nmivect = mem;
		mutex_exit(&av_lock);
		return (1);
	}
	/* find where it goes in list */
	for (p = nmivect; p != NULL; p = p->av_link) {
		if (p->av_vector == nmintr && p->av_intarg == arg) {
			/*
			 * already in list
			 * So? Somebody added the same interrupt twice.
			 */
			printf("Warning Driver already Registered\n");
			kmem_free(mem, sizeof (struct autovec));
			mutex_exit(&av_lock);
			return (0);
		}
		if (p->av_prilevel < lvl) {
			if (p == nmivect) {   /* it's at head of list */
				mem->av_link = p;
				nmivect = mem;
			} else {
				mem->av_link = p;
				prev->av_link = mem;
			}
			mutex_exit(&av_lock);
			return (1);
		}
		prev = p;

	}
	/* didn't find it, add it to the end */
	prev->av_link = mem;
	mutex_exit(&av_lock);
	return (1);

}

/*
 * register a hardware interrupt handler.
 */
int
add_avintr(void *intr_id, int lvl, avfunc xxintr, char *name, int vect,
	caddr_t arg, kmutex_t *mutex_p)
{
	register struct av_head *vecp = (struct av_head *)0;
	register avfunc f;
	int s;			/* save old spl value */

	if ((f = xxintr) == NULL) {
		printf("Attempt to add null vect for %s on vector %d\n",
			name, vect);
		return (0);

	}
	if (vect >= maxvects) {
		printf("%s IRQ %d is greater than allowed\n", name, vect);
		return (0);
	}

	vecp = &autovect[vect];
	if (vecp->avh_link) {
		if (((vecp->avh_hi_pri > LOCK_LEVEL) && (lvl < LOCK_LEVEL)) ||
		    ((vecp->avh_hi_pri < LOCK_LEVEL) && (lvl > LOCK_LEVEL))) {
			cmn_err(CE_WARN, multilevel2, name, lvl, vect,
				vecp->avh_hi_pri);
			return (0);
		}
		if ((vecp->avh_lo_pri != lvl) || (vecp->avh_hi_pri != lvl))
			cmn_err(CE_NOTE, multilevel, vect);
	}

	if (!insert_av(intr_id, vecp, f, arg, mutex_p, lvl, vect))
		return (0);
	s = splhi();
	/*
	 * do what ever machine specific things are necessary
	 * to set priority level (e.g. set picmasks)
	 */
	mutex_enter(&av_lock);
	(*addspl)(vect, lvl, vecp->avh_lo_pri, vecp->avh_hi_pri);
	mutex_exit(&av_lock);
	(void) splx(s);
	return (1);

}

/*
 * Register a software interrupt handler
 */
int
add_avsoftintr(void *intr_id, int lvl, avfunc xxintr, char *name,
	caddr_t arg, kmutex_t *mutex_p)
{
	if (slvltovect(lvl) != -1) {
		return (add_avintr(intr_id, lvl, xxintr, name, slvltovect(lvl),
			arg, mutex_p));
	}

	if (xxintr == NULL)
		return (0);

	if (lvl <= 0 || lvl >= LOCK_LEVEL) {
		printf(badsoft, lvl, name);
		return (0);
	}
	if (!insert_av(intr_id, &softvect[lvl], xxintr, arg,
	    mutex_p, lvl, 0)) {
		return (0);
	}
	return (1);
}

/* insert an interrupt vector into chain */
/*ARGSUSED6*/
static int
insert_av(void *intr_id, struct av_head *vectp, avfunc f, caddr_t arg,
	kmutex_t *mutex_p, int pri_level, int vect)
{
	/*
	 * Protect rewrites of the list
	 */
	struct autovec *p, *mem;


	mem = kmem_zalloc(sizeof (struct autovec), KM_SLEEP);
	mem->av_vector = f;
	mem->av_intarg = arg;
	mem->av_mutex = mutex_p;
	mem->av_intr_id = intr_id;
	mem->av_prilevel = pri_level;
	mem->av_link = NULL;

	mutex_enter(&av_lock);

	if (vectp->avh_link == NULL)	/* Nothing on list - put it at head */
	{
		vectp->avh_link = mem;
		vectp->avh_hi_pri = vectp->avh_lo_pri = (u_short)pri_level;

		mutex_exit(&av_lock);
		return (1);
	}

	/* find where it goes in list */
	for (p = vectp->avh_link; p != NULL; p = p->av_link) {
		if (p->av_vector == f && p->av_intarg == arg) {
			/*
			 * already in list
			 * So? Somebody added the same interrupt twice.
			 */
			printf("Warning Driver already Registered\n");
			kmem_free(mem, sizeof (struct autovec));
			mutex_exit(&av_lock);
			return (0);
		}
		if (p->av_vector == NULL) {	/* freed struct available */
			kmem_free(mem, sizeof (struct autovec));
			p->av_intarg = arg;
			p->av_mutex = mutex_p;
			p->av_intr_id = intr_id;
			p->av_prilevel = pri_level;
			if (pri_level > (int)vectp->avh_hi_pri) {
				vectp->avh_hi_pri = (u_short)pri_level;
			}
			if (pri_level < (int)vectp->avh_lo_pri) {
				vectp->avh_lo_pri = (u_short)pri_level;
			}
			p->av_vector = f;
			mutex_exit(&av_lock);
			return (1);
		}
	}
	/* insert new intpt at beginning of chain */
	mem->av_link = vectp->avh_link;
	vectp->avh_link = mem;
	if (pri_level > (int)vectp->avh_hi_pri) {
		vectp->avh_hi_pri = (u_short)pri_level;
	}
	if (pri_level < (int)vectp->avh_lo_pri) {
		vectp->avh_lo_pri = (u_short)pri_level;
	}
	mutex_exit(&av_lock);
	return (1);
}

/*
 * Remove a driver from the autovector list.
 *
 */
int
rem_avsoftintr(void *intr_id, int lvl, avfunc xxintr)
{
	register struct av_head *vecp = (struct av_head *)0;

	if (xxintr == NULL)
		return (0);

	if (slvltovect(lvl) != -1) {
		rem_avintr(intr_id, lvl, xxintr,  slvltovect(lvl));
		return (1);
	}

	if (lvl <= 0 && lvl >= LOCK_LEVEL) {
		return (0);
	}
	vecp = &softvect[lvl];
	remove_av(intr_id, vecp, xxintr, lvl, 0);

	return (1);
}

void
rem_avintr(void *intr_id, int lvl, avfunc xxintr, int vect)
{
	register struct av_head *vecp = (struct av_head *)0;
	register avfunc f;
	int s;			/* save old spl value */

	if ((f = xxintr) == NULL)
		return;

	if (vect >= maxvects) {
		printf("IRQ %d is greater than allowed\n", vect);
		return;
	}

	vecp = &autovect[vect];
	remove_av(intr_id, vecp, f, lvl, vect);
	s = splhi();
	mutex_enter(&av_lock);
	(*delspl)(vect, lvl, vecp->avh_lo_pri, vecp->avh_hi_pri);
	mutex_exit(&av_lock);
	(void) splx(s);
}


/*
 * After having made a change to an autovector list, wait until we have
 * seen each cpu not executing an interrupt at that level--so we know our
 * change has taken effect completely (no old state in registers, etc).
 *
 * XXX this test does not work if release_intr has been called by the
 * XXX driver we're attempting to unload!
 *
 */
void
wait_till_seen(int ipl)
{
	register int cpu_in_chain, cix;
	register struct cpu *cpup;
	cpuset_t cpus_to_check;

	cpus_to_check = CPUSET_ALL;
	do {
		cpu_in_chain = 0;
		for (cix = 0; cix < NCPU; cix++) {
			cpup = cpu[cix];
			if (cpup != NULL && CPU_IN_SET(cpus_to_check, cix)) {
				if (intr_active(cpup, ipl)) {
					cpu_in_chain = 1;
				} else {
					CPUSET_DEL(cpus_to_check, cix);
				}
			}
		}
	} while (cpu_in_chain);
}

/* remove an interrupt vector from the chain */
static void
remove_av(void *intr_id, struct av_head *vectp, avfunc f, int pri_level,
	int vect)
{
	struct autovec *endp, *p, *target;
	int	lo_pri, hi_pri;
	int	ipl, unsafe;
	/*
	 * Protect rewrites of the list
	 */
	target = NULL;
	unsafe = UNSAFE_DRIVER_LOCK_HELD();

	if (unsafe)
		mutex_exit(&unsafe_driver);

	mutex_enter(&av_lock);
	ipl = pri_level;
	lo_pri = MAXIPL;
	hi_pri = 0;
	for (endp = p = vectp->avh_link; p && p->av_vector; p = p->av_link) {
		endp = p;
		if ((p->av_vector == f) && (p->av_intr_id == intr_id)) {
			/* found the handler */
			target = p;
			continue;
		}
		if (p->av_prilevel > hi_pri)
			hi_pri = p->av_prilevel;
		if (p->av_prilevel < lo_pri)
			lo_pri = p->av_prilevel;
	}
	if (ipl < hi_pri)
		ipl = hi_pri;
	if (target == NULL)	/* not found */
	{
		printf("Couldn't remove function %x at %d, %d\n",
			f, vect, pri_level);
		mutex_exit(&av_lock);
		if (unsafe)
			mutex_enter(&unsafe_driver);
		return;
	}
	/*
	 * I beleive even if it is only vector in chain, it is unsafe for
	 * kmem_freeing since we could reach this place on another cpu even
	 * before one cpu finishes processing the chain. Remeber we are talking
	 * possibilities not probabilities
	 */

	if (endp == target) {	/* vector to be removed is last in chain */
		target->av_vector = NULL;
		wait_till_seen(ipl);
	} else {
		target->av_vector = nullintr;
		wait_till_seen(ipl);
		target->av_intarg = endp->av_intarg;
		target->av_prilevel = endp->av_prilevel;
		target->av_mutex = endp->av_mutex;
		target->av_intr_id = endp->av_intr_id;
		target->av_vector = endp->av_vector;
		/*
		 * We have a hole here where the routine corresponding to
		 * endp may not get called. Do a wait_till_seen to take care
		 * of this.
		 */
		wait_till_seen(ipl);
		endp->av_vector = NULL;
	}


	if (lo_pri > hi_pri) {	/* the chain is now empty */
		for (p = vectp->avh_link; p; p = endp) {
			endp = p->av_link;
			kmem_free(p, sizeof (struct autovec));
		}
		vectp->avh_link = NULL;
		vectp->avh_lo_pri = 0;
		vectp->avh_hi_pri = 0;
	} else {
		if ((int)vectp->avh_lo_pri < lo_pri)
			vectp->avh_lo_pri = (u_short)lo_pri;
		if ((int)vectp->avh_hi_pri > hi_pri)
			vectp->avh_hi_pri = (u_short)hi_pri;
	}
	mutex_exit(&av_lock);
	wait_till_seen(ipl);
	if (unsafe)
		mutex_enter(&unsafe_driver);
}

/*
 *  This was an assemble language routine in sparc which
 *  processed software interrupts on the softcall list
 *  XXX I'm not sure that this is the best place for this
 *  XXX routine, but for now it will do
 *  All interrupt routine get called with an argument
 *  This routine doesn't use the argument
 */

/*ARGSUSED*/
u_int
softlevel1(caddr_t arg)
{
	softint();
	return (DDI_INTR_UNCLAIMED);
}

/*
 *  This was an assemble language routine in sparc which turned on
 *   priority 1 software interrupts which eventually call softlevel1
 *  XXX I'm not sure that this is the best place for this
 *  XXX routine, but for now it will do
 */

void
siron(void)
{
	(*setsoftint)(1);
}
