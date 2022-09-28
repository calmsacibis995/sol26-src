/*
 * Copyright (c) 1993 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpr_uthread.c	1.17	96/10/17 SMI"

#include <sys/types.h>
#include <sys/thread.h>
#include <sys/conf.h>
#include <sys/cpuvar.h>
#include <sys/cpr.h>
#include <sys/user.h>
#include <sys/cmn_err.h>

extern void add_one_utstop(void);
extern void utstop_timedwait(long ticks);

static void cpr_stop_user(int);
static int cpr_check_user_threads(void);

/*
 * CPR user thread related support routines
 */
void
cpr_signal_user(int sig)
{
/*
 * The signal SIGTHAW and SIGFREEZE cannot be sent to every thread yet
 * since openwin is catching every signal and default action is to exit.
 * We also need to implement the true SIGFREEZE and SIGTHAW to stop threads.
 */
	struct proc *p;

	mutex_enter(&pidlock);

	for (p = practive; p; p = p->p_next) {
		/* only user threads */
		if (p->p_exec == NULL || p->p_stat == SZOMB ||
			p == proc_init || p == ttoproc(curthread))
			continue;

		mutex_enter(&p->p_lock);
		sigtoproc(p, NULL, sig, 0);
		mutex_exit(&p->p_lock);
	}
	mutex_exit(&pidlock);

	/* delay for now */
	delay(hz);
}

/* max wait time for user thread stop */
#define	CPR_UTSTOP_WAIT		hz
#define	CPR_UTSTOP_RETRY	4
static int count;

int
cpr_stop_user_threads()
{
	count = 0;
	do {
		if (++count > CPR_UTSTOP_RETRY)
			return (ESRCH);
		cpr_stop_user(count * count * CPR_UTSTOP_WAIT);
	} while (cpr_check_user_threads() &&
		(count < CPR_UTSTOP_RETRY || CPR->c_fcn != AD_CPR_FORCE));

	return (0);
}

/*
 * This routine tries to stop all user threads before we get rid of all
 * its pages.It goes through allthreads list and set the TP_CHKPT flag
 * for all user threads and make them runnable. If all of the threads
 * can be stopped within the max wait time, CPR will proceed. Otherwise
 * CPR is aborted after a few of similiar retries.
 */
static void
cpr_stop_user(int wait)
{
	kthread_id_t tp;
	proc_t *p;

	/* The whole loop below needs to be atomic */
	mutex_enter(&pidlock);

	/* faster this way */
	tp = curthread->t_next;
	do {
		/* kernel threads will be handled later */
		p = ttoproc(tp);
		if (p->p_as == &kas || p->p_stat == SZOMB)
			continue;

		/*
		 * If the thread is stopped (by CPR) already, do nothing;
		 * if running, mark TP_CHKPT;
		 * if sleeping normally, mark TP_CHKPT and setrun;
		 * if sleeping non-interruptable, mark TP_CHKPT only for now;
		 * if sleeping with t_wchan0 != 0 etc, virtually stopped,
		 * do nothing.
		 */

		/* p_lock is needed for modifying t_proc_flag */
		mutex_enter(&p->p_lock);
		thread_lock(tp); /* needed to check CPR_ISTOPPED */

		if (tp->t_state == TS_STOPPED) {
			/*
			 * if already stopped by other reasons, add this new
			 * reason to it.
			 */
			if (tp->t_schedflag & TS_RESUME)
				tp->t_schedflag &= ~TS_RESUME;
		} else {

			tp->t_proc_flag |= TP_CHKPT;

			/* only expect non-virtually stopped thread to stop */
#ifdef VSTOP_FIX
			/*
			 * VSTOP_FIX should be used when the problem for
			 * stoping virtually stopped threads is fixed in
			 * stop() of sig.c, where an if (!PR_CHECKPOINT)
			 * should be added for prstop(). It then fixes the
			 * core dump problem on vold upon resume.
			 * Due to the fact that we are replacing the uthread
			 * stoping mechanism with a new callback approach, we
			 * put the fix that would be in sig.c here.
			 */
			if (!CPR_VSTOPPED(tp)) {
#endif VSTOP_FIX
				thread_unlock(tp);
				mutex_exit(&p->p_lock);
				add_one_utstop();
				mutex_enter(&p->p_lock);
				thread_lock(tp);
#ifdef VSTOP_FIX
			}
#endif VSTOP_FIX
			aston(tp);

#ifdef VSTOP_FIX
			if (tp->t_state == TS_SLEEP &&
			    (tp->t_flag & T_WAKEABLE) &&
			    tp->t_wchan0 == 0) {
#else
			if (tp->t_state == TS_SLEEP &&
			    (tp->t_flag & T_WAKEABLE)) {
#endif VSTOP_FIX
				setrun_locked(tp);
			}

		}
#ifdef MP
		/*
		 * force the thread into the kernel if it is not already there.
		 */
		if (tp->t_state == TS_ONPROC && tp->t_cpu != CPU)
			poke_cpu(tp->t_cpu->cpu_id);
#endif /* MP */
		thread_unlock(tp);
		mutex_exit(&p->p_lock);

	} while ((tp = tp->t_next) != curthread);
	mutex_exit(&pidlock);

	utstop_timedwait(wait);
}

/*
 * Checks and makes sure all user threads are stopped
 */
static int
cpr_check_user_threads()
{
	kthread_id_t tp;
	int rc = 0;

	mutex_enter(&pidlock);
	tp = curthread->t_next;
	do {
		if (ttoproc(tp)->p_as == &kas || ttoproc(tp)->p_stat == SZOMB)
			continue;

		thread_lock(tp);
		/*
		 * make sure that we are off all the queues and in a stopped
		 * state.
		 */
#ifdef VSTOP_FIX
		if (!CPR_ISTOPPED(tp) && !CPR_VSTOPPED(tp)) {
#else
		if (!CPR_ISTOPPED(tp)) {
#endif VSTOP_FIX
			thread_unlock(tp);
			mutex_exit(&pidlock);

			if (count == CPR_UTSTOP_RETRY) {
			DEBUG1(errp("Suspend failed: cannt stop "
				"uthread\n"));
			cmn_err(CE_WARN, "suspend cannot stop "
				"process %s (%x:%x)",
				ttoproc(tp)->p_user.u_psargs, (int)tp,
				tp->t_state);
			cmn_err(CE_WARN, "process may be waiting for"
				" network request, please try again");
			}

			DEBUG2(errp("cant stop t=%x state=%x pfg=%x sched=%x\n",
			tp, tp->t_state, tp->t_proc_flag, tp->t_schedflag));
			DEBUG2(errp("proc %x state=%x pid=%d\n",
				ttoproc(tp), ttoproc(tp)->p_stat,
				ttoproc(tp)->p_pidp->pid_id));
			return (1);
		}
		if (CPR_VSTOPPED(tp))
			DEBUG2(errp("vstoped: tp=%x, proc=%x, user=%s\n",
				tp, ttoproc(tp), ttoproc(tp)->p_user.u_psargs));
		thread_unlock(tp);

	} while ((tp = tp->t_next) != curthread && rc == 0);

	mutex_exit(&pidlock);
	return (0);
}


/*
 * start all threads that were stopped for checkpoint.
 */
void
cpr_start_user_threads()
{
	kthread_id_t tp;
	proc_t *p;

	mutex_enter(&pidlock);
	tp = curthread->t_next;
	do {
		p = ttoproc(tp);
		/*
		 * kernel threads are callback'ed rather than setrun.
		 */
		if (ttoproc(tp)->p_as == &kas) continue;
		/*
		 * t_proc_flag should have been cleared. Just to make sure here
		 */
		mutex_enter(&p->p_lock);
		tp->t_proc_flag &= ~TP_CHKPT;
		mutex_exit(&p->p_lock);

		thread_lock(tp);
		if (CPR_ISTOPPED(tp)) {

			/*
			 * put it back on the runq
			 */
			tp->t_schedflag |= TS_RESUME;
			setrun_locked(tp);
		}
		thread_unlock(tp);
		/*
		 * DEBUG - Keep track of current and next thread pointer.
		 */
	} while ((tp = tp->t_next) != curthread);

	mutex_exit(&pidlock);

}
