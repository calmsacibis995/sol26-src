/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1996 Sun Microsystems, Inc.
 * All rights reserved.
 */
#ident	"@(#)prsubr.c	1.89	96/08/21 SMI"	/* from SVr4.0 1.44 */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/inline.h>
#include <sys/kmem.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/session.h>
#include <sys/tblock.h>

#include <sys/pcb.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/disp.h>
#include <sys/class.h>
#include <sys/ts.h>
#include <sys/bitmap.h>
#include <sys/poll.h>

#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/procfs.h>
#include <sys/processor.h>
#include <sys/cpuvar.h>

#include <vm/as.h>
#include <vm/rm.h>
#include <vm/seg.h>
#include <vm/seg_vn.h>
#include <vm/seg_dev.h>
#include <sys/vmparam.h>

#include <fs/proc/prdata.h>

extern u_int timer_resolution;

static	int	set_watched_page(struct as *, caddr_t, caddr_t, u_long,
			u_long, struct watched_page *);
static	void	clear_watched_page(struct as *, caddr_t, caddr_t, u_long);

/*
 * Choose an lwp from the complete set of lwps for the process.
 * This is called for any operation applied to the process
 * file descriptor that requires an lwp to operate upon.
 *
 * Returns a pointer to the thread for the selected LWP,
 * and with the dispatcher lock held for the thread.
 *
 * The algorithm for choosing an lwp is critical for /proc semantics;
 * don't touch this code unless you know all of the implications.
 */
kthread_t *
prchoose(proc_t *p)
{
	kthread_t *t;
	kthread_t *t_onproc = NULL;	/* running on processor */
	kthread_t *t_run = NULL;	/* runnable, on disp queue */
	kthread_t *t_sleep = NULL;	/* sleeping */
	kthread_t *t_hold = NULL;	/* sleeping, performing hold */
	kthread_t *t_susp = NULL;	/* suspended stop */
	kthread_t *t_jstop = NULL;	/* jobcontrol stop */
	kthread_t *t_req = NULL;	/* requested stop */
	kthread_t *t_istop = NULL;	/* event-of-interest stop */

	ASSERT(MUTEX_HELD(&p->p_lock));

	if ((t = p->p_tlist) == NULL)
		return (t);
	do {
		if (VSTOPPED(t)) {	/* virtually stopped */
			if (t_req == NULL)
				t_req = t;
			continue;
		}

		thread_lock(t);		/* make sure thread is in good state */
		switch (t->t_state) {
		default:
			cmn_err(CE_PANIC,
			    "prchoose: bad thread state %d, thread 0x%x\n",
			    t->t_state, (int)t);

			break;
		case TS_SLEEP:
			/* this is filthy */
			if (t->t_wchan == (caddr_t)&p->p_holdlwps &&
			    t->t_wchan0 == 0) {
				if (t_hold == NULL)
					t_hold = t;
			} else {
				if (t_sleep == NULL)
					t_sleep = t;
			}
			break;
		case TS_RUN:
			if (t_run == NULL)
				t_run = t;
			break;
		case TS_ONPROC:
			if (t_onproc == NULL)
				t_onproc = t;
			break;
		case TS_ZOMB:		/* last possible choice */
			break;
		case TS_STOPPED:
			switch (t->t_whystop) {
			case PR_SUSPENDED:
				if (t_susp == NULL)
					t_susp = t;
				break;
			case PR_JOBCONTROL:
				if (t_jstop == NULL)
					t_jstop = t;
				break;
			case PR_REQUESTED:
				if (t_req == NULL)
					t_req = t;
				break;
			default:
				if (t_istop == NULL)
					t_istop = t;
				break;
			}
			break;
		}
		thread_unlock(t);
	} while ((t = t->t_forw) != p->p_tlist);

	if (t_onproc)
		t = t_onproc;
	else if (t_run)
		t = t_run;
	else if (t_sleep)
		t = t_sleep;
	else if (t_jstop)
		t = t_jstop;
	else if (t_istop)
		t = t_istop;
	else if (t_req)
		t = t_req;
	else if (t_hold)
		t = t_hold;
	else if (t_susp)
		t = t_susp;
	else			/* TS_ZOMB */
		t = p->p_tlist;

	if (t != NULL)
		thread_lock(t);
	return (t);
}

/*
 * Wakeup anyone sleeping on the /proc vnode for the process/lwp to stop.
 * Also call pollwakeup() if any lwps are waiting in poll() for POLLPRI
 * on the /proc file descriptor.  Called from stop() when a traced
 * process stops on an event of interest.  Also called from exit()
 * and prinvalidate() to indicate POLLHUP and POLLERR respectively.
 */
void
prnotify(struct vnode *vp)
{
	prcommon_t *pcp = VTOP(vp)->pr_common;

	mutex_enter(&pcp->prc_mutex);
	cv_broadcast(&pcp->prc_wait);
	mutex_exit(&pcp->prc_mutex);
	if (pcp->prc_flags & PRC_POLL) {
		/*
		 * We call pollwakeup() with POLLHUP to ensure that
		 * the pollers are awakened even if they are polling
		 * for nothing (i.e., waiting for the process to exit).
		 * This enables the use of the PRC_POLL flag for optimization
		 * (we can turn off PRC_POLL only if we know no pollers remain).
		 */
		pcp->prc_flags &= ~PRC_POLL;
		pollwakeup(&pcp->prc_pollhead, POLLHUP);
	}
}

/*
 * Called from a hook in freeproc() when a traced process is removed
 * from the process table.  The proc-table pointers of all associated
 * /proc vnodes are cleared to indicate that the process has gone away.
 */
void
prfree(proc_t *p)
{
	vnode_t *vp;
	prnode_t *pnp;
	prcommon_t *pcp;
	u_int slot = p->p_slot;

	ASSERT(MUTEX_HELD(&pidlock));

	/*
	 * Block the process against /proc so it can be freed.
	 * It cannot be freed while locked by some controlling process.
	 * Lock ordering:
	 *	pidlock -> pr_pidlock -> p->p_lock -> pcp->prc_mutex
	 */
	mutex_enter(&pr_pidlock);	/* protects pcp->prc_proc */
	mutex_enter(&p->p_lock);
	p->p_prwant++;
	while (p->p_flag & SPRLOCK) {
		mutex_exit(&pr_pidlock);
		cv_wait(&pr_pid_cv[slot], &p->p_lock);
		mutex_exit(&p->p_lock);
		mutex_enter(&pr_pidlock);
		mutex_enter(&p->p_lock);
	}

	ASSERT(p->p_tlist == NULL);

	vp = p->p_plist;
	p->p_plist = NULL;
	while (vp != NULL) {
		pnp = VTOP(vp);
		pcp = pnp->pr_common;
		ASSERT(pcp->prc_thread == NULL);
		pcp->prc_proc = NULL;
		/*
		 * We can't call prnotify() here because we are holding
		 * pidlock.  We assert that there is no need to.
		 */
		mutex_enter(&pcp->prc_mutex);
		cv_broadcast(&pcp->prc_wait);
		mutex_exit(&pcp->prc_mutex);
		ASSERT(!(pcp->prc_flags & PRC_POLL));

		vp = pnp->pr_next;
		pnp->pr_next = NULL;
	}
	if ((vp = p->p_trace) != NULL) {
		p->p_trace = NULL;
		VTOP(vp)->pr_common->prc_proc = NULL;
	}

	/*
	 * We broadcast to wake up everyone waiting for this process.
	 * No one can reach this process from this point on.
	 */
	cv_broadcast(&pr_pid_cv[slot]);

	mutex_exit(&p->p_lock);
	mutex_exit(&pr_pidlock);
}

/*
 * Called from a hook in exit() when a traced process is becoming a zombie.
 */
void
prexit(proc_t *p)
{
	ASSERT(MUTEX_HELD(&p->p_lock));

	if (p->p_warea) {
		pr_free_watchlist(p->p_warea);
		p->p_warea = NULL;
		p->p_nwarea = 0;
		curthread->t_proc_flag &= ~TP_WATCHPT;
	}
	/* pr_free_my_pagelist() is called in exit(), after dropping p_lock */
	if (p->p_trace) {
		VTOP(p->p_trace)->pr_common->prc_flags |= PRC_DESTROY;
		prnotify(p->p_trace);
	}
	cv_broadcast(&pr_pid_cv[p->p_slot]);	/* pauselwps() */
}

/*
 * Called when an lwp is destroyed.
 * lwps either destroy themselves or a sibling destroys them.
 * The thread pointer t is not necessarily the curthread.
 */
void
prlwpexit(kthread_t *t)
{
	vnode_t *vp;
	prnode_t *pnp;
	prcommon_t *pcp;
	proc_t *p = ttoproc(t);

	ASSERT(MUTEX_HELD(&p->p_lock));
	ASSERT(p == ttoproc(curthread));

	/*
	 * The process must be blocked against /proc to do this safely.
	 * The lwp must not disappear while the process is marked SPRLOCK.
	 * It is the caller's responsibility to have called prbarrier(p).
	 */
	ASSERT(!(p->p_flag & SPRLOCK));

	for (vp = p->p_plist; vp != NULL; vp = pnp->pr_next) {
		pnp = VTOP(vp);
		pcp = pnp->pr_common;
		if (pcp->prc_thread == t)
			pcp->prc_thread = NULL;
	}

	if (t->t_trace) {
		prnotify(t->t_trace);
		t->t_trace = NULL;
	}
	if (p->p_trace)
		prnotify(p->p_trace);
}

/*
 * Called from a hook in exec() when a process starts exec().
 */
void
prexecstart()
{
	proc_t *p = ttoproc(curthread);
	klwp_t *lwp = ttolwp(curthread);

	/*
	 * The SPREXEC flag blocks /proc operations for
	 * the duration of the exec().
	 * We can't start exec() while the process is
	 * locked by /proc, so we call prbarrier().
	 * lwp_nostop keeps the process from being stopped
	 * via job control for the duration of the exec().
	 */

	mutex_enter(&p->p_lock);
	prbarrier(p);
	lwp->lwp_nostop++;
	p->p_flag |= SPREXEC;
	mutex_exit(&p->p_lock);
}

/*
 * Called from a hook in exec() when a process finishes exec().
 */
void
prexecend()
{
	proc_t *p = ttoproc(curthread);
	klwp_t *lwp = ttolwp(curthread);
	prcommon_t *pcp;
	vnode_t *vp;

	/*
	 * Wake up anyone waiting in /proc for the process to complete exec().
	 */
	mutex_enter(&p->p_lock);
	lwp->lwp_nostop--;
	p->p_flag &= ~SPREXEC;
	if ((vp = p->p_trace) != NULL) {
		pcp = VTOP(vp)->pr_pcommon;
		mutex_enter(&pcp->prc_mutex);
		cv_broadcast(&pcp->prc_wait);
		mutex_exit(&pcp->prc_mutex);
	}
	if ((vp = curthread->t_trace) != NULL) {
		pcp = VTOP(vp)->pr_pcommon;
		mutex_enter(&pcp->prc_mutex);
		cv_broadcast(&pcp->prc_wait);
		mutex_exit(&pcp->prc_mutex);
	}
	mutex_exit(&p->p_lock);
}

/*
 * Called from a hook in relvm() just before freeing the address space.
 * We free all the watched areas now.
 */
void
prrelvm()
{
	proc_t *p = ttoproc(curthread);

	mutex_enter(&p->p_lock);
	prbarrier(p);	/* block all other /proc operations */
	if (p->p_warea) {
		pr_free_watchlist(p->p_warea);
		p->p_warea = NULL;
		p->p_nwarea = 0;
		curthread->t_proc_flag &= ~TP_WATCHPT;
	}
	mutex_exit(&p->p_lock);
	if (p->p_as && p->p_as->a_wpage)
		pr_free_my_pagelist();
}

/*
 * Called from hooks in exec-related code when a traced process
 * attempts to exec(2) a setuid/setgid program or an unreadable
 * file.  Rather than fail the exec we invalidate the associated
 * /proc vnodes so that subsequent attempts to use them will fail.
 *
 * All /proc vnodes, except directory vnodes, are retained on a linked
 * list (rooted at p_plist in the process structure) until last close.
 *
 * A controlling process must re-open the /proc files in order to
 * regain control.
 */
void
prinvalidate(struct user *up)
{
	kthread_t *t = curthread;
	proc_t *p = ttoproc(t);
	vnode_t *vp;
	prnode_t *pnp;
	int writers = 0;

	mutex_enter(&p->p_lock);
	prbarrier(p);	/* block all other /proc operations */

	/*
	 * At this moment, there can be only one lwp in the process.
	 */
	ASSERT(p->p_tlist == t && t->t_forw == t);

	/*
	 * Invalidate any currently active /proc vnodes.
	 */
	for (vp = p->p_plist; vp != NULL; vp = pnp->pr_next) {
		pnp = VTOP(vp);
		switch (pnp->pr_type) {
		case PR_PSINFO:		/* these files can read by anyone */
		case PR_LPSINFO:
		case PR_LWPSINFO:
		case PR_LWPDIR:
		case PR_LWPIDDIR:
		case PR_USAGE:
		case PR_LUSAGE:
		case PR_LWPUSAGE:
			break;
		default:
			pnp->pr_flags |= PR_INVAL;
			break;
		}
	}
	/*
	 * Wake up anyone waiting for the process or lwp.
	 */
	if ((vp = p->p_trace) != NULL) {
		prnotify(vp);
		/*
		 * Look through all the valid and invalid vnodes to
		 * determine if there are any outstanding writers.
		 */
		do {
			writers += VTOP(vp)->pr_common->prc_writers;
		} while ((vp = VTOP(vp)->pr_next) != NULL);
	}
	if ((vp = t->t_trace) != NULL)
		prnotify(vp);

	/*
	 * If any tracing flags are in effect and any vnodes are open for
	 * writing then set the requested-stop and run-on-last-close flags.
	 * Otherwise, clear all tracing flags.
	 */
	t->t_proc_flag &= ~TP_PAUSE;
	if ((p->p_flag & SPROCTR) && writers) {
		t->t_proc_flag |= TP_PRSTOP;
		aston(t);		/* so ISSIG will see the flag */
		p->p_flag |= SRUNLCL;
	} else {
		premptyset(&up->u_entrymask);		/* syscalls */
		premptyset(&up->u_exitmask);
		up->u_systrap = 0;
		premptyset(&p->p_sigmask);		/* signals */
		premptyset(&p->p_fltmask);		/* faults */
		t->t_proc_flag &= ~(TP_PRSTOP|TP_PRVSTOP|TP_STOPPING);
		p->p_flag &= ~(SRUNLCL|SKILLCL|SPROCTR);
		prnostep(ttolwp(t));
	}

	mutex_exit(&p->p_lock);
}

/*
 * Acquire the controlled process's p_lock and mark it SPRLOCK.
 * Return with pr_pidlock held in all cases.
 * Return with p_lock held if the the process still exists.
 * Return value is the process pointer if the process still exists, else NULL.
 * If we lock the process, give ourself kernel priority to avoid deadlocks;
 * this is undone in prunlock().
 */
proc_t *
pr_p_lock(prnode_t *pnp)
{
	proc_t *p;
	prcommon_t *pcp;

	mutex_enter(&pr_pidlock);
	if ((pcp = pnp->pr_pcommon) == NULL || (p = pcp->prc_proc) == NULL)
		return (NULL);
	mutex_enter(&p->p_lock);
	while (p->p_flag & SPRLOCK) {
		/*
		 * This cv/mutex pair is persistent even if
		 * the process disappears while we sleep.
		 */
		kcondvar_t *cv = &pr_pid_cv[p->p_slot];
		kmutex_t *mp = &p->p_lock;

		p->p_prwant++;
		mutex_exit(&pr_pidlock);
		cv_wait(cv, mp);
		mutex_exit(mp);
		mutex_enter(&pr_pidlock);
		if (pcp->prc_proc == NULL)
			return (NULL);
		ASSERT(p == pcp->prc_proc);
		mutex_enter(&p->p_lock);
		p->p_prwant--;
	}
	p->p_flag |= SPRLOCK;
	THREAD_KPRI_REQUEST();
	return (p);
}

/*
 * Lock the target process by setting SPRLOCK and grabbing p->p_lock.
 * This prevents any lwp of the process from disappearing and
 * blocks most operations that a process can perform on itself.
 * Returns 0 on success, a non-zero error number on failure.
 *
 * 'zdisp' is ZYES or ZNO to indicate whether encountering a
 * zombie process is to be considered an error.
 *
 * error returns:
 *	ENOENT: process or lwp has disappeared
 *		(or has become a zombie and zdisp == ZNO).
 *	EAGAIN: procfs vnode has become invalid.
 *	EINTR:  signal arrived while waiting for exec to complete.
 */
int
prlock(prnode_t *pnp, int zdisp)
{
	prcommon_t *pcp;
	proc_t *p;
	kthread_t *t;

again:
	pcp = pnp->pr_common;
	p = pr_p_lock(pnp);
	mutex_exit(&pr_pidlock);

	/*
	 * Return ENOENT immediately if there is no process.
	 */
	if (p == NULL)
		return (ENOENT);

	ASSERT(p == pcp->prc_proc && p->p_stat != 0 && p->p_stat != SIDL);

	/*
	 * Return EAGAIN if we have encountered a security violation.
	 * (The process exec'd a set-id or unreadable executable file.)
	 */
	if (pnp->pr_flags & PR_INVAL) {
		prunlock(pnp);
		return (EAGAIN);
	}

	/*
	 * Return ENOENT if process entered zombie state
	 * and we are not interested in zombies.
	 */
	if (zdisp == ZNO &&
	    ((pcp->prc_flags & PRC_DESTROY) || p->p_tlist == NULL)) {
		prunlock(pnp);
		return (ENOENT);
	}

	/*
	 * If lwp-specific, check to see if lwp has disappeared.
	 */
	if (pcp->prc_flags & PRC_LWP) {
		if ((t = pcp->prc_thread) == NULL ||
		    (zdisp == ZNO && t->t_state == TS_ZOMB)) {
			prunlock(pnp);
			return (ENOENT);
		}
		ASSERT(t->t_state != TS_FREE);
		ASSERT(ttoproc(t) == p);
	}

	/*
	 * If process is undergoing an exec(), wait for
	 * completion and then start all over again.
	 */
	if (p->p_flag & SPREXEC) {
		mutex_enter(&pcp->prc_mutex);
		prunlock(pnp);
		if (!cv_wait_sig(&pcp->prc_wait, &pcp->prc_mutex)) {
			mutex_exit(&pcp->prc_mutex);
			return (EINTR);
		}
		mutex_exit(&pcp->prc_mutex);
		goto again;
	}

	/*
	 * We return holding p->p_lock.
	 */
	return (0);
}

/*
 * Undo prlock() and pr_p_lock().
 * p->p_lock is still held; pr_pidlock is no longer held.
 *
 * prunmark() drops the SPRLOCK flag and wakes up another thread,
 * if any, waiting for the flag to be dropped; it retains p->p_lock.
 *
 * prunlock() calls prunmark() and then drops p->p_lock.
 */
void
prunmark(proc_t *p)
{
	ASSERT(p->p_flag & SPRLOCK);
	ASSERT(MUTEX_HELD(&p->p_lock));

	if (p->p_prwant)	/* Somebody wants the process */
		cv_signal(&pr_pid_cv[p->p_slot]);
	p->p_flag &= ~SPRLOCK;
	THREAD_KPRI_RELEASE();
}

void
prunlock(prnode_t *pnp)
{
	proc_t *p = pnp->pr_pcommon->prc_proc;

	prunmark(p);
	mutex_exit(&p->p_lock);
}

/*
 * Called while holding p->p_lock to delay until the process is unlocked.
 * We enter holding p->p_lock; p->p_lock is dropped and reacquired.
 * The process cannot become locked again until p->p_lock is dropped.
 */
void
prbarrier(proc_t *p)
{
	ASSERT(MUTEX_HELD(&p->p_lock));

	if (p->p_flag & SPRLOCK) {
		/* The process is locked; delay until not locked */
		u_int slot = p->p_slot;

		p->p_prwant++;
		while (p->p_flag & SPRLOCK)
			cv_wait(&pr_pid_cv[slot], &p->p_lock);
		if (--p->p_prwant)
			cv_signal(&pr_pid_cv[slot]);
	}
}

/*
 * Return process/lwp status.
 * The u-block is mapped in by this routine and unmapped at the end.
 */
void
prgetstatus(proc_t *p, pstatus_t *sp)
{
	kthread_t *t;
	kthread_t *aslwptp;
	u_long restonano;

	ASSERT(MUTEX_HELD(&p->p_lock));

	t = prchoose(p);	/* returns locked thread */
	ASSERT(t != NULL);
	thread_unlock(t);

	/* just bzero the process part, prgetlwpstatus() does the rest */
	bzero((caddr_t)sp, sizeof (pstatus_t) - sizeof (lwpstatus_t));
	sp->pr_nlwp = p->p_lwpcnt;
	if ((aslwptp = p->p_aslwptp) != NULL) {
		k_sigset_t set;

		set = aslwptp->t_sig;
		sigorset(&set, &p->p_notifsigs);
		prassignset(&sp->pr_sigpend, &set);
	} else {
		prassignset(&sp->pr_sigpend, &p->p_sig);
	}
	sp->pr_brkbase = (uintptr_t)p->p_brkbase;
	sp->pr_brksize = p->p_brksize;
	sp->pr_stkbase = (uintptr_t)prgetstackbase(p);
	sp->pr_stksize = p->p_stksize;
	sp->pr_pid   = p->p_pid;
	sp->pr_ppid  = p->p_ppid;
	sp->pr_pgid  = p->p_pgrp;
	sp->pr_sid   = p->p_sessp->s_sid;
	restonano = 1000000000 / timer_resolution;
	sp->pr_utime.tv_sec = p->p_utime / timer_resolution;
	sp->pr_utime.tv_nsec = (p->p_utime % timer_resolution) * restonano;
	sp->pr_stime.tv_sec = p->p_stime / timer_resolution;
	sp->pr_stime.tv_nsec = (p->p_stime % timer_resolution) * restonano;
	sp->pr_cutime.tv_sec = p->p_cutime / timer_resolution;
	sp->pr_cutime.tv_nsec = (p->p_cutime % timer_resolution) * restonano;
	sp->pr_cstime.tv_sec = p->p_cstime / timer_resolution;
	sp->pr_cstime.tv_nsec = (p->p_cstime % timer_resolution) * restonano;
	prassignset(&sp->pr_sigtrace, &p->p_sigmask);
	prassignset(&sp->pr_flttrace, &p->p_fltmask);
	prassignset(&sp->pr_sysentry, &PTOU(p)->u_entrymask);
	prassignset(&sp->pr_sysexit, &PTOU(p)->u_exitmask);
	if (p->p_aslwptp)
		sp->pr_aslwpid = p->p_aslwptp->t_tid;

	/* get the chosen lwp's status */
	prgetlwpstatus(t, &sp->pr_lwp);

	/* replicate the flags */
	sp->pr_flags = sp->pr_lwp.pr_flags;
}

/*
 * Return lwp status.
 */
void
prgetlwpstatus(kthread_t *t, lwpstatus_t *sp)
{
	proc_t *p = ttoproc(t);
	klwp_t *lwp = ttolwp(t);
	long flags;

	ASSERT(MUTEX_HELD(&p->p_lock));

	bzero((caddr_t)sp, sizeof (*sp));
	flags = 0L;
	if (t->t_state == TS_STOPPED) {
		flags |= PR_STOPPED;
		if ((t->t_schedflag & TS_PSTART) == 0)
			flags |= PR_ISTOP;
	} else if (VSTOPPED(t)) {
		flags |= PR_STOPPED|PR_ISTOP;
	}
	if (!(flags & PR_ISTOP) && (t->t_proc_flag & TP_PRSTOP))
		flags |= PR_DSTOP;
	if (lwp->lwp_asleep)
		flags |= PR_ASLEEP;
	if (t == p->p_aslwptp)
		flags |= PR_ASLWP;
	if (p->p_flag & SPRFORK)
		flags |= PR_FORK;
	if (p->p_flag & SRUNLCL)
		flags |= PR_RLC;
	if (p->p_flag & SKILLCL)
		flags |= PR_KLC;
	if (p->p_flag & SPASYNC)
		flags |= PR_ASYNC;
	if (p->p_flag & SBPTADJ)
		flags |= PR_BPTADJ;
	if (p->p_flag & STRC)
		flags |= PR_PTRACE;
	if (p->p_flag & SMSACCT)
		flags |= PR_MSACCT;
	if (p->p_flag & SVFWAIT)
		flags |= PR_VFORKP;
	sp->pr_flags = flags;
	if (VSTOPPED(t)) {
		sp->pr_why   = PR_REQUESTED;
		sp->pr_what  = 0;
	} else {
		sp->pr_why   = t->t_whystop;
		sp->pr_what  = t->t_whatstop;
	}
	sp->pr_lwpid = t->t_tid;
	sp->pr_cursig  = lwp->lwp_cursig;
	prassignset(&sp->pr_lwppend, &t->t_sig);
	prassignset(&sp->pr_lwphold, &t->t_hold);
	if (t->t_whystop == PR_FAULTED)
		bcopy((caddr_t)&lwp->lwp_siginfo,
		    (caddr_t)&sp->pr_info, sizeof (k_siginfo_t));
	else if (lwp->lwp_curinfo)
		bcopy((caddr_t)&lwp->lwp_curinfo->sq_info,
		    (caddr_t)&sp->pr_info, sizeof (k_siginfo_t));
	sp->pr_altstack = lwp->lwp_sigaltstack;
	prgetaction(p, PTOU(p), lwp->lwp_cursig, &sp->pr_action);
	sp->pr_oldcontext = (uintptr_t)lwp->lwp_oldcontext;
	bcopy(sclass[t->t_cid].cl_name, sp->pr_clname,
	    min(sizeof (sclass[0].cl_name), sizeof (sp->pr_clname)-1));
	if (flags & PR_STOPPED)
		hrt2ts(t->t_stoptime, &sp->pr_tstamp);

	/*
	 * Fetch the current instruction, if not a system process.
	 * We don't attempt this unless the lwp is stopped.
	 */
	if ((p->p_flag & SSYS) || p->p_as == &kas)
		sp->pr_flags |= (PR_ISSYS|PR_PCINVAL);
	else if (!(flags & PR_STOPPED))
		sp->pr_flags |= PR_PCINVAL;
	else if (!prfetchinstr(lwp, &sp->pr_instr))
		sp->pr_flags |= PR_PCINVAL;

	/*
	 * Drop p_lock while touching the lwp's stack.
	 */
	mutex_exit(&p->p_lock);
	if (prisstep(lwp))
		sp->pr_flags |= PR_STEP;
	if ((flags & (PR_STOPPED|PR_ASLEEP)) && t->t_sysnum) {
		int i;

		sp->pr_syscall = get_syscall_args(lwp,
			(int *)sp->pr_sysarg, &i);
		sp->pr_nsysarg = (u_short)i;
	}
	if (flags & PR_VFORKP) {
		sp->pr_syscall = SYS_vfork;
		sp->pr_nsysarg = 0;
	}
	prgetprregs(lwp, sp->pr_reg);
	if ((t->t_state == TS_STOPPED && t->t_whystop == PR_SYSEXIT) ||
	    (flags & PR_VFORKP))
		sp->pr_errno =
			prgetrvals(lwp, &sp->pr_rval1, &sp->pr_rval2);
	if (prhasfp())
		prgetprfpregs(lwp, &sp->pr_fpreg);
	mutex_enter(&p->p_lock);
}

/*
 * Get the sigaction structure for the specified signal.  The u-block
 * must already have been mapped in by the caller.
 */
void
prgetaction(proc_t *p, user_t *up, u_int sig, struct sigaction *sp)
{
	sp->sa_handler = SIG_DFL;
	premptyset(&sp->sa_mask);
	sp->sa_flags = 0;

	if (sig != 0 && (unsigned)sig < NSIG) {
		sp->sa_handler = up->u_signal[sig-1];
		prassignset(&sp->sa_mask, &up->u_sigmask[sig-1]);
		if (sigismember(&up->u_sigonstack, sig))
			sp->sa_flags |= SA_ONSTACK;
		if (sigismember(&up->u_sigresethand, sig))
			sp->sa_flags |= SA_RESETHAND;
		if (sigismember(&up->u_sigrestart, sig))
			sp->sa_flags |= SA_RESTART;
		if (sigismember(&p->p_siginfo, sig))
			sp->sa_flags |= SA_SIGINFO;
		if (sigismember(&up->u_signodefer, sig))
			sp->sa_flags |= SA_NODEFER;
		switch (sig) {
		case SIGCLD:
			if (p->p_flag & SNOWAIT)
				sp->sa_flags |= SA_NOCLDWAIT;
			if ((p->p_flag & SJCTL) == 0)
				sp->sa_flags |= SA_NOCLDSTOP;
			break;
		case SIGWAITING:
			if (p->p_flag & SWAITSIG)
				sp->sa_flags |= SA_WAITSIG;
			break;
		}
	}
}

/*
 * Count the number of segments in this process's address space.
 */
int
prnsegs(struct as *as, int reserved)
{
	int n = 0;
	struct seg *seg;

	ASSERT(as != &kas && AS_WRITE_HELD(as, &as->a_lock));

	for (seg = AS_SEGP(as, as->a_segs); seg != NULL;
	    seg = AS_SEGP(as, seg->s_next)) {
		caddr_t naddr;
		caddr_t saddr = seg->s_base;
		caddr_t eaddr = seg->s_base + seg->s_size;
		void *tmp = NULL;

		do {
			(void) pr_getprot(seg, reserved, &tmp, &saddr, &naddr);
			if (saddr != naddr)
				n++;
		} while ((saddr = naddr) != eaddr);
		ASSERT(tmp == NULL);
	}

	return (n);
}

/*
 * Convert unsigned long to decimal string w/o leading zeros.
 * Add trailing null characters if 'len' is greater than string length.
 * Return the string length.
 */
int
pr_utos(u_long n, char *s, int len)
{
	char cbuf[11];		/* 32-bit unsigned integer fits in 10 digits */
	char *cp = cbuf;
	char *end = s + len;

	do {
		*cp++ = n % 10 + '0';
		n /= 10;
	} while (n);

	len = cp - cbuf;

	do {
		*s++ = *--cp;
	} while (cp > cbuf);

	while (s < end)		/* optional pad */
		*s++ = '\0';

	return (len);
}

/*
 * Convert u_longlong_t to decimal string w/o leading zeros.
 * Return the string length.
 */
static int
pr_u64tos(u_longlong_t n, char *s)
{
	char cbuf[21];		/* 64-bit unsigned integer fits in 20 digits */
	char *cp = cbuf;
	int len;

	do {
		*cp++ = n % 10 + '0';
		n /= 10;
	} while (n);

	len = cp - cbuf;

	do {
		*s++ = *--cp;
	} while (cp > cbuf);

	return (len);
}

void
pr_object_name(char *name, vnode_t *vp, struct vattr *vattr)
{
	char *s = name;
	struct vfs *vfsp;
	struct vfssw *vfsswp;

	if ((vfsp = vp->v_vfsp) != NULL &&
	    ((vfsswp = vfssw + vfsp->vfs_fstype), vfsswp->vsw_name) &&
	    *vfsswp->vsw_name) {
		strcpy(s, vfsswp->vsw_name);
		s += strlen(s);
		*s++ = '.';
	}
	s += pr_utos(getmajor(vattr->va_fsid), s, 0);
	*s++ = '.';
	s += pr_utos(getminor(vattr->va_fsid), s, 0);
	*s++ = '.';
	s += pr_u64tos(vattr->va_nodeid, s);
	*s++ = '\0';
}

/*
 * Return an array of structures with memory map information.
 * We allocate here; the caller must deallocate.
 */
#define	MAPSIZE	8192
int
prgetmap(proc_t *p, int reserved, prmap_t **prmapp, size_t *sizep)
{
	struct as *as = p->p_as;
	int nmaps = 0;
	prmap_t *mp;
	size_t size;
	struct seg *seg;
	struct seg *brkseg, *stkseg;
	struct vnode *vp;
	struct vattr vattr;
	int prot;

	ASSERT(as != &kas && AS_WRITE_HELD(as, &as->a_lock));

	/* initial allocation */
	*sizep = size = MAPSIZE;
	*prmapp = mp = kmem_alloc(MAPSIZE, KM_SLEEP);

	if ((seg = AS_SEGP(as, as->a_segs)) == NULL)
		return (0);

	brkseg = as_segat(as, p->p_brkbase + p->p_brksize - 1);
	stkseg = as_segat(as, prgetstackbase(p));

	do {
		caddr_t naddr;
		caddr_t saddr = seg->s_base;
		caddr_t eaddr = seg->s_base + seg->s_size;
		void *tmp = NULL;

		do {
			prot = pr_getprot(seg, reserved, &tmp, &saddr, &naddr);
			if (saddr == naddr)
				continue;
			/* reallocate if necessary */
			if ((nmaps + 1) * sizeof (prmap_t) > size) {
				size_t newsize = size + MAPSIZE;
				prmap_t *newmp = kmem_alloc(newsize, KM_SLEEP);

				bcopy(*prmapp, newmp, nmaps * sizeof (prmap_t));
				kmem_free(*prmapp, size);
				*sizep = size = newsize;
				*prmapp = newmp;
				mp = newmp + nmaps;
			}
			bzero(mp, sizeof (*mp));
			mp->pr_vaddr = (uintptr_t)saddr;
			mp->pr_size = naddr - saddr;
			mp->pr_offset = SEGOP_GETOFFSET(seg, saddr);
			mp->pr_mflags = 0;
			if (prot & PROT_READ)
				mp->pr_mflags |= MA_READ;
			if (prot & PROT_WRITE)
				mp->pr_mflags |= MA_WRITE;
			if (prot & PROT_EXEC)
				mp->pr_mflags |= MA_EXEC;
			if (SEGOP_GETTYPE(seg, saddr) == MAP_SHARED)
				mp->pr_mflags |= MA_SHARED;
			if (seg == brkseg)
				mp->pr_mflags |= MA_BREAK;
			else if (seg == stkseg) {
				mp->pr_mflags |= MA_STACK;
				if (reserved) {
					user_t *up = prumap(p);
					size_t maxstack =
					    ((size_t)U_CURLIMIT(up,
					    RLIMIT_STACK) + PAGEOFFSET) &
					    PAGEMASK;
					prunmap(p);
					mp->pr_vaddr =
					    (uintptr_t)prgetstackbase(p) +
					    p->p_stksize - maxstack;
					mp->pr_size = (uintptr_t)naddr -
					    mp->pr_vaddr;
				}
			}
			mp->pr_pagesize = PAGESIZE;

			/*
			 * Manufacture a filename for the "object" directory.
			 */
			vattr.va_mask = AT_FSID|AT_NODEID;
			if (seg->s_ops == &segvn_ops &&
			    SEGOP_GETVP(seg, saddr, &vp) == 0 &&
			    vp != NULL && vp->v_type == VREG &&
			    VOP_GETATTR(vp, &vattr, 0, CRED()) == 0) {
				if (vp == p->p_exec)
					strcpy(mp->pr_mapname, "a.out");
				else
					pr_object_name(mp->pr_mapname,
						vp, &vattr);
			}
			mp++;
			nmaps++;
		} while ((saddr = naddr) != eaddr);
		ASSERT(tmp == NULL);
	} while ((seg = AS_SEGP(as, seg->s_next)) != NULL);

	return (nmaps);
}

/*
 * Return the size of the /proc page data file.
 */
long
prpdsize(struct as *as)
{
	struct seg *seg;
	long size;

	ASSERT(as != &kas && AS_WRITE_HELD(as, &as->a_lock));

	if ((seg = AS_SEGP(as, as->a_segs)) == NULL)
		return (0);

	size = sizeof (prpageheader_t);
	do {
		caddr_t naddr;
		caddr_t saddr = seg->s_base;
		caddr_t eaddr = seg->s_base + seg->s_size;
		void *tmp = NULL;
		int npage;

		do {
			(void) pr_getprot(seg, 0, &tmp, &saddr, &naddr);
			if ((npage = (naddr-saddr)/PAGESIZE) != 0)
				size += sizeof (prasmap_t) + round8(npage);
		} while ((saddr = naddr) != eaddr);
		ASSERT(tmp == NULL);
	} while ((seg = AS_SEGP(as, seg->s_next)) != NULL);

	return (size);
}

/*
 * Read page data information.
 * The address space is locked and will not change.
 */
int
prpdread(struct as *as,
	vnode_t *exec,
	u_int hatid,
	struct uio *uiop)
{
	caddr_t buf;
	long size;
	prpageheader_t *php;
	prasmap_t *pmp;
	struct seg *seg;
	int error;

	AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);

	if ((seg = AS_SEGP(as, as->a_segs)) == NULL) {
		AS_LOCK_EXIT(as, &as->a_lock);
		return (0);
	}
	size = prpdsize(as);
	ASSERT(size > 0);
	if (uiop->uio_resid < size) {
		AS_LOCK_EXIT(as, &as->a_lock);
		return (E2BIG);
	}

	buf = kmem_zalloc(size, KM_SLEEP);
	php = (prpageheader_t *)buf;
	pmp = (prasmap_t *)(buf + sizeof (prpageheader_t));

	hrt2ts(gethrtime(), &php->pr_tstamp);
	php->pr_nmap = 0;
	php->pr_npage = 0;
	do {
		caddr_t naddr;
		caddr_t saddr = seg->s_base;
		caddr_t eaddr = saddr + seg->s_size;
		void *tmp = NULL;

		do {
			struct vnode *vp;
			struct vattr vattr;
			u_int len;
			int npage;
			int prot;

			prot = pr_getprot(seg, 0, &tmp, &saddr, &naddr);
			if ((len = naddr - saddr) == 0)
				continue;
			npage = len/PAGESIZE;
			ASSERT(npage > 0);
			php->pr_nmap++;
			php->pr_npage += npage;
			pmp->pr_vaddr = (uintptr_t)saddr;
			pmp->pr_npage = npage;
			pmp->pr_offset = SEGOP_GETOFFSET(seg, saddr);
			pmp->pr_mflags = 0;
			if (prot & PROT_READ)
				pmp->pr_mflags |= MA_READ;
			if (prot & PROT_WRITE)
				pmp->pr_mflags |= MA_WRITE;
			if (prot & PROT_EXEC)
				pmp->pr_mflags |= MA_EXEC;
			if (SEGOP_GETTYPE(seg, saddr) == MAP_SHARED)
				pmp->pr_mflags |= MA_SHARED;
			pmp->pr_pagesize = PAGESIZE;
			/*
			 * Manufacture a filename for the "object" directory.
			 */
			bzero(pmp->pr_mapname, sizeof (pmp->pr_mapname));
			vattr.va_mask = AT_FSID|AT_NODEID;
			if (seg->s_ops == &segvn_ops &&
			    SEGOP_GETVP(seg, saddr, &vp) == 0 &&
			    vp != NULL && vp->v_type == VREG &&
			    VOP_GETATTR(vp, &vattr, 0, CRED()) == 0) {
				if (vp == exec)
					strcpy(pmp->pr_mapname, "a.out");
				else
					pr_object_name(pmp->pr_mapname,
						vp, &vattr);
			}
			hat_getstatby(as, saddr, len, hatid,
			    (char *)(pmp+1), 1);
			pmp = (prasmap_t *)((caddr_t)(pmp+1) + round8(npage));
		} while ((saddr = naddr) != eaddr);
		ASSERT(tmp == NULL);
	} while ((seg = AS_SEGP(as, seg->s_next)) != NULL);

	AS_LOCK_EXIT(as, &as->a_lock);

	ASSERT((caddr_t)pmp == buf+size);
	error = uiomove(buf, size, UIO_READ, uiop);
	kmem_free(buf, size);

	return (error);
}

/*
 * Return information used by ps(1).
 */
void
prgetpsinfo(proc_t *p, psinfo_t *psp)
{
	kthread_t *t;
	u_long hztime;
	struct cred *cred;

	ASSERT(MUTEX_HELD(&p->p_lock));

	if ((t = prchoose(p)) == NULL)	/* returns locked thread */
		bzero((caddr_t)psp, sizeof (*psp));
	else {
		thread_unlock(t);
		bzero((caddr_t)psp, sizeof (*psp) - sizeof (psp->pr_lwp));
	}

	psp->pr_flag = p->p_flag;
	psp->pr_nlwp = p->p_lwpcnt;
	mutex_enter(&p->p_crlock);
	cred = p->p_cred;
	psp->pr_uid = cred->cr_ruid;
	psp->pr_euid = cred->cr_uid;
	psp->pr_gid = cred->cr_rgid;
	psp->pr_egid = cred->cr_gid;
	mutex_exit(&p->p_crlock);
	psp->pr_pid = p->p_pid;
	psp->pr_ppid = p->p_ppid;
	psp->pr_pgid = p->p_pgrp;
	psp->pr_sid = p->p_sessp->s_sid;
	psp->pr_addr = (uintptr_t)prgetpsaddr(p);

	hztime = p->p_utime + p->p_stime;
	psp->pr_time.tv_sec = hztime / timer_resolution;
	psp->pr_time.tv_nsec =
	    (hztime % timer_resolution) * (1000000000 / timer_resolution);
	hztime = p->p_cutime + p->p_cstime;
	psp->pr_ctime.tv_sec = hztime / timer_resolution;
	psp->pr_ctime.tv_nsec =
	    (hztime % timer_resolution) * (1000000000 / timer_resolution);
	if (t == NULL) {
		extern int wstat(int, int);	/* needs a header file */
		int wcode = p->p_wcode;		/* must be atomic read */

		if (wcode)
			psp->pr_wstat = wstat(wcode, p->p_wdata);
		psp->pr_ttydev = PRNODEV;
		psp->pr_lwp.pr_state = SZOMB;
		psp->pr_lwp.pr_sname = 'Z';
	} else {
		user_t *up = PTOU(p);
		struct as *as;
		dev_t d;
		extern dev_t rwsconsdev, rconsdev, uconsdev;

		d = cttydev(p);
		/*
		 * If the controlling terminal is the real
		 * or workstation console device, map to what the
		 * user thinks is the console device.
		 */
		if (d == rwsconsdev || d == rconsdev)
			d = uconsdev;
		psp->pr_ttydev = (d == NODEV) ? PRNODEV : d;
		psp->pr_start.tv_sec = up->u_start;
		psp->pr_start.tv_nsec = 0L;
		bcopy(up->u_comm, psp->pr_fname,
		    min(sizeof (up->u_comm), sizeof (psp->pr_fname)-1));
		bcopy(up->u_psargs, psp->pr_psargs,
		    min(PRARGSZ-1, PSARGSZ));
		psp->pr_argc = up->u_argc;
		psp->pr_argv = (uintptr_t)up->u_argv;
		psp->pr_envp = (uintptr_t)up->u_envp;

		/* get the chosen lwp's lwpsinfo */
		prgetlwpsinfo(t, &psp->pr_lwp);

		/* compute %cpu for the process */
		if (p->p_lwpcnt == 1)
			psp->pr_pctcpu = psp->pr_lwp.pr_pctcpu;
		else {
			clock_t ticks = lbolt;
			u_long pct = 0;

			t = p->p_tlist;
			do {
				pct += cpu_decay(t->t_pctcpu,
					ticks - t->t_lbolt - 1);
			} while ((t = t->t_forw) != p->p_tlist);

			/* prorate over online cpus so we don't exceed 100% */
			if (ncpus > 1)
				pct /= ncpus;
			if (pct > 0x8000) /* might happen, due to rounding */
				pct = 0x8000;
			psp->pr_pctcpu = pct;
		}
		if ((p->p_flag & SSYS) || (as = p->p_as) == &kas) {
			psp->pr_size = 0;
			psp->pr_rssize = 0;
		} else {
			mutex_exit(&p->p_lock);
			AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
			psp->pr_size = btoc(rm_assize(as)) * (PAGESIZE / 1024);
			psp->pr_rssize = rm_asrss(as) * (PAGESIZE / 1024);
			psp->pr_pctmem = rm_pctmemory(as);
			AS_LOCK_EXIT(as, &as->a_lock);
			mutex_enter(&p->p_lock);
		}
	}
}

void
prgetlwpsinfo(kthread_t *t, lwpsinfo_t *psp)
{
	klwp_t *lwp = ttolwp(t);
	char c, state;
	u_long hztime;
	u_long pct;
	int retval, niceval;
	clock_t ticks;

	ASSERT(MUTEX_HELD(&ttoproc(t)->p_lock));

	bzero((caddr_t)psp, sizeof (*psp));

	psp->pr_flag = t->t_flag;
	psp->pr_lwpid = t->t_tid;
	psp->pr_addr = (uintptr_t)t;
	psp->pr_wchan = (uintptr_t)t->t_wchan;

	/* map the thread state enum into a process state enum */
	state = VSTOPPED(t) ? TS_STOPPED : t->t_state;
	switch (state) {
	case TS_SLEEP:		state = SSLEEP;		c = 'S';	break;
	case TS_RUN:		state = SRUN;		c = 'R';	break;
	case TS_ONPROC:		state = SONPROC;	c = 'O';	break;
	case TS_ZOMB:		state = SZOMB;		c = 'Z';	break;
	case TS_STOPPED:	state = SSTOP;		c = 'T';	break;
	default:		state = 0;		c = '?';	break;
	}
	psp->pr_state = state;
	psp->pr_sname = c;
	psp->pr_stype = 0;	/* XXX ??? */
	retval = CL_DONICE(t, NULL, 0, &niceval);
	if (retval == 0) {
		psp->pr_oldpri = v.v_maxsyspri - t->t_pri;
		psp->pr_nice = niceval + NZERO;
	} else {
		psp->pr_oldpri = 0;
		psp->pr_nice = 0;
	}
	psp->pr_syscall = t->t_sysnum;
	psp->pr_pri = t->t_pri;
	psp->pr_start.tv_sec = t->t_start;
	psp->pr_start.tv_nsec = 0L;
	hztime = lwp->lwp_utime + lwp->lwp_stime;
	psp->pr_time.tv_sec = hztime / timer_resolution;
	psp->pr_time.tv_nsec =
	    (hztime % timer_resolution) * (1000000000 / timer_resolution);

	/* compute %cpu for the lwp */
	ticks = lbolt;
	pct = 0;
	pct += cpu_decay(t->t_pctcpu, ticks - t->t_lbolt - 1);
	/* prorate over the online cpus so we don't exceed 100% */
	if (ncpus > 1)
		pct /= ncpus;
	if (pct > 0x8000)	/* might happen, due to rounding */
		pct = 0x8000;
	psp->pr_pctcpu = pct;
	psp->pr_cpu = (pct*100 + 0x6000) >> 15;	/* [0..99] */
	if (psp->pr_cpu > 99)
		psp->pr_cpu = 99;

	bcopy(sclass[t->t_cid].cl_name, psp->pr_clname,
	    min(sizeof (sclass[0].cl_name), sizeof (psp->pr_clname)-1));
	bzero(psp->pr_name, sizeof (psp->pr_name));	/* XXX ??? */
	psp->pr_onpro = t->t_cpu->cpu_id;
	psp->pr_bindpro = t->t_bind_cpu;
	psp->pr_bindpset = t->t_bind_pset;
}

/*
 * Called when microstate accounting information is requested for a thread
 * where microstate accounting (TP_MSACCT) isn't on.  Turn it on for this and
 * all other LWPs in the process and get an estimate of usage so far.
 */
void
estimate_msacct(kthread_t *t, hrtime_t curtime)
{
	proc_t *p;
	klwp_t *lwp;
	struct mstate *ms;
	hrtime_t ns;

	if (t == NULL)
		return;

	p = ttoproc(t);
	ASSERT(MUTEX_HELD(&p->p_lock));

	/*
	 * A system process (p0) could be referenced if the thread is
	 * in the process of exiting.  Don't turn on microstate accounting
	 * in that case.
	 */
	if (p->p_flag & SSYS)
		return;

	/*
	 * Loop through all the LWPs (kernel threads) in the process.
	 */
	t = p->p_tlist;
	do {
		int ms_prev;
		int lwp_state;
		hrtime_t total;
		int i;

		ASSERT((t->t_proc_flag & TP_MSACCT) == 0);

		lwp = ttolwp(t);
		ms = &lwp->lwp_mstate;

		bzero((caddr_t)&ms->ms_acct[0], sizeof (ms->ms_acct));

		/*
		 * Convert tick-based user and system time to microstate times.
		 */
		ns = (hrtime_t)nsec_per_tick;
		ms->ms_acct[LMS_USER] = lwp->lwp_utime * ns;
		ms->ms_acct[LMS_SYSTEM] = lwp->lwp_stime * ns;
		/*
		 * Add all unaccounted-for time to the LMS_SLEEP time.
		 */
		for (total = 0, i = 0; i < NMSTATES; i++)
			total += ms->ms_acct[i];
		ms->ms_acct[LMS_SLEEP] += curtime - ms->ms_start - total;
		t->t_waitrq = 0;

		/*
		 * Determine the current microstate and set the start time.
		 * Be careful not to touch the lwp while holding thread_lock().
		 */
		ms->ms_state_start = curtime;
		lwp_state = lwp->lwp_state;
		thread_lock(t);
		switch (t->t_state) {
		case TS_SLEEP:
			t->t_mstate = LMS_SLEEP;
			ms_prev = LMS_SYSTEM;
			break;
		case TS_RUN:
			t->t_waitrq = curtime;
			t->t_mstate = LMS_SLEEP;
			ms_prev = LMS_SYSTEM;
			break;
		case TS_ONPROC:
			/*
			 * The user/system state cannot be determined accurately
			 * on MP without stopping the thread.
			 * This might miss a system/user state transition.
			 */
			if (lwp_state == LWP_USER) {
				t->t_mstate = ms_prev = LMS_USER;
			} else {
				t->t_mstate = ms_prev = LMS_SYSTEM;
			}
			break;
		case TS_ZOMB:
		case TS_FREE:			/* shouldn't happen */
		case TS_STOPPED:
			t->t_mstate = LMS_STOPPED;
			ms_prev = LMS_SYSTEM;
			break;
		}
		thread_unlock(t);
		ms->ms_prev = ms_prev;	/* guess previous running state */
		t->t_proc_flag |= TP_MSACCT;
	} while ((t = t->t_forw) != p->p_tlist);

	p->p_flag |= SMSACCT;			/* set process-wide MSACCT */
	/*
	 * Set system call pre- and post-processing flags for the process.
	 * This must be done AFTER the TP_MSACCT flag is set.
	 * Do this outside of the loop to avoid re-ordering.
	 */
	set_proc_sys(p);
}

/*
 * Turn off microstate accounting for all LWPs in the process.
 */
void
disable_msacct(proc_t *p)
{
	kthread_id_t t;

	ASSERT(MUTEX_HELD(&p->p_lock));

	p->p_flag &= ~SMSACCT;		/* clear process-wide MSACCT */
	/*
	 * Loop through all the LWPs (kernel threads) in the process.
	 */
	if ((t = p->p_tlist) != NULL) {
		do {
			/* clear per-thread flag */
			t->t_proc_flag &= ~TP_MSACCT;
		} while ((t = t->t_forw) != p->p_tlist);
	}
}

/*
 * Return resource usage information.
 */
void
prgetusage(kthread_t *t, prhusage_t *pup)
{
	klwp_t *lwp = ttolwp(t);
	hrtime_t *mstimep;
	struct mstate *ms = &lwp->lwp_mstate;
	int state;
	hrtime_t curtime;
	hrtime_t waitrq;

	curtime = pup->pr_tstamp;	/* passed by caller */

	/*
	 * If microstate accounting (TP_MSACCT) isn't on, turn it on and
	 * get an estimate of usage so far.
	 */
	if ((t->t_proc_flag & TP_MSACCT) == 0)
		estimate_msacct(t, curtime);

	pup->pr_lwpid	= t->t_tid;
	pup->pr_count	= 1;
	pup->pr_create	= ms->ms_start;
	pup->pr_term	= ms->ms_term;
	if (ms->ms_term == 0)
		pup->pr_rtime = curtime - ms->ms_start;
	else
		pup->pr_rtime = ms->ms_term - ms->ms_start;

	pup->pr_utime    = ms->ms_acct[LMS_USER];
	pup->pr_stime    = ms->ms_acct[LMS_SYSTEM];
	pup->pr_ttime    = ms->ms_acct[LMS_TRAP];
	pup->pr_tftime   = ms->ms_acct[LMS_TFAULT];
	pup->pr_dftime   = ms->ms_acct[LMS_DFAULT];
	pup->pr_kftime   = ms->ms_acct[LMS_KFAULT];
	pup->pr_ltime    = ms->ms_acct[LMS_USER_LOCK];
	pup->pr_slptime  = ms->ms_acct[LMS_SLEEP];
	pup->pr_wtime    = ms->ms_acct[LMS_WAIT_CPU];
	pup->pr_stoptime = ms->ms_acct[LMS_STOPPED];

	/*
	 * Adjust for time waiting in the dispatcher queue.
	 */
	waitrq = t->t_waitrq;	/* hopefully atomic */
	if (waitrq != 0) {
		pup->pr_wtime += curtime - waitrq;
		curtime = waitrq;
	}

	/*
	 * Adjust for time spent in current microstate.
	 */
	switch (state = t->t_mstate) {
	case LMS_SLEEP:
		/*
		 * Update the timer for the current sleep state.
		 */
		switch (state = ms->ms_prev) {
		case LMS_TFAULT:
		case LMS_DFAULT:
		case LMS_KFAULT:
		case LMS_USER_LOCK:
			break;
		default:
			state = LMS_SLEEP;
			break;
		}
		break;
	case LMS_TFAULT:
	case LMS_DFAULT:
	case LMS_KFAULT:
	case LMS_USER_LOCK:
		state = LMS_SYSTEM;
		break;
	}
	switch (state) {
	case LMS_USER:		mstimep = &pup->pr_utime;	break;
	case LMS_SYSTEM:	mstimep = &pup->pr_stime;	break;
	case LMS_TRAP:		mstimep = &pup->pr_ttime;	break;
	case LMS_TFAULT:	mstimep = &pup->pr_tftime;	break;
	case LMS_DFAULT:	mstimep = &pup->pr_dftime;	break;
	case LMS_KFAULT:	mstimep = &pup->pr_kftime;	break;
	case LMS_USER_LOCK:	mstimep = &pup->pr_ltime;	break;
	case LMS_SLEEP:		mstimep = &pup->pr_slptime;	break;
	case LMS_WAIT_CPU:	mstimep = &pup->pr_wtime;	break;
	case LMS_STOPPED:	mstimep = &pup->pr_stoptime;	break;
	default:		panic("prgetusage: unknown microstate");
	}
	*mstimep += curtime - ms->ms_state_start;

	/*
	 * Resource usage counters.
	 */
	pup->pr_minf  = lwp->lwp_ru.minflt;
	pup->pr_majf  = lwp->lwp_ru.majflt;
	pup->pr_nswap = lwp->lwp_ru.nswap;
	pup->pr_inblk = lwp->lwp_ru.inblock;
	pup->pr_oublk = lwp->lwp_ru.oublock;
	pup->pr_msnd  = lwp->lwp_ru.msgsnd;
	pup->pr_mrcv  = lwp->lwp_ru.msgrcv;
	pup->pr_sigs  = lwp->lwp_ru.nsignals;
	pup->pr_vctx  = lwp->lwp_ru.nvcsw;
	pup->pr_ictx  = lwp->lwp_ru.nivcsw;
	pup->pr_sysc  = lwp->lwp_ru.sysc;
	pup->pr_ioch  = lwp->lwp_ru.ioch;
}

/*
 * Sum resource usage information.
 */
void
praddusage(kthread_t *t, prhusage_t *pup)
{
	klwp_t *lwp = ttolwp(t);
	hrtime_t *mstimep;
	struct mstate *ms = &lwp->lwp_mstate;
	int state;
	hrtime_t curtime;
	hrtime_t waitrq;

	curtime = pup->pr_tstamp;	/* passed by caller */

	/*
	 * If microstate accounting (TP_MSACCT) isn't on, turn it on and
	 * get an estimate of usage so far.
	 */
	if ((t->t_proc_flag & TP_MSACCT) == 0)
		estimate_msacct(t, curtime);

	if (ms->ms_term == 0)
		pup->pr_rtime += curtime - ms->ms_start;
	else
		pup->pr_rtime += ms->ms_term - ms->ms_start;
	pup->pr_utime	+= ms->ms_acct[LMS_USER];
	pup->pr_stime	+= ms->ms_acct[LMS_SYSTEM];
	pup->pr_ttime	+= ms->ms_acct[LMS_TRAP];
	pup->pr_tftime	+= ms->ms_acct[LMS_TFAULT];
	pup->pr_dftime	+= ms->ms_acct[LMS_DFAULT];
	pup->pr_kftime	+= ms->ms_acct[LMS_KFAULT];
	pup->pr_ltime	+= ms->ms_acct[LMS_USER_LOCK];
	pup->pr_slptime	+= ms->ms_acct[LMS_SLEEP];
	pup->pr_wtime	+= ms->ms_acct[LMS_WAIT_CPU];
	pup->pr_stoptime += ms->ms_acct[LMS_STOPPED];

	/*
	 * Adjust for time waiting in the dispatcher queue.
	 */
	waitrq = t->t_waitrq;	/* hopefully atomic */
	if (waitrq != 0) {
		pup->pr_wtime += curtime - waitrq;
		curtime = waitrq;
	}

	/*
	 * Adjust for time spent in current microstate.
	 */
	switch (state = t->t_mstate) {
	case LMS_SLEEP:
		/*
		 * Update the timer for the current sleep state.
		 */
		switch (state = ms->ms_prev) {
		case LMS_TFAULT:
		case LMS_DFAULT:
		case LMS_KFAULT:
		case LMS_USER_LOCK:
			break;
		default:
			state = LMS_SLEEP;
			break;
		}
		break;
	case LMS_TFAULT:
	case LMS_DFAULT:
	case LMS_KFAULT:
	case LMS_USER_LOCK:
		state = LMS_SYSTEM;
		break;
	}
	switch (state) {
	case LMS_USER:		mstimep = &pup->pr_utime;	break;
	case LMS_SYSTEM:	mstimep = &pup->pr_stime;	break;
	case LMS_TRAP:		mstimep = &pup->pr_ttime;	break;
	case LMS_TFAULT:	mstimep = &pup->pr_tftime;	break;
	case LMS_DFAULT:	mstimep = &pup->pr_dftime;	break;
	case LMS_KFAULT:	mstimep = &pup->pr_kftime;	break;
	case LMS_USER_LOCK:	mstimep = &pup->pr_ltime;	break;
	case LMS_SLEEP:		mstimep = &pup->pr_slptime;	break;
	case LMS_WAIT_CPU:	mstimep = &pup->pr_wtime;	break;
	case LMS_STOPPED:	mstimep = &pup->pr_stoptime;	break;
	default:		panic("praddusage: unknown microstate");
	}
	*mstimep += curtime - ms->ms_state_start;

	/*
	 * Resource usage counters.
	 */
	pup->pr_minf  += lwp->lwp_ru.minflt;
	pup->pr_majf  += lwp->lwp_ru.majflt;
	pup->pr_nswap += lwp->lwp_ru.nswap;
	pup->pr_inblk += lwp->lwp_ru.inblock;
	pup->pr_oublk += lwp->lwp_ru.oublock;
	pup->pr_msnd  += lwp->lwp_ru.msgsnd;
	pup->pr_mrcv  += lwp->lwp_ru.msgrcv;
	pup->pr_sigs  += lwp->lwp_ru.nsignals;
	pup->pr_vctx  += lwp->lwp_ru.nvcsw;
	pup->pr_ictx  += lwp->lwp_ru.nivcsw;
	pup->pr_sysc  += lwp->lwp_ru.sysc;
	pup->pr_ioch  += lwp->lwp_ru.ioch;
}

/*
 * Convert a prhusage_t to a prusage_t, in place.
 * This means convert each hrtime_t to a timestruc_t.
 */
void
prcvtusage(prhusage_t *pup)
{
	hrt2ts(pup->pr_tstamp,	(timestruc_t *)&pup->pr_tstamp);
	hrt2ts(pup->pr_create,	(timestruc_t *)&pup->pr_create);
	hrt2ts(pup->pr_term,	(timestruc_t *)&pup->pr_term);
	hrt2ts(pup->pr_rtime,	(timestruc_t *)&pup->pr_rtime);
	hrt2ts(pup->pr_utime,	(timestruc_t *)&pup->pr_utime);
	hrt2ts(pup->pr_stime,	(timestruc_t *)&pup->pr_stime);
	hrt2ts(pup->pr_ttime,	(timestruc_t *)&pup->pr_ttime);
	hrt2ts(pup->pr_tftime,	(timestruc_t *)&pup->pr_tftime);
	hrt2ts(pup->pr_dftime,	(timestruc_t *)&pup->pr_dftime);
	hrt2ts(pup->pr_kftime,	(timestruc_t *)&pup->pr_kftime);
	hrt2ts(pup->pr_ltime,	(timestruc_t *)&pup->pr_ltime);
	hrt2ts(pup->pr_slptime,	(timestruc_t *)&pup->pr_slptime);
	hrt2ts(pup->pr_wtime,	(timestruc_t *)&pup->pr_wtime);
	hrt2ts(pup->pr_stoptime, (timestruc_t *)&pup->pr_stoptime);
}

/*
 * Determine whether a set is empty.
 */
int
setisempty(uint32_t *sp, u_int n)
{
	while (n--)
		if (*sp++)
			return (0);
	return (1);
}

/*
 * Utility routine for establishing a watched area in the process.
 * Keep the list of watched areas sorted by virtual address.
 */
int
set_watched_area(proc_t *p,
	struct watched_area *pwa,
	struct watched_page *pwplist)
{
	caddr_t vaddr = pwa->wa_vaddr;
	caddr_t eaddr = pwa->wa_eaddr;
	u_int flags = pwa->wa_flags;
	struct watched_area *successor;
	int error = 0;

	/* we must not be holding p->p_lock, but the process must be locked */
	ASSERT(MUTEX_NOT_HELD(&p->p_lock));
	ASSERT(p->p_flag & SPRLOCK);

	if ((successor = p->p_warea) == NULL) {
		kthread_t *t;

		ASSERT(p->p_nwarea == 0);
		p->p_nwarea = 1;
		p->p_warea = pwa->wa_forw = pwa->wa_back = pwa;
		mutex_enter(&p->p_lock);
		if ((t = p->p_tlist) != NULL) {
			do {
				t->t_proc_flag |= TP_WATCHPT;
			} while ((t = t->t_forw) != p->p_tlist);
		}
		mutex_exit(&p->p_lock);
	} else {
		ASSERT(p->p_nwarea > 0);
		do {
			if (successor->wa_eaddr <= vaddr)
				continue;
			if (successor->wa_vaddr >= eaddr)
				break;
			/*
			 * We discovered an existing, overlapping watched area.
			 * Allow it only if it is an exact match.
			 */
			if (successor->wa_vaddr != vaddr ||
			    successor->wa_eaddr != eaddr)
				error = EINVAL;
			else if (successor->wa_flags != flags) {
				error = set_watched_page(p->p_as, vaddr, eaddr,
				    flags, successor->wa_flags, pwplist);
				successor->wa_flags = flags;
			}
			kmem_free(pwa, sizeof (struct watched_area));
			return (error);
		} while ((successor = successor->wa_forw) != p->p_warea);

		if (p->p_nwarea >= prnwatch) {
			kmem_free(pwa, sizeof (struct watched_area));
			return (E2BIG);
		}
		p->p_nwarea++;
		insque(pwa, successor->wa_back);
		if (p->p_warea->wa_vaddr > vaddr)
			p->p_warea = pwa;
	}
	return (set_watched_page(p->p_as, vaddr, eaddr, flags, 0, pwplist));
}

/*
 * Utility routine for clearing a watched area in the process.
 * Must be an exact match of the virtual address.
 * size and flags don't matter.
 */
int
clear_watched_area(proc_t *p, struct watched_area *pwa)
{
	caddr_t vaddr = pwa->wa_vaddr;

	/* we must not be holding p->p_lock, but the process must be locked */
	ASSERT(MUTEX_NOT_HELD(&p->p_lock));
	ASSERT(p->p_flag & SPRLOCK);

	kmem_free(pwa, sizeof (struct watched_area));

	if ((pwa = p->p_warea) == NULL)
		return (0);

	/*
	 * Look for a matching address in the watched areas.
	 * If a match is found, clear the old watched area
	 * and adjust the watched page(s).
	 * It is not an error if there is no match.
	 */
	do {
		if (pwa->wa_vaddr == vaddr) {
			p->p_nwarea--;
			if (p->p_warea == pwa)
				p->p_warea = pwa->wa_forw;
			if (p->p_warea == pwa) {
				p->p_warea = NULL;
				ASSERT(p->p_nwarea == 0);
			} else {
				remque(pwa);
				ASSERT(p->p_nwarea > 0);
			}
			clear_watched_page(p->p_as, pwa->wa_vaddr,
			    pwa->wa_eaddr, pwa->wa_flags);
			kmem_free(pwa, sizeof (struct watched_area));
			break;
		}
	} while ((pwa = pwa->wa_forw) != p->p_warea);

	if (p->p_warea == NULL) {
		kthread_t *t;

		mutex_enter(&p->p_lock);
		if ((t = p->p_tlist) != NULL) {
			do {
				t->t_proc_flag &= ~TP_WATCHPT;
			} while ((t = t->t_forw) != p->p_tlist);
		}
		mutex_exit(&p->p_lock);
	}

	return (0);
}

/*
 * Utility routine for deallocating a linked list of watched_area structs.
 */
void
pr_free_watchlist(struct watched_area *pwa)
{
	struct watched_area *delp;

	while (pwa != NULL) {
		delp = pwa;
		if ((pwa = pwa->wa_back) == delp)
			pwa = NULL;
		else
			remque(delp);
		kmem_free(delp, sizeof (struct watched_area));
	}
}

/*
 * Utility routines for deallocating a linked list of watched_page structs.
 * This one just deallocates the structures.
 */
void
pr_free_pagelist(struct watched_page *pwp)
{
	struct watched_page *delp;

	while (pwp != NULL) {
		delp = pwp;
		if ((pwp = pwp->wp_back) == delp)
			pwp = NULL;
		else
			remque(delp);
		kmem_free(delp, sizeof (struct watched_page));
	}
}

/*
 * This one is called by the traced process to unwatch all the
 * pages while deallocating the list of watched_page structs.
 */
void
pr_free_my_pagelist()
{
	struct as *as = curproc->p_as;
	struct watched_page *pwp;
	struct watched_page *delp;
	u_int prot;

	ASSERT(MUTEX_NOT_HELD(&curproc->p_lock));
	AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);

	pwp = as->a_wpage;
	as->a_wpage = NULL;
	as->a_nwpage = 0;

	while (pwp != NULL) {
		delp = pwp;
		if ((pwp = pwp->wp_back) == delp)
			pwp = NULL;
		else
			remque(delp);
		if ((prot = delp->wp_oprot) != 0) {
			caddr_t addr = delp->wp_vaddr;
			struct seg *seg;

			if ((delp->wp_prot != prot ||
			    (delp->wp_flags & WP_NOWATCH)) &&
			    (seg = as_segat(as, addr)) != NULL)
				(void) SEGOP_SETPROT(seg, addr, PAGESIZE, prot);
		}
		kmem_free(delp, sizeof (struct watched_page));
	}

	AS_LOCK_EXIT(as, &as->a_lock);
}

/*
 * Insert a watched area into the list of watched pages.
 * If oflags is zero then we are adding a new watched area.
 * Otherwise we are changing the flags of an existing watched area.
 */
static int
set_watched_page(struct as *as, caddr_t vaddr, caddr_t eaddr,
	u_long flags, u_long oflags, struct watched_page *pwplist)
{
	struct watched_page *pwp;
	struct watched_page *pnew;
	u_long npage;
	struct seg *seg;
	u_int prot;

	AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);

	/*
	 * Search for an existing watched page to contain the watched area.
	 * If none is found, grab a new one from the available list
	 * and insert it in the active list, keeping the list sorted
	 * by user-level virtual address.
	 */
again:
	if ((pwp = as->a_wpage) == NULL) {
		ASSERT(as->a_nwpage == 0);
		pwp = pwplist->wp_forw;
		remque(pwp);
		as->a_wpage = pwp->wp_forw = pwp->wp_back = pwp;
		as->a_nwpage = 1;
		pwp->wp_vaddr = (caddr_t)((u_long)vaddr & PAGEMASK);
	} else {
		ASSERT(as->a_nwpage != 0);
		for (npage = as->a_nwpage; npage != 0;
		    npage--, pwp = pwp->wp_forw) {
			if (pwp->wp_vaddr > vaddr) {
				npage = 0;
				break;
			}
			if (pwp->wp_vaddr + PAGESIZE > vaddr)
				break;
		}
		/*
		 * If we exhaust the loop then pwp == as->a_wpage
		 * and we will be adding a new element at the end.
		 * If we get here from the 'break' with npage = 0
		 * then we will be inserting a new element before
		 * the page whose uaddr exceeds vaddr.  Both of these
		 * conditions keep the list sorted by user virtual address.
		 */
		if (npage == 0) {
			pnew = pwplist->wp_forw;
			remque(pnew);
			insque(pnew, pwp->wp_back);
			pwp = pnew;
			as->a_nwpage++;
			pwp->wp_vaddr = (caddr_t)((u_long)vaddr & PAGEMASK);
			/*
			 * If we inserted a new page at the head of
			 * the list then reset the list head pointer.
			 */
			if (as->a_wpage->wp_vaddr > pwp->wp_vaddr)
				as->a_wpage = pwp;
		}
	}

	if (oflags & WA_READ)
		pwp->wp_read--;
	if (oflags & WA_WRITE)
		pwp->wp_write--;
	if (oflags & WA_EXEC)
		pwp->wp_exec--;

	ASSERT(pwp->wp_read >= 0);
	ASSERT(pwp->wp_write >= 0);
	ASSERT(pwp->wp_exec >= 0);

	if (flags & WA_READ)
		pwp->wp_read++;
	if (flags & WA_WRITE)
		pwp->wp_write++;
	if (flags & WA_EXEC)
		pwp->wp_exec++;

	vaddr = pwp->wp_vaddr;
	if (pwp->wp_oprot == 0 &&
	    (seg = as_segat(as, vaddr)) != NULL) {
		SEGOP_GETPROT(seg, vaddr, 0, &prot);
		pwp->wp_oprot = prot;
		pwp->wp_prot = prot;
	}
	if (pwp->wp_oprot != 0) {
		prot = pwp->wp_oprot;
		if (pwp->wp_read)
			prot &= ~(PROT_READ|PROT_WRITE|PROT_EXEC);
		if (pwp->wp_write)
			prot &= ~PROT_WRITE;
		if (pwp->wp_exec)
			prot &= ~(PROT_READ|PROT_WRITE|PROT_EXEC);
		if (!(pwp->wp_flags & WP_NOWATCH) && pwp->wp_prot != prot)
			pwp->wp_flags |= WP_SETPROT;
		pwp->wp_prot = prot;
	}

	/*
	 * If the watched area extends into the next page then do
	 * it over again with the virtual address of the next page.
	 */
	if ((vaddr = pwp->wp_vaddr + PAGESIZE) < eaddr)
		goto again;

	AS_LOCK_EXIT(as, &as->a_lock);
	if (as->a_nwpage > prnwatch)
		return (E2BIG);
	return (0);
}

/*
 * Remove a watched area from the list of watched pages.
 * A watched area may extend over more than one page.
 */
static void
clear_watched_page(struct as *as, caddr_t vaddr, caddr_t eaddr, u_long flags)
{
	struct watched_page *pwp = as->a_wpage;
	u_long npage = as->a_nwpage;

	ASSERT(npage != 0 && pwp != NULL);

	AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);

	while (npage-- != 0) {
		if (pwp->wp_vaddr >= eaddr)
			break;
		if (pwp->wp_vaddr + PAGESIZE <= vaddr) {
			pwp = pwp->wp_forw;
			continue;
		}

		if (flags & WA_READ)
			pwp->wp_read--;
		if (flags & WA_WRITE)
			pwp->wp_write--;
		if (flags & WA_EXEC)
			pwp->wp_exec--;

		if (pwp->wp_read + pwp->wp_write + pwp->wp_exec != 0) {
			/*
			 * Reset the hat layer's protections on this page.
			 */
			if (pwp->wp_oprot != 0) {
				u_int prot = pwp->wp_oprot;

				if (pwp->wp_read)
					prot &=
					    ~(PROT_READ|PROT_WRITE|PROT_EXEC);
				if (pwp->wp_write)
					prot &= ~PROT_WRITE;
				if (pwp->wp_exec)
					prot &=
					    ~(PROT_READ|PROT_WRITE|PROT_EXEC);
				if (!(pwp->wp_flags & WP_NOWATCH) &&
				    pwp->wp_prot != prot)
					pwp->wp_flags |= WP_SETPROT;
				pwp->wp_prot = prot;
			}
			pwp = pwp->wp_forw;
		} else {
			/*
			 * No watched areas remain in this page.
			 * Reset everything to normal.
			 */
			if (pwp->wp_oprot != 0) {
				pwp->wp_flags |= WP_SETPROT;
				pwp->wp_prot = pwp->wp_oprot;
			}
			pwp = pwp->wp_forw;
		}
	}

	AS_LOCK_EXIT(as, &as->a_lock);
}

/*
 * Return the original protections for the specified page.
 */
static void
getwatchprot(struct as *as, caddr_t addr, u_int *prot)
{
	struct watched_page *pwp;

	if ((pwp = as->a_wpage) == NULL)
		return;

	ASSERT(AS_LOCK_HELD(as, &as->a_lock));

	do {
		if (addr < pwp->wp_vaddr)
			break;
		if (addr == pwp->wp_vaddr) {
			if (pwp->wp_oprot != 0)
				*prot = pwp->wp_oprot;
			break;
		}
	} while ((pwp = pwp->wp_forw) != as->a_wpage);
}

u_int
pr_getprot(struct seg *seg, int reserved, void **tmp,
	caddr_t *saddr, caddr_t *naddr)
{
	struct as *as = seg->s_as;
	struct segvn_data *svd;
	struct segdev_data *sdp;
	caddr_t addr;
	caddr_t eaddr;
	vnode_t *vp;
	vattr_t vattr;
	u_int prot;
	u_int nprot;
	int check_noreserve;
	extern struct seg_ops segdev_ops;	/* needs a header file */

	/*
	 * Even though we are performing a read-only operation, we
	 * must have acquired the address space writer lock because
	 * the as_setprot() function only acquires the readers lock.
	 * The reason as_setprot() does this is lost in mystification,
	 * but a lot of people think changing it to acquire the writer
	 * lock would severely impact some application's performance.
	 */
	ASSERT(AS_WRITE_HELD(as, &as->a_lock));

	addr = *saddr;
	eaddr = seg->s_base + seg->s_size;
	ASSERT(addr >= seg->s_base && addr < eaddr);

	/*
	 * Don't include MAP_NORESERVE pages in the address range
	 * unless their mappings have actually materialized.
	 * We cheat by knowing that segvn is the only segment
	 * driver that supports MAP_NORESERVE.
	 */
	check_noreserve = (!reserved && seg->s_ops == &segvn_ops &&
			(svd = (struct segvn_data *)seg->s_data) != NULL &&
			(svd->vp == NULL || svd->vp->v_type != VREG) &&
			(svd->flags & MAP_NORESERVE));

	/*
	 * Don't include pages in the address range that don't map
	 * to pages in the underlying mapped file, if one exists.
	 * We just have to adjust eaddr to match the size of the file.
	 */
	vattr.va_mask = AT_SIZE;
	if (!reserved &&
	    seg->s_ops == &segvn_ops &&
	    SEGOP_GETVP(seg, addr, &vp) == 0 &&
	    vp != NULL && vp->v_type == VREG &&
	    VOP_GETATTR(vp, &vattr, 0, CRED()) == 0) {
		u_offset_t size = vattr.va_size;
		u_offset_t offset = SEGOP_GETOFFSET(seg, addr);

		if (size < offset)
			size = 0;
		else
			size -= offset;
		size = roundup(size, (u_offset_t)PAGESIZE);
		if (size < (u_offset_t)seg->s_size)
			eaddr = seg->s_base + size;

		/*
		 * If we ended up with no pages, set both return addresses to
		 * the end of the segment.  Loops in the callers expect this.
		 */
		if (addr >= eaddr) {
			addr = seg->s_base + seg->s_size;
			*saddr = addr;
			prot = 0;
			goto out;
		}
	}

	/*
	 * Don't include segments mapped from /dev/null.
	 * These simply reserve address ranges and have no memory.
	 * The key is that the mapping comes from segdev and the
	 * type is neither MAP_SHARED nor MAP_PRIVATE.
	 */
	if (!reserved &&
	    seg->s_ops == &segdev_ops &&
	    SEGOP_GETTYPE(seg, addr) == 0) {
		addr = seg->s_base + seg->s_size;
		*saddr = addr;
		prot = 0;
		goto out;
	}

	/*
	 * Examine every page only as a last resort.
	 * We use guilty knowledge of segvn and segdev to avoid this.
	 */
	if (!check_noreserve &&
	    addr == seg->s_base &&
	    seg->s_ops == &segvn_ops &&
	    (svd = (struct segvn_data *)seg->s_data) != NULL &&
	    svd->pageprot == 0) {
		prot = svd->prot;
		getwatchprot(as, addr, &prot);
		addr = eaddr;
	} else if (!check_noreserve &&
	    addr == seg->s_base &&
	    seg->s_ops == &segdev_ops &&
	    (sdp = (struct segdev_data *)seg->s_data) != NULL &&
	    sdp->pageprot == 0) {
		prot = sdp->prot;
		getwatchprot(as, addr, &prot);
		addr = eaddr;
	} else {
		/*
		 * Get the incore and prot tables once.
		 * Pass the pointer back to the caller, who will pass it back
		 * here on the next iteration, until the end of the segment
		 * is reached, at which time we free the memory.
		 */
		size_t npages = seg_pages(seg);
		u_int *protv;
		char *incore;
		u_int pn;

		if (addr == seg->s_base) {
			size_t size = npages * sizeof (u_int);
			if (check_noreserve)
				size += npages * sizeof (char);
			protv = kmem_alloc(size, KM_SLEEP);
			incore = (char *)(protv + npages);
			ASSERT(*tmp == NULL);
			*tmp = protv;
			SEGOP_GETPROT(seg, addr, seg->s_size - 1, protv);
			if (check_noreserve)
				SEGOP_INCORE(seg, addr, seg->s_size, incore);
		} else {
			protv = (u_int *)*tmp;
			incore = (char *)(protv + npages);
		}

		if (check_noreserve) {
			/*
			 * Find the first MAP_NORESERVE page with backing store.
			 */
			pn = seg_page(seg, addr);
			do {
				/*
				 * Guilty knowledge here.  We know that
				 * segvn_incore returns more than just the
				 * low-order bit that indicates the page is
				 * actually in memory.  If any bits are set,
				 * then there is backing store for the page.
				 */
				if (incore[pn++])
					break;
			} while ((addr += PAGESIZE) < eaddr);

			*saddr = addr;
			if (addr == eaddr) {
				prot = 0;
				goto out;
			}
		}

		pn = seg_page(seg, addr);
		prot = protv[pn];
		getwatchprot(as, addr, &prot);

		while ((addr += PAGESIZE) < eaddr) {
			nprot = protv[++pn];
			getwatchprot(as, addr, &nprot);
			if (nprot != prot)
				break;
			/*
			 * Stop on the first MAP_NORESERVE page
			 * that has no backing store.
			 */
			if (check_noreserve && !incore[pn])
				break;
		}
	}

out:
	if (addr == seg->s_base + seg->s_size && *tmp != NULL) {
		size_t npages = seg_pages(seg);
		size_t size = npages * sizeof (u_int);
		if (check_noreserve)
			size += npages * sizeof (char);
		kmem_free(*tmp, size);
		*tmp = NULL;
	}
	*naddr = addr;
	return (prot);
}
