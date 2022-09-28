/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prcontrol.c	1.4	96/10/28 SMI"

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/inline.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/regset.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/signal.h>
#include <sys/auxv.h>
#include <sys/user.h>
#include <sys/class.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/procfs.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <fs/proc/prdata.h>

typedef union {
	long		sig;		/* PCKILL, PCUNKILL */
	long		nice;		/* PCNICE */
	time_t		timeo;		/* PCTWSTOP */
	u_long		flags;		/* PCRUN, PCSET, PCUNSET */
	caddr_t		vaddr;		/* PCSVADDR */
	siginfo_t	siginfo;	/* PCSSIG */
	sigset_t	sigset;		/* PCSTRACE, PCSHOLD */
	fltset_t	fltset;		/* PCSFAULT */
	sysset_t	sysset;		/* PCSENTRY, PCSEXIT */
	prgregset_t	prgregset;	/* PCSREG */
	prfpregset_t	prfpregset;	/* PCSFPREG */
#if defined(sparc) || defined(__sparc)
	prxregset_t	prxregset;	/* PCSXREG */
#endif
	prwatch_t	prwatch;	/* PCWATCH */
} arg_t;

static	int	pr_control(long, arg_t *, prnode_t *, struct cred *);
static	kthread_t *pr_thread(prnode_t *);
static	int	allstopped(proc_t *);
static	void	pauselwps(proc_t *);
static	void	unpauselwps(proc_t *);

static unsigned
ctlsize(long cmd, unsigned resid)
{
	register unsigned size = sizeof (long);

	switch (cmd) {
	case PCNULL:
	case PCSTOP:
	case PCDSTOP:
	case PCWSTOP:
	case PCCSIG:
	case PCCFAULT:
		break;
	case PCSSIG:
		size += sizeof (siginfo_t);
		break;
	case PCTWSTOP:
		size += sizeof (time_t);
		break;
	case PCKILL:
	case PCUNKILL:
	case PCNICE:
		size += sizeof (long);
		break;
	case PCRUN:
	case PCSET:
	case PCUNSET:
		size += sizeof (u_long);
		break;
	case PCSVADDR:
		size += sizeof (caddr_t);
		break;
	case PCSTRACE:
	case PCSHOLD:
		size += sizeof (sigset_t);
		break;
	case PCSFAULT:
		size += sizeof (fltset_t);
		break;
	case PCSENTRY:
	case PCSEXIT:
		size += sizeof (sysset_t);
		break;
	case PCSREG:
		size += sizeof (prgregset_t);
		break;
	case PCSFPREG:
		size += sizeof (prfpregset_t);
		break;
#if defined(sparc) || defined(__sparc)
	case PCSXREG:
		size += sizeof (prxregset_t);
		break;
#endif
	case PCWATCH:
		size += sizeof (prwatch_t);
		break;
	default:
		return (0);
	}

	if (size > resid)
		return (0);
	return (size);
}

/*
 * Control operations (lots).
 */
int
prwritectl(vp, uiop, cr)
	struct vnode *vp;
	struct uio *uiop;
	struct cred *cr;
{
#define	MY_BUFFER_SIZE	100 > 1 + sizeof (arg_t) / sizeof (long) ? \
			100 : 1 + sizeof (arg_t) / sizeof (long)
	long buf[MY_BUFFER_SIZE];
	register long *bufp;
	register unsigned resid = 0;
	register unsigned size;
	register prnode_t *pnp = VTOP(vp);
	register int error;
	int locked = 0;

	while (uiop->uio_resid) {
		/*
		 * Read several commands in one gulp.
		 */
		bufp = buf;
		if (resid) {	/* move incomplete command to front of buffer */
			register long *tail;

			if (resid >= sizeof (buf))
				break;
			tail = (long *)((char *)buf + sizeof (buf) - resid);
			do {
				*bufp++ = *tail++;
			} while ((resid -= sizeof (long)) != 0);
		}
		resid = min(uiop->uio_resid,
		    sizeof (buf) - ((char *)bufp - (char *)buf));
		if (error = uiomove((caddr_t)bufp, resid, UIO_WRITE, uiop))
			return (error);
		resid += (char *)bufp - (char *)buf;
		bufp = buf;

		do {		/* loop over commands in buffer */
			register long cmd = bufp[0];
			register arg_t *argp = (arg_t *)&bufp[1];

			size = ctlsize(cmd, resid);
			if (size == 0)	/* incomplete or invalid command */
				break;
			/*
			 * Perform the specified control operation.
			 */
			if (!locked) {
				if ((error = prlock(pnp, ZNO)) != 0)
					return (error);
				locked = 1;
			}
			if (error = pr_control(cmd, argp, pnp, cr)) {
				if (error == -1)	/* -1 is timeout */
					locked = 0;
				else
					return (error);
			}
			bufp = (long *)((char *)bufp + size);
		} while ((resid -= size) != 0);

		if (locked) {
			prunlock(pnp);
			locked = 0;
		}
	}
	return (resid? EINVAL : 0);
}

static int
pr_control(cmd, argp, pnp, cr)
	long cmd;
	arg_t *argp;
	register prnode_t *pnp;
	struct cred *cr;
{
	register prcommon_t *pcp;
	register proc_t *p;
	register kthread_t *t;
	register int error = 0;
	register user_t *up;

	if (cmd == PCNULL)
		return (0);

	pcp = pnp->pr_common;
	p = pcp->prc_proc;
	ASSERT(p != NULL);

	switch (cmd) {

	default:
	    {
		error = EINVAL;
		break;
	    }

	case PCSTOP:	/* direct process or lwp to stop and wait for stop */
	case PCDSTOP:	/* direct process or lwp to stop, don't wait */
	    {
		/*
		 * Can't apply to a system process.
		 */
		if ((p->p_flag & SSYS) || p->p_as == &kas) {
			error = EBUSY;
			break;
		}

		/*
		 * If already stopped, do nothing; otherwise flag
		 * it to be stopped the next time it tries to run.
		 * If sleeping at interruptible priority, set it
		 * running so it will stop within cv_wait_sig().
		 *
		 * Take care to cooperate with jobcontrol: if an lwp
		 * is stopped due to the default action of a jobcontrol
		 * stop signal, flag it to be stopped the next time it
		 * starts due to a SIGCONT signal.
		 */
		if (pcp->prc_flags & PRC_LWP)
			t = pcp->prc_thread;
		else
			t = p->p_tlist;
		ASSERT(t != NULL);
		do {
			int notify;

			notify = 0;
			thread_lock(t);
			if (!ISTOPPED(t)) {
				t->t_proc_flag |= TP_PRSTOP;
				t->t_sig_check = 1;	/* do ISSIG */
			}
			if (t->t_state == TS_SLEEP &&
			    (t->t_flag & T_WAKEABLE)) {
				if (t->t_wchan0 == 0)
					setrun_locked(t);
				else if (!VSTOPPED(t)) {
					/*
					 * Mark it virtually stopped.
					 */
					t->t_proc_flag |= TP_PRVSTOP;
					notify = 1;
				}
			}
			/*
			 * force the thread into the kernel
			 * if it is not already there.
			 */
			prpokethread(t);
			thread_unlock(t);
			if (notify && t->t_trace)
				prnotify(t->t_trace);
			if (pcp->prc_flags & PRC_LWP)
				break;
		} while ((t = t->t_forw) != p->p_tlist);
		/*
		 * We do this just in case the thread we asked
		 * to stop is in holdlwps() (called from cfork()).
		 */
		cv_broadcast(&p->p_holdlwps);

		/*
		 * If an lwp is stopping itself or its process, don't wait.
		 * The lwp will never see the fact that it is stopped.
		 */
		if (cmd == PCDSTOP ||
		    ((pcp->prc_flags & PRC_LWP)? t == curthread : p == curproc))
			break;
	    }
	    /* FALLTHROUGH */

	case PCWSTOP:	/* wait for process or lwp to stop */
	case PCTWSTOP:	/* wait for process or lwp to stop, with timeout */
	    {
		time_t starttime;
		time_t timeo;

		/*
		 * Can't apply to a system process.
		 */
		if ((p->p_flag & SSYS) || p->p_as == &kas) {
			error = EBUSY;
			break;
		}

		/*
		 * Sleep until the lwp stops, but cooperate with
		 * jobcontrol:  Don't wake up if the lwp is stopped
		 * due to the default action of a jobcontrol stop signal.
		 * If this is the process file descriptor, sleep
		 * until all of the process's lwps stop.
		 */
		if (cmd == PCTWSTOP) {
			starttime = lbolt;
			timeo = argp->timeo;
		} else {
			starttime = 0;
			timeo = 0;
		}
		if (pcp->prc_flags & PRC_LWP) {	/* lwp file descriptor */
			t = pcp->prc_thread;
			ASSERT(t != NULL);
			thread_lock(t);
			while (!ISTOPPED(t) && !VSTOPPED(t)) {
				thread_unlock(t);
				ASSERT(pnp->pr_parent == t->t_trace);
				mutex_enter(&pcp->prc_mutex);
				prunlock(pnp);
				error = pr_wait(pcp, timeo, starttime);
				if (error)	/* -1 is timeout */
					return (error);
				if ((error = prlock(pnp, ZNO)) != 0)
					return (error);
				ASSERT(p == pcp->prc_proc);
				ASSERT(t == pcp->prc_thread);
				thread_lock(t);
			}
			thread_unlock(t);
		} else {			/* process file descriptor */
			t = prchoose(p);	/* returns locked thread */
			ASSERT(t != NULL);
			while (!ISTOPPED(t) && !VSTOPPED(t) && !SUSPENDED(t)) {
				thread_unlock(t);
				ASSERT(pnp->pr_parent == p->p_trace);
				mutex_enter(&pcp->prc_mutex);
				prunlock(pnp);
				error = pr_wait(pcp, timeo, starttime);
				if (error)	/* -1 is timeout */
					return (error);
				if ((error = prlock(pnp, ZNO)) != 0)
					return (error);
				ASSERT(p == pcp->prc_proc);
				t = prchoose(p);	/* returns locked t */
				ASSERT(t != NULL);
			}
			thread_unlock(t);
		}

		ASSERT(!(pcp->prc_flags & PRC_DESTROY) && p->p_stat != SZOMB &&
		    t != NULL && t->t_state != TS_ZOMB);

		break;
	    }

	case PCRUN:		/* make lwp or process runnable */
	    {
		register u_long flags = argp->flags;
		register klwp_t *lwp;

		t = pr_thread(pnp);	/* returns locked thread */
		if (!ISTOPPED(t) && !VSTOPPED(t)) {
			thread_unlock(t);
			error = EBUSY;
			break;
		}
		thread_unlock(t);
		if (flags & ~(PRCSIG|PRCFAULT|PRSTEP|PRSTOP|PRSABORT)) {
			error = EINVAL;
			break;
		}
		lwp = ttolwp(t);
		if ((flags & PRCSIG) && lwp->lwp_cursig != SIGKILL) {
			/*
			 * Discard current siginfo_t, if any.
			 */
			lwp->lwp_cursig = 0;
			if (lwp->lwp_curinfo) {
				siginfofree(lwp->lwp_curinfo);
				lwp->lwp_curinfo = NULL;
			}
		}
		if (flags & PRCFAULT)
			lwp->lwp_curflt = 0;
		/*
		 * We can't hold p->p_lock when we touch the lwp's registers.
		 * It may be swapped out and we will get a page fault.
		 */
		if (flags & PRSTEP) {
			mutex_exit(&p->p_lock);
			prstep(lwp, 0);
			mutex_enter(&p->p_lock);
		}
		if (flags & PRSTOP) {
			t->t_proc_flag |= TP_PRSTOP;
			t->t_sig_check = 1;	/* do ISSIG */
		}
		if (flags & PRSABORT)
			lwp->lwp_sysabort = 1;
		thread_lock(t);
		if ((pcp->prc_flags & PRC_LWP) || (flags & (PRSTEP|PRSTOP))) {
			if (ISTOPPED(t)) {
				t->t_schedflag |= TS_PSTART;
				setrun_locked(t);
			} else if (flags & PRSABORT) {
				t->t_proc_flag &=
				    ~(TP_PRSTOP|TP_PRVSTOP|TP_STOPPING);
				setrun_locked(t);
			} else if (!(flags & PRSTOP)) {
				t->t_proc_flag &=
				    ~(TP_PRSTOP|TP_PRVSTOP|TP_STOPPING);
			}
			thread_unlock(t);
		} else {
			if (ISTOPPED(t)) {
				t->t_whystop = PR_REQUESTED;
				t->t_whatstop = 0;
				thread_unlock(t);
				t = prchoose(p);	/* returns locked t */
				ASSERT(ISTOPPED(t) || VSTOPPED(t));
				if (VSTOPPED(t) ||
				    t->t_whystop == PR_REQUESTED) {
					thread_unlock(t);
					allsetrun(p);
				} else {
					thread_unlock(t);
				}
			} else {
				if (flags & PRSABORT) {
					t->t_proc_flag &=
					    ~(TP_PRSTOP|TP_PRVSTOP|TP_STOPPING);
					setrun_locked(t);
				}
				thread_unlock(t);
				allsetrun(p);
			}
		}
		break;
	    }

	case PCSTRACE:	/* set signal trace mask */
	    {
		prdelset(&argp->sigset, SIGKILL);
		prassignset(&p->p_sigmask, &argp->sigset);
		if (!sigisempty(&p->p_sigmask))
			p->p_flag |= SPROCTR;
		else if (prisempty(&p->p_fltmask)) {
			up = prumap(p);
			if (up->u_systrap == 0)
				p->p_flag &= ~SPROCTR;
			prunmap(p);
		}
		break;
	    }

	case PCSSIG:		/* set current signal */
	    {
		int sig = argp->siginfo.si_signo;
		register klwp_t *lwp;

		t = pr_thread(pnp);	/* returns locked thread */
		thread_unlock(t);
		lwp = ttolwp(t);
		if (sig < 0 || sig >= NSIG)
			/* Zero allowed here */
			error = EINVAL;
		else if (lwp->lwp_cursig == SIGKILL)
			/* "can't happen", but just in case */
			error = EBUSY;
		else if ((lwp->lwp_cursig = (u_char)sig) == 0) {
			/*
			 * Discard current siginfo_t, if any.
			 */
			if (lwp->lwp_curinfo) {
				siginfofree(lwp->lwp_curinfo);
				lwp->lwp_curinfo = NULL;
			}
		} else {
			kthread_t *tx;
			kthread_t *ty;
			sigqueue_t *sqp;

			/* drop p_lock to do kmem_alloc(KM_SLEEP) */
			mutex_exit(&p->p_lock);
			sqp = kmem_zalloc(sizeof (sigqueue_t), KM_SLEEP);
			mutex_enter(&p->p_lock);

			if (lwp->lwp_curinfo == NULL)
				lwp->lwp_curinfo = sqp;
			else
				kmem_free(sqp, sizeof (sigqueue_t));
			/*
			 * Copy contents of info to current siginfo_t.
			 */
			bcopy((caddr_t)&argp->siginfo,
			    (caddr_t)&lwp->lwp_curinfo->sq_info,
			    sizeof (lwp->lwp_curinfo->sq_info));
			/*
			 * Side-effects for SIGKILL and jobcontrol signals.
			 */
			tx = p->p_aslwptp;
			if (sig == SIGKILL)
				p->p_flag |= SKILLED;
			else if (sig == SIGCONT) {
				sigdelq(p, tx, SIGSTOP);
				sigdelq(p, tx, SIGTSTP);
				sigdelq(p, tx, SIGTTOU);
				sigdelq(p, tx, SIGTTIN);
				if (tx == NULL)
					sigdiffset(&p->p_sig, &stopdefault);
				else {
					sigdiffset(&tx->t_sig, &stopdefault);
					sigdiffset(&p->p_notifsigs,
					    &stopdefault);
					if ((ty = p->p_tlist) != NULL) {
						do {
							sigdelq(p, ty, SIGSTOP);
							sigdelq(p, ty, SIGTSTP);
							sigdelq(p, ty, SIGTTOU);
							sigdelq(p, ty, SIGTTIN);
							sigdiffset(&ty->t_sig,
							    &stopdefault);
						} while ((ty = ty->t_forw) !=
						    p->p_tlist);
					}
				}
			} else if (sigismember(&stopdefault, sig)) {
				sigdelq(p, tx, SIGCONT);
				if (tx == NULL)
					sigdelset(&p->p_sig, SIGCONT);
				else {
					sigdelset(&tx->t_sig, SIGCONT);
					sigdelset(&p->p_notifsigs, SIGCONT);
					if ((ty = p->p_tlist) != NULL) {
						do {
							sigdelq(p, ty, SIGCONT);
							sigdelset(&ty->t_sig,
							    SIGCONT);
						} while ((ty = ty->t_forw) !=
						    p->p_tlist);
					}
				}
			}
			thread_lock(t);
			if (t->t_state == TS_SLEEP &&
			    (t->t_flag & T_WAKEABLE)) {
				/* Set signalled sleeping lwp running */
				setrun_locked(t);
			} else if (t->t_state == TS_STOPPED && sig == SIGKILL) {
				/* If SIGKILL, set stopped lwp running */
				p->p_stopsig = 0;
				t->t_schedflag |= TS_XSTART | TS_PSTART;
				setrun_locked(t);
			}
			t->t_sig_check = 1;	/* so ISSIG will be done */
			thread_unlock(t);
			/*
			 * More jobcontrol side-effects.
			 */
			if (sig == SIGCONT && (tx = p->p_tlist) != NULL) {
				p->p_stopsig = 0;
				do {
					thread_lock(tx);
					if (tx->t_state == TS_STOPPED &&
					    tx->t_whystop == PR_JOBCONTROL) {
						tx->t_schedflag |= TS_XSTART;
						setrun_locked(tx);
					}
					thread_unlock(tx);
				} while ((tx = tx->t_forw) != p->p_tlist);
			}
		}
		if (sig == SIGKILL && error == 0) {
			prunlock(pnp);
			pr_wait_die(pnp);
			return (-1);
		}
		break;
	    }

	case PCKILL:		/* send signal */
	    {
		register int sig = argp->sig;
		k_siginfo_t info;

		if (sig <= 0 || sig >= NSIG)
			error = EINVAL;
		else {
			bzero((caddr_t)&info, sizeof (info));
			info.si_signo = sig;
			info.si_code = SI_USER;
			info.si_pid = ttoproc(curthread)->p_pid;
			info.si_uid = cr->cr_ruid;
			sigaddq(p, (pcp->prc_flags & PRC_LWP)?
			    pcp->prc_thread : NULL, &info, KM_NOSLEEP);
			if (sig == SIGKILL) {
				prunlock(pnp);
				pr_wait_die(pnp);
				return (-1);
			}
		}
		break;
	    }

	case PCUNKILL:	/* delete a pending signal */
	    {
		sigqueue_t *infop = NULL;
		int sig = argp->sig;

		if (sig <= 0 || sig >= NSIG || sig == SIGKILL)
			error = EINVAL;
		else {
			if (pcp->prc_flags & PRC_LWP) {
				t = pcp->prc_thread;
				prdelset(&t->t_sig, sig);
				sigdeq(p, t, sig, &infop);
			} else {
				kthread_t *aslwptp;

				if ((aslwptp = p->p_aslwptp) != NULL) {
					if (sigismember(&p->p_notifsigs, sig))
						prdelset(&p->p_notifsigs, sig);
					else
						prdelset(&aslwptp->t_sig, sig);
					sigdeq(p, aslwptp, sig, &infop);
				} else {
					prdelset(&p->p_sig, sig);
					sigdeq(p, NULL, sig, &infop);
				}
			}
			if (infop)
				siginfofree(infop);
		}
		break;
	    }

	case PCNICE:		/* set nice priority */
	    {
		int err;

		t = p->p_tlist;
		do {
			ASSERT(!(t->t_proc_flag & TP_LWPEXIT));
			err = CL_DONICE(t, cr, argp->nice, (int *)NULL);
			if (error == 0)
				error = err;
		} while ((t = t->t_forw) != p->p_tlist);
		break;
	    }

	case PCSENTRY:		/* set syscall entry bit mask */
	case PCSEXIT:		/* set syscall exit bit mask */
	    {
		up = prumap(p);
		if (cmd == PCSENTRY) {
			prassignset(&up->u_entrymask, &argp->sysset);
		} else {
			prassignset(&up->u_exitmask, &argp->sysset);
		}
		if (!prisempty(&up->u_entrymask) ||
		    !prisempty(&up->u_exitmask)) {
			up->u_systrap = 1;
			p->p_flag |= SPROCTR;
			set_proc_sys(p);	/* set pre and post-sys flags */
		} else {
			up->u_systrap = 0;
			if (sigisempty(&p->p_sigmask) &&
			    prisempty(&p->p_fltmask))
				p->p_flag &= ~SPROCTR;
		}
		prunmap(p);
		break;
	    }

	case PCSET:		/* set process flags */
	case PCUNSET:		/* unset process flags */
	    {
		register long flags = argp->flags;

#define	ALLFLAGS	\
	(PR_FORK|PR_RLC|PR_KLC|PR_ASYNC|PR_BPTADJ|PR_MSACCT|PR_PTRACE)

		if ((p->p_flag & SSYS) || p->p_as == &kas)
			error = EBUSY;
		else if (flags & ~ALLFLAGS)
			error = EINVAL;
		else if (cmd == PCSET) {
			if (flags & PR_FORK)
				p->p_flag |= SPRFORK;
			if (flags & PR_RLC)
				p->p_flag |= SRUNLCL;
			if (flags & PR_KLC)
				p->p_flag |= SKILLCL;
			if (flags & PR_ASYNC)
				p->p_flag |= SPASYNC;
			if (flags & PR_BPTADJ)
				p->p_flag |= SBPTADJ;
			if (flags & PR_MSACCT)
				if ((p->p_flag & SMSACCT) == 0)
					estimate_msacct(p->p_tlist,
					    gethrtime());
			if (flags & PR_PTRACE) {
				p->p_flag |= STRC;
				/* ptraced process must die if parent dead */
				if (p->p_ppid == 1)
					sigtoproc(p, NULL, SIGKILL, 0);
			}
		} else {
			if (flags & PR_FORK)
				p->p_flag &= ~SPRFORK;
			if (flags & PR_RLC)
				p->p_flag &= ~SRUNLCL;
			if (flags & PR_KLC)
				p->p_flag &= ~SKILLCL;
			if (flags & PR_ASYNC)
				p->p_flag &= ~SPASYNC;
			if (flags & PR_BPTADJ)
				p->p_flag &= ~SBPTADJ;
			if (flags & PR_MSACCT)
				disable_msacct(p);
			if (flags & PR_PTRACE)
				p->p_flag &= ~STRC;
		}
		break;
	    }

	case PCSREG:		/* set general registers */
	    {
		t = pr_thread(pnp);	/* returns locked thread */
		if (!ISTOPPED(t) && !VSTOPPED(t)) {
			thread_unlock(t);
			error = EBUSY;
		} else {
			/* drop p_lock while touching the lwp's stack */
			thread_unlock(t);
			mutex_exit(&p->p_lock);
			prsetprregs(ttolwp(t), argp->prgregset);
			mutex_enter(&p->p_lock);
		}
		break;
	    }

	case PCSFPREG:	/* set floating-point registers */
	    {
		t = pr_thread(pnp);	/* returns locked thread */
		if (!ISTOPPED(t) && !VSTOPPED(t)) {
			thread_unlock(t);
			error = EBUSY;
		} else if (!prhasfp()) {
			thread_unlock(t);
			error = EINVAL;	/* No FP support */
		} else {
			/* drop p_lock while touching the lwp's stack */
			thread_unlock(t);
			mutex_exit(&p->p_lock);
			prsetprfpregs(ttolwp(t), &argp->prfpregset);
			mutex_enter(&p->p_lock);
		}
		break;
	    }

	case PCSXREG:		/* set extra registers */
#if defined(sparc) || defined(__sparc)
		t = pr_thread(pnp);	/* returns locked thread */
		if (!ISTOPPED(t) && !VSTOPPED(t)) {
			thread_unlock(t);
			error = EBUSY;
		} else if (!prhasx()) {
			thread_unlock(t);
			error = EINVAL;	/* No extra register support */
		} else {
			/* drop p_lock while touching the lwp's stack */
			thread_unlock(t);
			mutex_exit(&p->p_lock);
			prsetprxregs(ttolwp(t), (caddr_t)&argp->prxregset);
			mutex_enter(&p->p_lock);
		}
#else
		error = EINVAL;		/* No extra register support */
#endif
		break;

	case PCSVADDR:		/* set virtual address at which to resume */
	    {
		t = pr_thread(pnp);	/* returns locked thread */
		if (!ISTOPPED(t) && !VSTOPPED(t)) {
			thread_unlock(t);
			error = EBUSY;
		} else {
			/* drop p_lock while touching the lwp's stack */
			thread_unlock(t);
			mutex_exit(&p->p_lock);
			prsvaddr(ttolwp(t), argp->vaddr);
			mutex_enter(&p->p_lock);
		}
		break;
	    }

	case PCSHOLD:		/* set signal-hold mask */
	    {
		t = pr_thread(pnp);	/* returns locked thread */
		sigutok(&argp->sigset, &t->t_hold);
		sigdiffset(&t->t_hold, &cantmask);
		if (t->t_state == TS_SLEEP &&
		    (t->t_flag & T_WAKEABLE) &&
		    (fsig(&p->p_sig, t) || fsig(&t->t_sig, t)))
			setrun_locked(t);
		thread_unlock(t);
		break;
	    }

	case PCSFAULT:	/* set mask of traced faults */
	    {
		prassignset(&p->p_fltmask, &argp->fltset);
		if (!prisempty(&p->p_fltmask))
			p->p_flag |= SPROCTR;
		else if (sigisempty(&p->p_sigmask)) {
			up = prumap(p);
			if (up->u_systrap == 0)
				p->p_flag &= ~SPROCTR;
			prunmap(p);
		}
		break;
	    }

	case PCCSIG:	/* clear current signal */
	    {
		register klwp_t *lwp;

		t = pr_thread(pnp);	/* returns locked thread */
		thread_unlock(t);
		lwp = ttolwp(t);
		if (lwp->lwp_cursig == SIGKILL)
			error = EBUSY;
		else {
			/*
			 * Discard current siginfo_t, if any.
			 */
			lwp->lwp_cursig = 0;
			if (lwp->lwp_curinfo) {
				siginfofree(lwp->lwp_curinfo);
				lwp->lwp_curinfo = NULL;
			}
		}
		break;
	    }

	case PCCFAULT:	/* clear current fault */
	    {
		t = pr_thread(pnp);	/* returns locked thread */
		thread_unlock(t);
		ttolwp(t)->lwp_curflt = 0;
		break;
	    }

	case PCWATCH:
	    {
		prwatch_t *pwp = &argp->prwatch;
		uintptr_t vaddr = pwp->pr_vaddr;
		size_t size = pwp->pr_size;
		int wflags = pwp->pr_wflags;
		struct watched_page *pwplist = NULL;
		int newpage = 0;
		struct watched_area *pwa;

		/*
		 * Can't apply to a system process.
		 */
		if ((p->p_flag & SSYS) || p->p_as == &kas) {
			error = EBUSY;
			break;
		}

		/*
		 * Verify that the address range does not wrap
		 * and that only the proper flags were specified.
		 */
		if (vaddr + size <= vaddr ||
		    (wflags & ~(WA_READ|WA_WRITE|WA_EXEC|WA_TRAPAFTER)) != 0) {
			error = EINVAL;
			break;
		}
		/*
		 * Don't let the address range go above USERLIMIT.
		 * There is no error here, just a limitation.
		 */
		if (vaddr >= USERLIMIT)
			break;
		if (vaddr + size > USERLIMIT)
			size = USERLIMIT - vaddr;

		/*
		 * Compute maximum number of pages this will add.
		 */
		if ((wflags & ~WA_TRAPAFTER) != 0) {
			u_long pagespan = (vaddr + size) - (vaddr & PAGEMASK);
			newpage = btopr(pagespan);
			if (newpage > 2 * prnwatch) {
				error = E2BIG;
				break;
			}
		}

		/*
		 * Force the process to be fully stopped.
		 */
		if (p == curproc) {
			prunlock(pnp);
			while (holdwatch() == 0)
				;
			if ((error = prlock(pnp, ZNO)) != 0) {
				continuelwps(p);
				return (error);
			}
		} else {
			int rv;

			pauselwps(p);
			while ((rv = allstopped(p)) != 0) {
				kmutex_t *mp;
				kcondvar_t *cv;
				/*
				 * Cannot set watchpoints if the process
				 * is stopped on exit from vfork().
				 */
				if (rv == -1) {
					unpauselwps(p);
					prunlock(pnp);
					return (EBUSY);
				}
				/*
				 * This cv/mutex pair is persistent even
				 * if the process disappears after we
				 * unmark it and drop p->p_lock.
				 */
				cv = &pr_pid_cv[p->p_slot];
				mp = &p->p_lock;
				prunmark(p);
				(void) cv_wait(cv, mp);
				mutex_exit(mp);
				if ((error = prlock(pnp, ZNO)) != 0) {
					/*
					 * Unpause the process if it exists.
					 */
					p = pr_p_lock(pnp);
					mutex_exit(&pr_pidlock);
					if (p != NULL) {
						unpauselwps(p);
						prunlock(pnp);
					}
					return (error);
				}
			}
		}

		/*
		 * Drop p->p_lock in order to perform the rest of this.
		 * The process is still locked with the SPRLOCK flag.
		 */
		mutex_exit(&p->p_lock);

		pwa = kmem_alloc(sizeof (struct watched_area), KM_SLEEP);
		pwa->wa_vaddr = (caddr_t)vaddr;
		pwa->wa_eaddr = (caddr_t)vaddr + size;
		pwa->wa_flags = wflags;

		/*
		 * Allocate enough watched_page structs to use while holding
		 * the process's p_lock.  We will later free the excess.
		 * Allocate one more than we will possible need in order
		 * to simplify subsequent code.
		 */
		if (newpage) {
			struct watched_page *pwpg;

			pwpg = kmem_zalloc(sizeof (struct watched_page),
				KM_SLEEP);
			pwplist = pwpg->wp_forw = pwpg->wp_back = pwpg;
			while (newpage-- > 0) {
				pwpg = kmem_zalloc(sizeof (struct watched_page),
					KM_SLEEP);
				insque(pwpg, pwplist->wp_back);
			}
			pwplist = pwplist->wp_back;
		}

		error = ((pwa->wa_flags & ~WA_TRAPAFTER) == 0)?
			clear_watched_area(p, pwa) :
			set_watched_area(p, pwa, pwplist);

		/*
		 * Free the watched_page structs we didn't use.
		 */
		if (pwplist)
			pr_free_pagelist(pwplist);

		mutex_enter(&p->p_lock);
		if (p == curproc)
			continuelwps(p);
		else
			unpauselwps(p);
		break;
	    }

	}

	if (error)
		prunlock(pnp);
	return (error);
}

/*
 * Return the specific or chosen thread/lwp for a control operation.
 * Returns with the thread locked via thread_lock(t).
 */
static kthread_t *
pr_thread(pnp)
	register prnode_t *pnp;
{
	register prcommon_t *pcp = pnp->pr_common;
	register kthread_t *t;

	if (pcp->prc_flags & PRC_LWP) {
		t = pcp->prc_thread;
		ASSERT(t != NULL);
		ASSERT(pnp->pr_parent == t->t_trace);
		thread_lock(t);
	} else {
		register proc_t *p = pcp->prc_proc;
		t = prchoose(p);	/* returns locked thread */
		ASSERT(t != NULL);
		ASSERT(pnp->pr_parent == p->p_trace);
	}

	return (t);
}

static void
pr_timeout(pcp)
	register prcommon_t *pcp;
{
	mutex_enter(&pcp->prc_mutex);
	cv_broadcast(&pcp->prc_wait);
	mutex_exit(&pcp->prc_mutex);
}

/*
 * Wait until process/lwp stops or until timer expires.
 */
int
pr_wait(pcp, timeo, starttime)
	register prcommon_t *pcp;	/* prcommon referring to process/lwp */
	register time_t timeo;		/* timeout in milliseconds */
	register time_t starttime;	/* value of lbolt at start of timer */
{
	int error = 0;
	int id = 0;

	ASSERT(MUTEX_HELD(&pcp->prc_mutex));

	if (timeo > 0) {
		/*
		 * Make sure the millisecond timeout hasn't been reached.
		 */
		int rem = timeo - ((lbolt - starttime)*1000)/hz;
		if (rem <= 0) {
			mutex_exit(&pcp->prc_mutex);
			return (-1);
		}
		/*
		 * Turn rem into clock ticks and round up.
		 */
		rem = ((rem/1000) * hz) + ((((rem%1000) * hz) + 999) / 1000);
		id = timeout((void(*)())pr_timeout, (caddr_t)pcp, rem);
	}

	if (!cv_wait_sig(&pcp->prc_wait, &pcp->prc_mutex)) {
		mutex_exit(&pcp->prc_mutex);
		if (timeo > 0)
			(void) untimeout(id);
		return (EINTR);
	}
	mutex_exit(&pcp->prc_mutex);
	if (timeo > 0 && untimeout(id) < 0)
		error = -1;
	return (error);
}

/*
 * Make all threads in the process runnable.
 */
void
allsetrun(p)
	register proc_t *p;
{
	register kthread_t *t;

	ASSERT(MUTEX_HELD(&p->p_lock));

	if ((t = p->p_tlist) != NULL) {
		do {
			thread_lock(t);
			ASSERT(!(t->t_proc_flag & TP_LWPEXIT));
			t->t_proc_flag &= ~(TP_PRSTOP|TP_PRVSTOP|TP_STOPPING);
			if (ISTOPPED(t)) {
				t->t_schedflag |= TS_PSTART;
				setrun_locked(t);
			}
			thread_unlock(t);
		} while ((t = t->t_forw) != p->p_tlist);
	}
}

/*
 * Wait for the process to die.
 * We do this after sending SIGKILL because we know it will
 * die soon and we want subsequent operations to return ENOENT.
 */
void
pr_wait_die(register prnode_t *pnp)
{
	register proc_t *p;

	mutex_enter(&pidlock);
	while ((p = pnp->pr_common->prc_proc) != NULL && p->p_stat != SZOMB) {
		if (!cv_wait_sig(&p->p_srwchan_cv, &pidlock))
			break;
	}
	mutex_exit(&pidlock);
}

/*
 * Return -1 if the process is the parent of a vfork(1)
 * whose child has yet to terminate or perform an exec(2).
 * Otherwise return 0 if the process is fully stopped, except
 * for the current lwp (if we are operating on our own process).
 * Otherwise return 1 to indicate the process is not fully stopped.
 */
static int
allstopped(proc_t *p)
{
	kthread_t *t;
	int rv = 0;

	if (p->p_flag & SVFWAIT)	/* waiting for vfork'd child to exec */
		return (-1);

	if ((t = p->p_tlist) != NULL) {
		do {
			if (t == curthread || VSTOPPED(t))
				continue;
			ASSERT(t->t_proc_flag & TP_PAUSE);
			thread_lock(t);
			switch (t->t_state) {
			case TS_ZOMB:
			case TS_STOPPED:
				break;
			case TS_SLEEP:
				if (!(t->t_flag & T_WAKEABLE) ||
				    t->t_wchan0 == 0)
					rv = 1;
				break;
			default:
				rv = 1;
				break;
			}
			thread_unlock(t);
		} while (rv == 0 && (t = t->t_forw) != p->p_tlist);
	}

	return (rv);
}

/*
 * Cause all lwps in the process to pause (for watchpoint operations).
 */
static void
pauselwps(proc_t *p)
{
	kthread_t *t;

	ASSERT(MUTEX_HELD(&p->p_lock));
	ASSERT(p != curproc);

	if ((t = p->p_tlist) != NULL) {
		do {
			thread_lock(t);
			t->t_proc_flag |= TP_PAUSE;
			aston(t);
			if (t->t_state == TS_SLEEP &&
			    (t->t_flag & T_WAKEABLE)) {
				if (t->t_wchan0 == 0)
					setrun_locked(t);
			}
			prpokethread(t);
			thread_unlock(t);
		} while ((t = t->t_forw) != p->p_tlist);
	}
}

/*
 * undo the effects of pauselwps()
 */
static void
unpauselwps(proc_t *p)
{
	kthread_t *t;

	ASSERT(MUTEX_HELD(&p->p_lock));
	ASSERT(p != curproc);

	if ((t = p->p_tlist) != NULL) {
		do {
			thread_lock(t);
			t->t_proc_flag &= ~TP_PAUSE;
			if (t->t_state == TS_STOPPED) {
				t->t_schedflag |= TS_UNPAUSE;
				setrun_locked(t);
			}
			thread_unlock(t);
		} while ((t = t->t_forw) != p->p_tlist);
	}
}

/*
 * Cancel all watched areas.  Called from prclose().
 */
proc_t *
pr_cancel_watch(prnode_t *pnp)
{
	proc_t *p = pnp->pr_pcommon->prc_proc;
	struct as *as;
	kthread_t *t;

	ASSERT(MUTEX_HELD(&p->p_lock) && (p->p_flag & SPRLOCK));

	if (p->p_warea == NULL)		/* no watchpoints */
		return (p);

	/*
	 * Pause the process before dealing with the watchpoints.
	 */
	if (p == curproc) {
		prunlock(pnp);
		while (holdwatch() == 0)
			;
		p = pr_p_lock(pnp);
		mutex_exit(&pr_pidlock);
		ASSERT(p == curproc);
	} else {
		pauselwps(p);
		while (p != NULL && allstopped(p) > 0) {
			/*
			 * This cv/mutex pair is persistent even
			 * if the process disappears after we
			 * unmark it and drop p->p_lock.
			 */
			kcondvar_t *cv = &pr_pid_cv[p->p_slot];
			kmutex_t *mp = &p->p_lock;

			prunmark(p);
			(void) cv_wait(cv, mp);
			mutex_exit(mp);
			p = pr_p_lock(pnp);  /* NULL if process disappeared */
			mutex_exit(&pr_pidlock);
		}
	}

	if (p == NULL)		/* the process disappeared */
		return (NULL);

	ASSERT(p == pnp->pr_pcommon->prc_proc);
	ASSERT(MUTEX_HELD(&p->p_lock) && (p->p_flag & SPRLOCK));

	if (p->p_warea != NULL) {
		pr_free_watchlist(p->p_warea);
		p->p_warea = NULL;
		p->p_nwarea = 0;
		if ((t = p->p_tlist) != NULL) {
			do {
				t->t_proc_flag &= ~TP_WATCHPT;
			} while ((t = t->t_forw) != p->p_tlist);
		}
	}

	if ((as = p->p_as) != NULL) {
		struct watched_page *pwp_first;
		struct watched_page *pwp;

		/*
		 * If this is the parent of a vfork, the watched page
		 * list has been moved temporarily to p->p_wpage.
		 */
		pwp = pwp_first = p->p_wpage? p->p_wpage : as->a_wpage;
		if (pwp != NULL) {
			mutex_exit(&p->p_lock);
			AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);
			do {
				pwp->wp_read = 0;
				pwp->wp_write = 0;
				pwp->wp_exec = 0;
				if (pwp->wp_oprot != 0)
					pwp->wp_flags |= WP_SETPROT;
				pwp->wp_prot = pwp->wp_oprot;
			} while ((pwp = pwp->wp_forw) != pwp_first);
			AS_LOCK_EXIT(as, &as->a_lock);
			mutex_enter(&p->p_lock);
		}
	}

	/*
	 * Unpause the process now.
	 */
	if (p == curproc)
		continuelwps(p);
	else
		unpauselwps(p);

	return (p);
}
