/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
#pragma ident "@(#)syscall.c	1.58	96/10/17 SMI"

#include <sys/param.h>
#include <sys/vmparam.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/stack.h>
#include <sys/cred.h>
#include <sys/cmn_err.h>
#include <sys/user.h>
#include <sys/psw.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/modctl.h>
#include <sys/var.h>
#include <sys/inline.h>
#include <sys/syscall.h>
#include <sys/ucontext.h>
#include <sys/cpuvar.h>
#include <sys/siginfo.h>
#include <sys/trap.h>
#include <sys/vtrace.h>
#include <sys/sysinfo.h>
#include <sys/procfs.h>
#include <c2/audit.h>
#include <sys/modctl.h>
#include <sys/aio_impl.h>
#include <sys/tnf.h>
#include <sys/tnf_probe.h>


typedef	longlong_t (*llfcn_t)();	/* function returning long long */

int pre_syscall(void);
void post_syscall(int rval1, int rval2);
static krwlock_t *lock_syscall(uint_t code);

#ifdef SYSCALLTRACE
int syscalltrace = 0;
#else
#define	syscalltrace 0
#endif /* SYSCALLTRACE */

/*
 * Arrange for the real time profiling signal to be dispatched.
 */
void
realsigprof(int sysnum, int error)
{
	register proc_t *p;
	register klwp_t *lwp;

	if (curthread->t_rprof->rp_anystate == 0)
		return;
	p = ttoproc(curthread);
	lwp = ttolwp(curthread);
	mutex_enter(&p->p_lock);
	if (sigismember(&p->p_ignore, SIGPROF) ||
	    sigismember(&curthread->t_hold, SIGPROF)) {
		mutex_exit(&p->p_lock);
		return;
	}
	lwp->lwp_siginfo.si_signo = SIGPROF;
	lwp->lwp_siginfo.si_code = PROF_SIG;
	lwp->lwp_siginfo.si_errno = error;
	hrt2ts(gethrtime(), &lwp->lwp_siginfo.si_tstamp);
	lwp->lwp_siginfo.si_syscall = sysnum;
	lwp->lwp_siginfo.si_nsysarg =
	    (sysnum > 0 && sysnum < NSYSCALL) ? sysent[sysnum].sy_narg : 0;
	lwp->lwp_siginfo.si_fault = lwp->lwp_lastfault;
	lwp->lwp_siginfo.si_faddr = lwp->lwp_lastfaddr;
	lwp->lwp_lastfault = 0;
	lwp->lwp_lastfaddr = NULL;
	sigtoproc(p, curthread, SIGPROF, 0);
	mutex_exit(&p->p_lock);
	ASSERT(lwp->lwp_cursig == 0);
	if (issig(FORREAL))
		psig();
	mutex_enter(&p->p_lock);
	lwp->lwp_siginfo.si_signo = 0;
	bzero((caddr_t)curthread->t_rprof, sizeof (*curthread->t_rprof));
	mutex_exit(&p->p_lock);
}

int
pre_syscall()
{
	register kthread_id_t t = curthread;
	register unsigned code = t->t_sysnum;
	register int error = 0;
	register klwp_t *lwp = ttolwp(t);
	register proc_t *p = ttoproc(t);
	register struct regs *rp = lwptoregs(lwp);
	int	repost = 0;

	t->t_pre_sys = 0;

	ASSERT(t->t_schedflag & TS_DONT_SWAP);

	if (t->t_proc_flag & TP_MSACCT) {
		(void) new_mstate(t, LMS_SYSTEM);	/* in syscall */
		repost = 1;
	}
	/*
	 * The syscall arguments in the registers should be pointed to
	 * by lwp_ap.  If the args need to be copied so that the regs can
	 * be changed without losing the ability to get the args for /proc,
	 * they can be saved by save_syscall_args(), and lwp_ap will be
	 * restored by post_syscall().
	 */
	ASSERT(lwp->lwp_ap == &rp->r_r3);

	/*
	 * Make sure the thread is holding the latest credentials for the
	 * process.  The credentials in the process right now apply to this
	 * thread for the entire system call.
	 */
	if (t->t_cred != p->p_cred) {
		crfree(t->t_cred);
		t->t_cred = crgetcred();
	}

	if (lwp->lwp_prof.pr_scale) {
		lwp->lwp_scall_start = lwp->lwp_stime;
		t->t_flag |= T_SYS_PROF;
		t->t_post_sys = 1;	/* make sure post_syscall runs */
		repost = 1;		/* make sure pre_syscall runs again */
	}

	/*
	 * From the proc(4) manual page:
	 * When entry to a system call is being traced, the traced process
	 * stops after having begun the call to the system but before the
	 * system call arguments have been fetched from the process.
	 */
	if (PTOU(p)->u_systrap) {
		if (prismember(&PTOU(p)->u_entrymask, code)) {
			mutex_enter(&p->p_lock);
			/*
			 * Recheck stop condition, now that lock is held.
			 */
			if (PTOU(p)->u_systrap &&
			    prismember(&PTOU(p)->u_entrymask, code)) {
				stop(PR_SYSENTRY, code);
				/*
				 * Must refetch args since they were possibly
				 * modified by /proc.  Indicate that the valid
				 * copy is in the registers.
				 */
				lwp->lwp_ap = &rp->r_r3;
			}
			mutex_exit(&p->p_lock);
		}
		repost = 1;
	}

	if (lwp->lwp_sysabort) {
		/*
		 * lwp_sysabort may have been set via /proc while the process
		 * was stopped on PR_SYSENTRY.  If so, abort the system call.
		 * Override any error from the copyin() of the arguments.
		 */
		lwp->lwp_sysabort = 0;
		(void) set_errno(EINTR);
		t->t_pre_sys = 1;	/* repost anyway */
		return (1);		/* don't do system call, return EINTR */
	}

#ifdef C2_AUDIT
	if (audit_active) {	/* begin auditing for this syscall */
		audit_start(T_SYSCALL, code, error, lwp);
		repost = 1;
	}
#endif /* C2_AUDIT */

#ifndef NPROBE
	/* Kernel probe */
	if (tnf_tracing_active) {
		TNF_PROBE_1(syscall_start, "syscall thread", /* CSTYLED */,
			tnf_sysnum,     sysnum,         t->t_sysnum);
		t->t_post_sys = 1;	/* make sure post_syscall runs */
		repost = 1;
	}
#endif /* NPROBE */

#ifdef SYSCALLTRACE
	if (error == 0 && syscalltrace) {
		register int i;
		char *cp;
		char *sysname;
		struct sysent *callp;

		if (code >= NSYSCALL) {
			callp = &nosys_ent;	/* nosys has no args */
		} else {
			callp = &sysent[code];
		}
		(void) save_syscall_args();
		printf("%d: ", p->p_pid);
		if (code >= NSYSCALL)
			printf("0x%x", code);
		else
			sysname = mod_getsysname(code);
			printf("%s[0x%x]", sysname == NULL ? "NULL" :
			    sysname, code);
		cp = "(";
		for (i = 0; i < callp->sy_narg; i++) {
			printf("%s%x", cp, lwp->lwp_ap[i]);
			cp = ", ";
		}
		if (i)
			printf(")");
		printf(" %s", PTOU(p)->u_comm);
		printf("\n");
	}
#endif /* SYSCALLTRACE */

	/*
	 * If there was a continuing reason for pre-syscall processing,
	 * set the t_pre_sys flag for the next system call.
	 */
	if (repost)
		t->t_pre_sys = 1;
	lwp->lwp_error = 0;	/* for old drivers */
	return (error);
}


/*
 * Post-syscall processing.  Perform abnormal system call completion
 * actions such as /proc tracing, profiling, signals, preemption, etc.
 *
 * This routine is called only if the t_post_sys flag or t_astflag is set.
 * Any condition requiring pre-syscall handling must set one of these.
 * If the condition is persistent, this routine will repost t_post_sys.
 */
void
post_syscall(int rval1, int rval2)
{
	register kthread_id_t t = curthread;
	register klwp_t *lwp = ttolwp(t);
	register proc_t *p = ttoproc(t);
	register struct regs *rp = lwptoregs(lwp);
	uint_t	code = t->t_sysnum;
	uint_t	error = lwp->lwp_errno;
	int	proc_stop = 0;		/* non-zero if stopping for /proc */
	int	sigprof = 0;		/* non-zero if sending SIGPROF */
	int	repost = 0;

	t->t_post_sys = 0;

	/*
	 * Code can be zero if this is a new LWP returning after a fork(),
	 * other than the one which matches the one in the parent which called
	 * fork.  In these LWPs, skip most of post-syscall activity.
	 */
	if (code == 0)
		goto sig_check;
#ifdef C2_AUDIT
	if (audit_active) {	/* put out audit record for this syscall */
		rval_t	rval;

		rval.r_val1 = rval1;
		rval.r_val2 = rval2;
		audit_finish(T_SYSCALL, code, error, &rval);
		repost = 1;
	}
#endif /* C2_AUDIT */

	/*
	 * If we're going to stop for /proc tracing, set the flag and
	 * save the arguments so that the return values don't smash them.
	 */
	if (PTOU(p)->u_systrap) {
		if (prismember(&PTOU(p)->u_exitmask, code)) {
			proc_stop = 1;
			(void) save_syscall_args();
		}
		repost = 1;
	}

	/*
	 * Similarly check to see if SIGPROF might be sent.
	 */
	if (curthread->t_rprof != NULL &&
	    curthread->t_rprof->rp_anystate != 0) {
		save_syscall_args();
		sigprof = 1;
	}

	if (lwp->lwp_eosys == NORMALRETURN) {

		if (error) {
			int sig;
#ifdef SYSCALLTRACE
			if (syscalltrace)
				printf("%d: error=%d\n", p->p_pid, error);
#endif /* SYSCALLTRACE */
			if (error == EINTR &&
			    (sig = lwp->lwp_cursig) != 0 &&
			    sigismember(&PTOU(p)->u_sigrestart, sig) &&
			    PTOU(p)->u_signal[sig - 1] != SIG_DFL &&
			    PTOU(p)->u_signal[sig - 1] != SIG_IGN)
				error = ERESTART;
			rp->r_r3 = error;
			rp->r_cr |= CR0_SO;	/* failure */
		} else {
#ifdef SYSCALLTRACE
			if (syscalltrace)
				printf("%d: r_val1=0x%x, r_val2=0x%x\n",
				    p->p_pid, rval1, rval2);
#endif /* SYSCALLTRACE */
			rp->r_cr &= ~CR0_SO;	/* success */
			rp->r_r3 = rval1;
			rp->r_r4 = rval2;
		}
	}
	lwp->lwp_eosys = NORMALRETURN;	/* reset flag for next time */

	/*
	 * From the proc(4) manual page:
	 * When exit from a system call is being traced, the traced process
	 * stops on completion of the system call just prior to checking for
	 * signals and returning to user level.  At this point all return
	 * values have been stored into the traced process's saved registers.
	 */
	if (proc_stop) {
		mutex_enter(&p->p_lock);
		/*
		 * Recheck stop conditions now that p_lock is held.
		 */
		if (PTOU(p)->u_systrap &&
		    prismember(&PTOU(p)->u_exitmask, code)) {
			stop(PR_SYSEXIT, code);
		}
		mutex_exit(&p->p_lock);
	}

	/*
	 * If we are the parent returning from a successful
	 * vfork, wait for the child to exec or exit.
	 * This code must be here and not in the bowels of the system
	 * so that /proc can intercept exit from vfork in a timely way.
	 */
	if (code == SYS_vfork && rp->r_r4 == 0 && error == 0)
		vfwait((pid_t)rval1);

	/*
	 * If profiling was active at the start of this system call, and
	 * is still active, add the number of ticks spent in the call to
	 * the time spent at the current PC in user-land.
	 */
	if ((t->t_flag & T_SYS_PROF) != 0) {
		t->t_flag &= ~T_SYS_PROF;
		if (lwp->lwp_prof.pr_scale) {
			register int ticks;

			ticks = lwp->lwp_stime - lwp->lwp_scall_start;
			if (ticks) {
				mutex_enter(&p->p_pflock);
				addupc((void(*)())rp->r_pc, &lwp->lwp_prof,
					ticks);
				mutex_exit(&p->p_pflock);
			}
		}
	}

	t->t_sysnum = 0;		/* no longer in a system call */

sig_check:
	if (t->t_astflag | t->t_sig_check) {
		/*
		 * Turn off the AST flag before checking all the conditions that
		 * may have caused an AST.  This flag is on whenever a signal or
		 * unusual condition should be handled after the next trap or
		 * syscall.
		 */
		astoff(t);
		t->t_sig_check = 0;

		/*
		 * for kaio requests that are on the per-process poll queue,
		 * aiop->aio_pollq, they're AIO_POLL bit is set, the kernel
		 * should copyout their result_t to user memory. by copying
		 * out the result_t, the user can poll on memory waiting
		 * for the kaio request to complete.
		 */
		if (p->p_aio)
			aio_cleanup(0);

		/*
		 * If this LWP was asked to hold, call holdlwp(), which will
		 * stop.  holdlwps() sets this up and calls pokelwps() which
		 * sets the AST flag.
		 *
		 * Also check TP_EXITLWP, since this is used by fresh new LWPs
		 * through lwp_rtt().  That flag is set if the lwp_create(2)
		 * syscall failed after creating the LWP.
		 */
		if (ISHOLD(p) || (t->t_proc_flag & TP_EXITLWP))
			holdlwp();

		/*
		 * All code that sets signals and makes ISSIG_PENDING
		 * evaluate true must set t_astflag afterwards.
		 */
		if (ISSIG_PENDING(t, lwp, p)) {
			if (issig(FORREAL))
				psig();
			t->t_sig_check = 1;	/* recheck next time */
		}

		if (sigprof) {
			realsigprof(code, error);
			t->t_sig_check = 1;	/* recheck next time */
		}
	}

#ifndef NPROBE
	/* Kernel probe */
	if (tnf_tracing_active) {
		TNF_PROBE_3(syscall_end, "syscall thread", /* CSTYLED */,
			tnf_long,	rval1,	rval1,
			tnf_long,	rval2,	rval2,
			tnf_long,	errno,	(long)error);
		repost = 1;
	}
#endif /* NPROBE */


	/*
	 * Set state to LWP_USER here so preempt won't give us a kernel
	 * priority if it occurs after this point.  Call CL_TRAPRET() to
	 * restore the user-level priority.
	 *
	 * It is important that no locks (other than spinlocks) be entered
	 * after this point before returning to user mode (unless lwp_state
	 * is set back to LWP_SYS).
	 *
	 * XXX Sampled times past this point are charged to the user.
	 */
	lwp->lwp_state = LWP_USER;
	if (t->t_proc_flag & TP_MSACCT) {
		(void) new_mstate(t, LMS_USER);
		repost = 1;
	}
	if (t->t_trapret) {
		t->t_trapret = 0;
		thread_lock(t);
		CL_TRAPRET(t);
		thread_unlock(t);
	}
	if (CPU->cpu_runrun)
		preempt();

	/*
	 * In case the args were copied to the lwp, reset the pointer so
	 * the next syscall will have the right lwp_ap pointer.
	 */
	lwp->lwp_ap = &rp->r_r3;

	lwp->lwp_errno = 0;		/* clear error for next time */

	/*
	 * If there was a continuing reason for post-syscall processing,
	 * set the t_post_sys flag for the next system call.
	 */
	if (repost)
		t->t_post_sys = 1;
}

/*
 * nonexistent system call-- signal lwp (may want to handle it)
 * flag error if lwp won't see signal immediately
 */
longlong_t
nosys()
{
	psignal(ttoproc(curthread), SIGSYS);
	return ((longlong_t) set_errno(ENOSYS));
}


/*
 * Get the arguments to the current system call.
 *	lwp->lwp_ap normally points to %r3-%r10 in the reg structure.
 *	If the user is going to change those registers and might want
 *	to get the args (for /proc tracing), it must copy the args elsewhere
 *	via save_syscall_args().
 */
uint_t
get_syscall_args(klwp_t *lwp, int *argp, int *nargsp)
{
	kthread_id_t	t = lwptot(lwp);
	uint_t	code = t->t_sysnum;
	int	*ap;
	int	nargs;

	if (code != 0 && code < NSYSCALL) {
		nargs = sysent[code].sy_narg;
		ASSERT(nargs <= MAXSYSARGS);

		*nargsp = nargs;
		ap = lwp->lwp_ap;
		while (nargs-- > 0)
			*argp++ = *ap++;
	} else {
		*nargsp = 0;
	}
	return (code);
}

/*
 * Save_syscall_args - copy the users args prior to changing the stack or
 * stack pointer.  This is so /proc will be able to get a valid copy of the
 * args from the user stack even after the user stack has been changed.
 * Note that the kernel stack copy of the args may also have been changed by
 * a system call handler which takes C-style arguments.
 *
 * Note that this may be called by stop() from trap().  In that case t_sysnum
 * will be zero (syscall_exit clears it), so no args will be copied.
 */
int
save_syscall_args()
{
	kthread_id_t	t = curthread;
	klwp_t		*lwp = ttolwp(t);
	u_int		code = t->t_sysnum;
	u_int		nargs;

	if (lwp->lwp_ap == lwp->lwp_arg || code == 0)
		return (0);		/* args already saved or not needed */

	if (code >= NSYSCALL) {
		nargs = 0;		/* illegal syscall */
	} else {
		register struct sysent *callp;

		callp = &sysent[code];
		nargs = callp->sy_narg;
		if (LOADABLE_SYSCALL(callp) && nargs == 0) {
			krwlock_t	*module_lock;

			/*
			 * Find out how many arguments the system
			 * call uses.
			 *
			 * We have the property that loaded syscalls
			 * never change the number of arguments they
			 * use after they've been loaded once.  This
			 * allows us to stop for /proc tracing without
			 * holding the module lock.
			 * /proc is assured that sy_narg is valid.
			 */
			module_lock = lock_syscall(code);
			nargs = callp->sy_narg;
			rw_exit(module_lock);
		}

		/*
		 * Fetch the system call arguments.
		 */
		if (nargs != 0) {
			ASSERT(nargs <= MAXSYSARGS);

			/*
			 * sp points to the return addr on the user's
			 * stack. bump it up to the actual args.
			 */
			bcopy((caddr_t)lwp->lwp_ap, (caddr_t)lwp->lwp_arg,
			    nargs * sizeof (int));
		}
	}
	lwp->lwp_ap = lwp->lwp_arg;
	t->t_post_sys = 1;	/* so lwp_ap will be reset */
	return (0);
}

/*
 * Call a system call which takes a pointer to the user args struct and
 * a pointer to the return values.  This is a bit slower than the standard
 * C arg-passing method in some cases.
 */
longlong_t
syscall_ap()
{
	uint_t	error;
	struct sysent *callp;
	rval_t	rval;
	klwp_t	*lwp = ttolwp(curthread);
	struct regs *rp = lwptoregs(lwp);

	callp = &sysent[curthread->t_sysnum];

	rval.r_val1 = 0;
	rval.r_val2 = rp->r_r4;
	lwp->lwp_error = 0;	/* for old drivers */
	error = (*(callp->sy_call))(lwp->lwp_ap, &rval);
	if (error)
		return ((longlong_t)set_errno(error));
	return (rval.r_vals);
}

/*
 * Load system call module.
 *	Returns with pointer to held read lock for module.
 */
static krwlock_t *
lock_syscall(uint_t code)
{
	krwlock_t	*module_lock;
	int 		loaded = 1;
	struct sysent	*callp;
	extern char	**syscallnames;

	callp = &sysent[code];
	module_lock = callp->sy_lock;
	rw_enter(module_lock, RW_READER);
	while (!LOADED_SYSCALL(callp) && loaded) {
		/*
		 * Give up the lock so that the module can get
		 * a write lock when it installs itself
		 */
		rw_exit(module_lock);
		/* Returns zero on success */
		loaded = (modload("sys", syscallnames[code]) != -1);
		rw_enter(module_lock, RW_READER);
	}
	return (module_lock);
}

/*
 * Loadable syscall support.
 *	If needed, load the module, then reserve it by holding a read
 * 	lock for the duration of the call.
 *	Later, if the syscall is not unloadable, it could patch the vector.
 */
longlong_t
loadable_syscall(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7)
{
	longlong_t	rval;
	struct sysent	*callp;
	krwlock_t	*module_lock;
	int		code;

	code = curthread->t_sysnum;
	callp = &sysent[code];

	/*
	 * Try to autoload the system call if necessary.
	 */
	module_lock = lock_syscall(code);
	THREAD_KPRI_RELEASE();	/* drop priority given by rw_enter */

	/*
	 * we've locked either the loaded syscall or nosys
	 */
	if (callp->sy_flags & SE_ARGC) {
		longlong_t (*sy_call)();

		sy_call = (longlong_t (*)())callp->sy_call;
		rval = (*sy_call)(a0, a1, a2, a3, a4, a5, a6, a7);
	} else
		rval = syscall_ap();

	THREAD_KPRI_REQUEST();	/* regain priority from read lock */
	rw_exit(module_lock);
	return (rval);
}


/*
 * Indirect syscall handled in libc on PowerPC.
 */
longlong_t
indir()
{
	return (nosys());
}

/*
 * set_errno - set an error return from the current system call.
 *	This could be a macro.
 *	This returns the value it is passed, so that the caller can
 *	use tail-recursion-elimination and do return (set_errno(ERRNO));
 */
uint_t
set_errno(uint_t errno)
{
	ASSERT(errno != 0);		/* must not be used to clear errno */

	curthread->t_post_sys = 1;	/* have post_syscall do error return */
	return (ttolwp(curthread)->lwp_errno = errno);
}

/*
 * set_proc_pre_sys - Set pre-syscall processing for entire process.
 */
void
set_proc_pre_sys(proc_t *p)
{
	register kthread_id_t	t;
	register kthread_id_t	first;

	ASSERT(MUTEX_HELD(&p->p_lock));

	t = first = p->p_tlist;
	do {
		t->t_pre_sys = 1;
	} while ((t = t->t_forw) != first);
}

/*
 * set_proc_post_sys - Set post-syscall processing for entire process.
 */
void
set_proc_post_sys(proc_t *p)
{
	register kthread_id_t	t;
	register kthread_id_t	first;

	ASSERT(MUTEX_HELD(&p->p_lock));

	t = first = p->p_tlist;
	do {
		t->t_post_sys = 1;
	} while ((t = t->t_forw) != first);
}

/*
 * set_proc_sys - Set pre- and post-syscall processing for entire process.
 */
void
set_proc_sys(proc_t *p)
{
	register kthread_id_t	t;
	register kthread_id_t	first;

	ASSERT(MUTEX_HELD(&p->p_lock));

	t = first = p->p_tlist;
	do {
		t->t_pre_sys = 1;
		t->t_post_sys = 1;
	} while ((t = t->t_forw) != first);
}

/*
 * set_all_proc_sys - set pre- and post-syscall processing flags for all
 * user processes.
 *
 * This is needed when auditing, tracing, or other facilities which affect
 * all processes are turned on.
 */
void
set_all_proc_sys()
{
	register kthread_id_t	t;
	register kthread_id_t	first;

	mutex_enter(&pidlock);
	t = first = curthread;
	do {
		t->t_pre_sys = 1;
		t->t_post_sys = 1;
	} while ((t = t->t_next) != first);
	mutex_exit(&pidlock);
}

/*
 * set_proc_ast - Set asynchronous service trap (AST) flag for all
 * threads in process.
 */
void
set_proc_ast(proc_t *p)
{
	register kthread_id_t	t;
	register kthread_id_t	first;

	ASSERT(MUTEX_HELD(&p->p_lock));

	t = first = p->p_tlist;
	do {
		aston(t);
	} while ((t = t->t_forw) != first);
}
