/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ident	"@(#)prioctl.c	1.83	96/09/24 SMI"	/* SVr4.0 1.29	*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/vmparam.h>
#include <sys/var.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/inline.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/cpuvar.h>
#include <sys/session.h>
#include <sys/signal.h>
#include <sys/auxv.h>
#include <sys/user.h>
#include <sys/disp.h>
#include <sys/class.h>
#include <sys/ts.h>
#include <sys/mman.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/old_procfs.h>
#include <vm/rm.h>
#include <vm/as.h>
#include <vm/rm.h>
#include <vm/seg.h>
#include <vm/seg_vn.h>

#if i386
#include <sys/sysi86.h>
#endif

#include <fs/proc/prdata.h>

static	int	isprwrioctl(int);
static	void	prsetrun(kthread_t *, prrun_t *);
extern	void	oprgetstatus(kthread_t *, prstatus_t *);
extern	void	oprgetpsinfo(proc_t *, prpsinfo_t *, kthread_t *);
static	int	oprgetmap(proc_t *, prmap_t **, size_t *);

/*
 * Control operations (lots).
 */
int
prioctl(vp, cmd, arg, flag, cr, rvalp)
	struct vnode *vp;
	int cmd;
	intptr_t arg;
	int flag;
	struct cred *cr;
	int *rvalp;
{
	caddr_t cmaddr = (caddr_t)arg;
	register proc_t *p;
	register user_t *up;
	register kthread_t *t;
	register klwp_t *lwp;
	register prnode_t *pnp = VTOP(vp);
	register prcommon_t *pcp;
	prnode_t *xpnp = NULL;
	register int error;
	int zdisp;
	void *thing = NULL;
	size_t thingsize = 0;

	/*
	 * For copyin()/copyout().
	 */
	union {
		caddr_t		va;
		int		signo;
		int		nice;
		u_int		lwpid;
		long		flags;
		prstatus_t	prstat;
		prrun_t		prrun;
		sigset_t	smask;
		siginfo_t	info;
		sysset_t	prmask;
		prgregset_t	regs;
		prfpregset_t	fpregs;
		prpsinfo_t	prps;
		sigset_t	holdmask;
		fltset_t	fltmask;
		prcred_t	prcred;
		prusage_t	prusage;
		prhusage_t	prhusage;
		prmap_t		prmap;
		auxv_t		auxv[NUM_AUX_VECTORS];
	} un;

	/*
	 * Support for old /proc interface.
	 */
	if (pnp->pr_pidfile != NULL) {
		ASSERT(pnp->pr_type == PR_PIDDIR);
		vp = pnp->pr_pidfile;
		pnp = VTOP(vp);
		ASSERT(pnp->pr_type == PR_PIDFILE);
	}

	if (pnp->pr_type != PR_PIDFILE && pnp->pr_type != PR_LWPIDFILE)
		return (ENOTTY);

	/*
	 * Fail ioctls which are logically "write" requests unless
	 * the user has write permission.
	 */
	if ((flag & FWRITE) == 0 && isprwrioctl(cmd))
		return (EBADF);

	/*
	 * Perform any necessary copyin() operations before
	 * locking the process.  Helps avoid deadlocks and
	 * improves performance.
	 *
	 * Also, detect invalid ioctl codes here to avoid
	 * locking a process unnnecessarily.
	 *
	 * Also, prepare to allocate space that will be needed below,
	 * case by case.
	 */
	error = 0;
	switch (cmd) {
	case PIOCGETPR:
		thingsize = sizeof (proc_t);
		break;
	case PIOCGETU:
		thingsize = sizeof (user_t);
		break;
	case PIOCSTOP:
	case PIOCWSTOP:
	case PIOCLWPIDS:
	case PIOCGTRACE:
	case PIOCGENTRY:
	case PIOCGEXIT:
	case PIOCSRLC:
	case PIOCRRLC:
	case PIOCSFORK:
	case PIOCRFORK:
	case PIOCGREG:
	case PIOCGFPREG:
	case PIOCSTATUS:
	case PIOCLSTATUS:
	case PIOCPSINFO:
	case PIOCMAXSIG:
	case PIOCGXREGSIZE:
		break;
	case PIOCSXREG:		/* set extra registers */
	case PIOCGXREG:		/* get extra registers */
		if (prhasx())
			thingsize = prgetprxregsize();
		break;
	case PIOCACTION:
		thingsize = (NSIG-1) * sizeof (struct sigaction);
		break;
	case PIOCGHOLD:
	case PIOCNMAP:
	case PIOCMAP:
	case PIOCGFAULT:
	case PIOCCFAULT:
	case PIOCCRED:
	case PIOCGROUPS:
	case PIOCUSAGE:
	case PIOCLUSAGE:
		break;
	case PIOCOPENPD:
		/*
		 * We will need this below.
		 * Allocate it now, before locking the process.
		 */
		xpnp = prgetnode(PR_OPAGEDATA);
		break;
	case PIOCNAUXV:
	case PIOCAUXV:
		break;
#if i386
#if 0	/* USL, not Solaris */
	case PIOCGDBREG:
	case PIOCSDBREG:
		thingsize = sizeof (dbregset_t);
		break;
#endif
	case PIOCNLDT:
	case PIOCLDT:
		break;
#endif	/* i386 */
#if sparc
	case PIOCGWIN:
		thingsize = sizeof (gwindows_t);
		break;
#endif	/* sparc */

	case PIOCOPENM:		/* open mapped object for reading */
		if (cmaddr == NULL)
			un.va = NULL;
		else if (copyin(cmaddr, (caddr_t)&un.va, sizeof (un.va)))
			error = EFAULT;
		break;

	case PIOCRUN:		/* make lwp or process runnable */
		if (cmaddr == NULL)
			un.prrun.pr_flags = 0;
		else if (copyin(cmaddr, (caddr_t)&un.prrun, sizeof (un.prrun)))
			error = EFAULT;
		break;

	case PIOCOPENLWP:	/* return /proc lwp file descriptor */
		if (copyin(cmaddr, (caddr_t)&un.lwpid, sizeof (un.lwpid)))
			error = EFAULT;
		break;

	case PIOCSTRACE:	/* set signal trace mask */
		if (copyin(cmaddr, (caddr_t)&un.smask, sizeof (un.smask)))
			error = EFAULT;
		break;

	case PIOCSSIG:		/* set current signal */
		thingsize = sizeof (sigqueue_t);
		if (cmaddr == NULL)
			un.info.si_signo = 0;
		else if (copyin(cmaddr, (caddr_t)&un.info, sizeof (un.info)))
			error = EFAULT;
		break;

	case PIOCKILL:		/* send signal */
		thingsize = sizeof (sigqueue_t);
		/* FALLTHROUGH */
	case PIOCUNKILL:	/* delete a signal */
		if (copyin(cmaddr, (caddr_t)&un.signo, sizeof (un.signo)))
			error = EFAULT;
		break;

	case PIOCNICE:		/* set nice priority */
		if (copyin(cmaddr, (caddr_t)&un.nice, sizeof (un.nice)))
			error = EFAULT;
		break;

	case PIOCSENTRY:	/* set syscall entry bit mask */
	case PIOCSEXIT:		/* set syscall exit bit mask */
		if (copyin(cmaddr, (caddr_t)&un.prmask, sizeof (un.prmask)))
			error = EFAULT;
		break;

	case PIOCSET:		/* set process flags */
	case PIOCRESET:		/* reset process flags */
		if (copyin(cmaddr, (caddr_t)&un.flags, sizeof (un.flags)))
			error = EFAULT;
		break;

	case PIOCSREG:		/* set general registers */
		if (copyin(cmaddr, (caddr_t)un.regs, sizeof (un.regs)))
			error = EFAULT;
		break;

	case PIOCSFPREG:	/* set floating-point registers */
		if (copyin(cmaddr, (caddr_t)&un.fpregs, sizeof (un.fpregs)))
			error = EFAULT;
		break;

	case PIOCSHOLD:		/* set signal-hold mask */
		if (copyin(cmaddr, (caddr_t)&un.holdmask,
		    sizeof (un.holdmask)))
			error = EFAULT;
		break;

	case PIOCSFAULT:	/* set mask of traced faults */
		if (copyin(cmaddr, (caddr_t)&un.fltmask, sizeof (un.fltmask)))
			error = EFAULT;
		break;

	default:
		error = EINVAL;
		break;
	}

	if (error)
		return (error);

startover:
	/*
	 * If we need kmem_alloc()d space then we allocate it now, before
	 * grabbing the process lock.  Using kmem_alloc(KM_SLEEP) while
	 * holding the process lock leads to deadlock with the clock thread.
	 * (The clock thread wakes up the pageout daemon to free up space.
	 * If the clock thread blocks behind us and we are sleeping waiting
	 * for space, then space may never become available.)
	 */
	if (thingsize) {
		ASSERT(thing == NULL);
		thing = kmem_alloc(thingsize, KM_SLEEP);
	}

	switch (cmd) {
	case PIOCPSINFO:
	case PIOCGETPR:
	case PIOCUSAGE:
	case PIOCLUSAGE:
		zdisp = ZYES;
		break;
	case PIOCSXREG:		/* set extra registers */
		/*
		 * perform copyin before grabbing the process lock
		 */
		if (prhasx() && thing) {
			if (copyin(cmaddr, thing, thingsize)) {
				kmem_free(thing, thingsize);
				return (EFAULT);
			}
		}
		/* fall through... */
	default:
		zdisp = ZNO;
		break;
	}

	if ((error = prlock(pnp, zdisp)) != 0) {
		if (thing != NULL)
			kmem_free(thing, thingsize);
		if (xpnp)
			prfreenode(xpnp);
		return (error);
	}

	/*
	 * Choose a thread/lwp for the operation.
	 */
	pcp = pnp->pr_common;
	p = pcp->prc_proc;
	ASSERT(p != NULL);
	if (zdisp == ZNO && cmd != PIOCSTOP && cmd != PIOCWSTOP) {
		if (pnp->pr_type == PR_LWPIDFILE && cmd != PIOCLSTATUS) {
			t = pcp->prc_thread;
			ASSERT(t != NULL);
			ASSERT(pnp->pr_parent == t->t_trace);
		} else {
			t = prchoose(p);	/* returns locked thread */
			ASSERT(t != NULL);
			thread_unlock(t);
			ASSERT((pnp->pr_type == PR_PIDFILE)?
			    (pnp->pr_parent == p->p_trace) : 1);
		}
		lwp = ttolwp(t);
	}

	error = 0;
	switch (cmd) {

	case PIOCGETPR:		/* read struct proc */
	{
		proc_t *prp = (proc_t *)thing;

		*prp = *p;
		prunlock(pnp);
		if (copyout((caddr_t)prp, cmaddr, sizeof (proc_t)))
			error = EFAULT;
		kmem_free((caddr_t)prp, sizeof (proc_t));
		thing = NULL;
		break;
	}

	case PIOCGETU:		/* read u-area */
	{
		user_t *userp = (user_t *)thing;

		up = prumap(p);
		*userp = *up;
		prunmap(p);
		prunlock(pnp);
		if (copyout((caddr_t)userp, cmaddr, sizeof (user_t)))
			error = EFAULT;
		kmem_free((caddr_t)userp, sizeof (user_t));
		thing = NULL;
		break;
	}

	case PIOCOPENM:		/* open mapped object for reading */
	{
		register struct seg *seg;
		int n;
		struct vnode *xvp;

		/*
		 * By fiat, a system process has no address space.
		 */
		if ((p->p_flag & SSYS) || p->p_as == &kas) {
			error = EINVAL;
		} else if (cmaddr) {
			struct as *as = p->p_as;

			/*
			 * We drop p_lock before grabbing the address
			 * space lock in order to avoid a deadlock with
			 * the clock thread.  The process will not
			 * disappear and its address space will not
			 * change because it is marked SPRLOCK.
			 */
			mutex_exit(&p->p_lock);
			AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
			seg = as_segat(as, un.va);
			if (seg != NULL &&
			    seg->s_ops == &segvn_ops &&
			    SEGOP_GETVP(seg, un.va, &xvp) == 0 &&
			    xvp != NULL &&
			    xvp->v_type == VREG) {
				VN_HOLD(xvp);
			} else {
				error = EINVAL;
			}
			AS_LOCK_EXIT(as, &as->a_lock);
			mutex_enter(&p->p_lock);
		} else if ((xvp = p->p_exec) == NULL) {
			error = EINVAL;
		} else {
			VN_HOLD(xvp);
		}

		prunlock(pnp);

		if (error == 0) {
			if ((error = VOP_ACCESS(xvp, VREAD, 0, cr)) == 0)
				error = fassign(&xvp, FREAD, &n);
			if (error) {
				VN_RELE(xvp);
			} else {
				*rvalp = n;
			}
		}
		break;
	}

	case PIOCSTOP:		/* stop process or lwp from running */
	case PIOCWSTOP:		/* wait for process or lwp to stop */
		if (pnp->pr_type == PR_LWPIDFILE)
			t = pcp->prc_thread;
		else
			t = p->p_tlist;
		ASSERT(t != NULL);

		/*
		 * Can't apply to a system process.
		 */
		if ((p->p_flag & SSYS) || p->p_as == &kas) {
			prunlock(pnp);
			error = EBUSY;
			break;
		}

		if (cmd == PIOCSTOP) {
			/*
			 * If already stopped, do nothing; otherwise flag
			 * it to be stopped the next time it tries to run.
			 * If sleeping at interruptible priority, set it
			 * running so it will stop within cv_wait_sig(),
			 * unless it is in one of the lwp_*() syscalls,
			 * in which case mark it as virtually stopped
			 * and notify /proc waiters.
			 *
			 * Take care to cooperate with jobcontrol: if an lwp
			 * is stopped due to the default action of a jobcontrol
			 * stop signal, flag it to be stopped the next time it
			 * starts due to a SIGCONT signal.  We sleep below,
			 * in PIOCWSTOP, for the lwp to stop again.
			 */

			register int notify;

			do {
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
				if (pnp->pr_type == PR_LWPIDFILE)
					break;
			} while ((t = t->t_forw) != p->p_tlist);
			/*
			 * We do this just in case the thread we asked
			 * to stop is in holdlwps() (called from cfork()).
			 */
			cv_broadcast(&p->p_holdlwps);
		}

		/*
		 * Sleep until the lwp stops, but cooperate with
		 * jobcontrol:  Don't wake up if the lwp is stopped
		 * due to the default action of a jobcontrol stop signal.
		 * If this is the process file descriptor, sleep
		 * until all of the process's lwps stop.
		 *
		 * If an lwp/process is stopping itself, don't wait.
		 * The lwp will never see the fact that it is stopped.
		 */
		if (cmd == PIOCSTOP && ((pnp->pr_type == PR_LWPIDFILE)?
		    t == curthread : p == curproc))
			t = curthread;
		else if (pnp->pr_type == PR_LWPIDFILE) {
			t = pcp->prc_thread;
			ASSERT(t != NULL);
			thread_lock(t);
			while (!ISTOPPED(t) && !VSTOPPED(t)) {
				thread_unlock(t);
				ASSERT(pnp->pr_parent == t->t_trace);
				mutex_enter(&pcp->prc_mutex);
				prunlock(pnp);
				error = pr_wait(pcp, (time_t)0, (time_t)0);
				if (error)
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
				error = pr_wait(pcp, (time_t)0, (time_t)0);
				if (error)
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

		if (cmaddr) {
			/*
			 * Return process/lwp status information.
			 */
			oprgetstatus(t, &un.prstat);
			prunlock(pnp);
			if (copyout((caddr_t)&un.prstat, cmaddr,
			    sizeof (un.prstat)))
				error = EFAULT;
		} else {
			prunlock(pnp);
		}
		break;

	case PIOCRUN:		/* make lwp or process runnable */
	{
		register long flags = un.prrun.pr_flags;

		if (!ISTOPPED(t) && !VSTOPPED(t)) {
			prunlock(pnp);
			error = EBUSY;
			break;
		}

		if (flags & PRSTOP) {
			t->t_proc_flag |= TP_PRSTOP;
			t->t_sig_check = 1;	/* so ISSIG will be done */
		} else {
			t->t_proc_flag &= ~(TP_PRSTOP|TP_PRVSTOP|TP_STOPPING);
		}

		if (flags)
			prsetrun(t, &un.prrun);

		thread_lock(t);
		if (pnp->pr_type == PR_LWPIDFILE || (flags & (PRSTEP|PRSTOP))) {
			if (ISTOPPED(t)) {
				t->t_schedflag |= TS_PSTART;
				setrun_locked(t);
			} else if (flags & PRSABORT) {
				t->t_proc_flag &=
				    ~(TP_PRSTOP|TP_PRVSTOP|TP_STOPPING);
				setrun_locked(t);
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
		prunlock(pnp);
		break;
	}

	case PIOCLWPIDS:	/* get array of lwp identifiers */
	{
		int nlwp;
		int Nlwp;
		id_t *idp;
		id_t *Bidp;

		Nlwp = nlwp = p->p_lwpcnt;

		if (thing && thingsize != (Nlwp+1) * sizeof (id_t)) {
			kmem_free(thing, thingsize);
			thing = NULL;
		}
		if (thing == NULL) {
			thingsize = (Nlwp+1) * sizeof (id_t);
			thing = kmem_alloc(thingsize, KM_NOSLEEP);
		}
		if (thing == NULL) {
			prunlock(pnp);
			goto startover;
		}

		idp = (id_t *)thing;
		thing = NULL;
		Bidp = idp;
		if ((t = p->p_tlist) != NULL) {
			do {
				ASSERT(!(t->t_proc_flag & TP_LWPEXIT));
				ASSERT(nlwp > 0);
				--nlwp;
				*idp++ = t->t_tid;
			} while ((t = t->t_forw) != p->p_tlist);
		}
		*idp = 0;
		ASSERT(nlwp == 0);
		prunlock(pnp);
		if (copyout((caddr_t)Bidp, cmaddr, (Nlwp+1) * sizeof (id_t)))
			error = EFAULT;
		kmem_free((caddr_t)Bidp, (Nlwp+1) * sizeof (id_t));
		break;
	}

	case PIOCOPENLWP:	/* return /proc lwp file descriptor */
	{
		pid_t pid = p->p_pid;
		vnode_t *xvp;
		int n;

		prunlock(pnp);
		if ((xvp = prlwpnode(pid, un.lwpid)) == NULL)
			error = ENOENT;
		else if (error = fassign(&xvp, flag & (FREAD|FWRITE), &n)) {
			VN_RELE(xvp);
		} else
			*rvalp = n;
		break;
	}

	case PIOCOPENPD:	/* return /proc page data file descriptor */
	{
		vnode_t *xvp = PTOV(xpnp);
		vnode_t *dp = p->p_trace;
		int n;

		ASSERT(dp != NULL);
		VN_HOLD(dp);

		pcp = VTOP(dp)->pr_common;
		xpnp->pr_ino = ptoi(pcp->prc_pid);
		xpnp->pr_common = pcp;
		xpnp->pr_pcommon = pcp;
		xpnp->pr_parent = dp;

		xpnp->pr_next = p->p_plist;
		p->p_plist = xvp;

		prunlock(pnp);
		if (error = fassign(&xvp, FREAD, &n)) {
			VN_RELE(xvp);
		} else
			*rvalp = n;

		xpnp = NULL;
		break;
	}

	case PIOCGTRACE:	/* get signal trace mask */
		prassignset(&un.smask, &p->p_sigmask);
		prunlock(pnp);
		if (copyout((caddr_t)&un.smask, cmaddr, sizeof (un.smask)))
			error = EFAULT;
		break;

	case PIOCSTRACE:	/* set signal trace mask */
		prdelset(&un.smask, SIGKILL);
		prassignset(&p->p_sigmask, &un.smask);
		if (!sigisempty(&p->p_sigmask))
			p->p_flag |= SPROCTR;
		else if (prisempty(&p->p_fltmask)) {
			up = prumap(p);
			if (up->u_systrap == 0)
				p->p_flag &= ~SPROCTR;
			prunmap(p);
		}
		prunlock(pnp);
		break;

	case PIOCSSIG:		/* set current signal */
	{
		int sig = un.info.si_signo;

		if (sig < 0 || sig >= NSIG)
			/* Zero allowed here */
			error = EINVAL;
		else if (lwp->lwp_cursig == SIGKILL)
			/* "can't happen", but just in case */
			error = EBUSY;
		else if ((lwp->lwp_cursig = (u_char) sig) == 0) {
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

			/*
			 * If no current siginfo_t, use the allocated one.
			 */
			if (lwp->lwp_curinfo == NULL) {
				bzero((caddr_t)thing, sizeof (sigqueue_t));
				lwp->lwp_curinfo = (sigqueue_t *)thing;
				thing = NULL;
			}
			/*
			 * Copy contents of info to current siginfo_t.
			 */
			bcopy((caddr_t)&un.info,
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
		prunlock(pnp);
		if (thing)
			kmem_free(thing, thingsize);
		thing = NULL;
		if (sig == SIGKILL && error == 0)
			pr_wait_die(pnp);
		break;
	}

	case PIOCKILL:		/* send signal */
	{
		register int sig = un.signo;
		sigqueue_t *sqp = (sigqueue_t *)thing;

		if (sig <= 0 || sig >= NSIG) {
			error = EINVAL;
			kmem_free(thing, thingsize);
		} else {
			bzero((caddr_t)sqp, sizeof (sigqueue_t));
			sqp->sq_info.si_signo = sig;
			sqp->sq_info.si_code = SI_USER;
			sqp->sq_info.si_pid = ttoproc(curthread)->p_pid;
			sqp->sq_info.si_uid = cr->cr_ruid;
			sigaddqa(p, (pnp->pr_type == PR_LWPIDFILE)? t : NULL,
			    sqp);
		}
		prunlock(pnp);
		thing = NULL;
		if (sig == SIGKILL && error == 0)
			pr_wait_die(pnp);
		break;
	}

	case PIOCUNKILL:	/* delete a signal */
	{
		sigqueue_t *infop = NULL;

		if (un.signo <= 0 || un.signo >= NSIG || un.signo == SIGKILL)
			error = EINVAL;
		else {
			if (pnp->pr_type == PR_LWPIDFILE) {
				prdelset(&t->t_sig, un.signo);
				sigdeq(p, t, un.signo, &infop);
			} else {
				kthread_t *aslwptp;

				if ((aslwptp = p->p_aslwptp) != NULL) {
					if (sigismember(&p->p_notifsigs,
					    un.signo))
						prdelset(&p->p_notifsigs,
						    un.signo);
					else
						prdelset(&aslwptp->t_sig,
						    un.signo);
					sigdeq(p, aslwptp, un.signo, &infop);
				} else {
					prdelset(&p->p_sig, un.signo);
					sigdeq(p, NULL, un.signo, &infop);
				}
			}
			if (infop)
				siginfofree(infop);
		}
		prunlock(pnp);
		break;
	}

	case PIOCNICE:		/* set nice priority */
	{
		int err;

		t = p->p_tlist;
		do {
			ASSERT(!(t->t_proc_flag & TP_LWPEXIT));
			err = CL_DONICE(t, cr, un.nice, (int *)NULL);
			if (error == 0)
				error = err;
		} while ((t = t->t_forw) != p->p_tlist);
		prunlock(pnp);
		break;
	}

	case PIOCGENTRY:	/* get syscall entry bit mask */
	case PIOCGEXIT:		/* get syscall exit bit mask */
		up = prumap(p);
		if (cmd == PIOCGENTRY) {
			prassignset(&un.prmask, &up->u_entrymask);
		} else {
			prassignset(&un.prmask, &up->u_exitmask);
		}
		prunmap(p);
		prunlock(pnp);
		if (copyout((caddr_t)&un.prmask, cmaddr, sizeof (un.prmask)))
			error = EFAULT;
		break;

	case PIOCSENTRY:	/* set syscall entry bit mask */
	case PIOCSEXIT:		/* set syscall exit bit mask */
		up = prumap(p);
		if (cmd == PIOCSENTRY) {
			prassignset(&up->u_entrymask, &un.prmask);
		} else {
			prassignset(&up->u_exitmask, &un.prmask);
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
		prunlock(pnp);
		break;

	case PIOCSRLC:		/* obsolete: set running on last /proc close */
		if ((p->p_flag & SSYS) || p->p_as == &kas)
			error = EINVAL;
		else
			p->p_flag |= SRUNLCL;
		prunlock(pnp);
		break;

	case PIOCRRLC:		/* obsolete: reset run-on-last-close flag */
		if ((p->p_flag & SSYS) || p->p_as == &kas)
			error = EINVAL;
		else
			p->p_flag &= ~SRUNLCL;
		prunlock(pnp);
		break;

	case PIOCSFORK:		/* obsolete: set inherit-on-fork flag */
		if ((p->p_flag & SSYS) || p->p_as == &kas)
			error = EINVAL;
		else
			p->p_flag |= SPRFORK;
		prunlock(pnp);
		break;

	case PIOCRFORK:		/* obsolete: reset inherit-on-fork flag */
		if ((p->p_flag & SSYS) || p->p_as == &kas)
			error = EINVAL;
		else
			p->p_flag &= ~SPRFORK;
		prunlock(pnp);
		break;

	case PIOCSET:		/* set process flags */
	case PIOCRESET:		/* reset process flags */
	{
		register long flags = un.flags;

#define	ALLFLAGS	\
	(PR_FORK|PR_RLC|PR_KLC|PR_ASYNC|PR_BPTADJ|PR_MSACCT|PR_PCOMPAT)

		if ((p->p_flag & SSYS) || p->p_as == &kas)
			error = EINVAL;
		else if (flags & ~ALLFLAGS)
			error = EINVAL;
		else if (cmd == PIOCSET) {
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
			if (flags & PR_PCOMPAT) {
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
			if (flags & PR_PCOMPAT)
				p->p_flag &= ~STRC;
		}
		prunlock(pnp);
		break;
	}

	case PIOCGREG:		/* get general registers */
		if (t->t_state != TS_STOPPED && !VSTOPPED(t))
			bzero((caddr_t)un.regs, sizeof (un.regs));
		else {
			/* drop p_lock while touching the lwp's stack */
			mutex_exit(&p->p_lock);
			prgetprregs(lwp, un.regs);
			mutex_enter(&p->p_lock);
		}
		prunlock(pnp);
		if (copyout((caddr_t)un.regs, cmaddr, sizeof (prgregset_t)))
			error = EFAULT;
		break;

	case PIOCSREG:		/* set general registers */
		if (!ISTOPPED(t) && !VSTOPPED(t))
			error = EBUSY;
		else {
			/* drop p_lock while touching the lwp's stack */
			mutex_exit(&p->p_lock);
			prsetprregs(lwp, un.regs);
			mutex_enter(&p->p_lock);
		}
		prunlock(pnp);
		break;

	case PIOCGFPREG:	/* get floating-point registers */
		if (!prhasfp()) {
			prunlock(pnp);
			error = EINVAL;	/* No FP support */
			break;
		}

		if (t->t_state != TS_STOPPED && !VSTOPPED(t))
			bzero((caddr_t)&un.fpregs, sizeof (un.regs));
		else {
			/* drop p_lock while touching the lwp's stack */
			mutex_exit(&p->p_lock);
			prgetprfpregs(lwp, &un.fpregs);
			mutex_enter(&p->p_lock);
		}
		prunlock(pnp);
		if (copyout((caddr_t)&un.fpregs, cmaddr, sizeof (un.fpregs)))
			error = EFAULT;
		break;

	case PIOCSFPREG:	/* set floating-point registers */
		if (!prhasfp())
			error = EINVAL;	/* No FP support */
		else if (!ISTOPPED(t) && !VSTOPPED(t))
			error = EBUSY;
		else {
			/* drop p_lock while touching the lwp's stack */
			mutex_exit(&p->p_lock);
			prsetprfpregs(lwp, &un.fpregs);
			mutex_enter(&p->p_lock);
		}
		prunlock(pnp);
		break;

	case PIOCGXREGSIZE:	/* get the size of the extra registers */
	{
		int xregsize;

		if (prhasx()) {
			xregsize = prgetprxregsize();
			prunlock(pnp);
			if (copyout((caddr_t)&xregsize, cmaddr,
			    sizeof (xregsize)))
				error = EFAULT;
		} else {
			prunlock(pnp);
			error = EINVAL;	/* No extra register support */
		}
		break;
	}

	case PIOCGXREG:		/* get extra registers */
		if (prhasx()) {
			if (thing) {
				bzero(thing, thingsize);
				if (t->t_state == TS_STOPPED || VSTOPPED(t)) {
					/* drop p_lock to touch the stack */
					mutex_exit(&p->p_lock);
					prgetprxregs(lwp, thing);
					mutex_enter(&p->p_lock);
				}
				prunlock(pnp);
				if (copyout(thing, cmaddr, thingsize))
					error = EFAULT;
				kmem_free(thing, thingsize);
				thing = NULL;
			} else {
				prunlock(pnp);
			}
		} else {
			prunlock(pnp);
			error = EINVAL;	/* No extra register support */
		}
		break;

	case PIOCSXREG:		/* set extra registers */
		if (!ISTOPPED(t) && !VSTOPPED(t))
			error = EBUSY;
		else if (!prhasx())
			error = EINVAL;	/* No extra register support */
		else if (thing) {
			/* drop p_lock while touching the lwp's stack */
			mutex_exit(&p->p_lock);
			prsetprxregs(lwp, thing);
			mutex_enter(&p->p_lock);
		}
		prunlock(pnp);
		if (thing) {
			kmem_free(thing, thingsize);
			thing = NULL;
		}
		break;

	case PIOCSTATUS:	/* get process/lwp status */
		oprgetstatus(t, &un.prstat);
		prunlock(pnp);
		if (copyout((caddr_t)&un.prstat, cmaddr, sizeof (un.prstat)))
			error = EFAULT;
		break;

	case PIOCLSTATUS:	/* get status for process & all lwps */
	{
		int Nlwp;
		register int nlwp;
		prstatus_t *Bprsp;
		register prstatus_t *prsp;

		nlwp = Nlwp = p->p_lwpcnt;

		if (thing && thingsize != (Nlwp+1) * sizeof (prstatus_t)) {
			kmem_free(thing, thingsize);
			thing = NULL;
		}
		if (thing == NULL) {
			thingsize = (Nlwp+1) * sizeof (prstatus_t);
			thing = kmem_alloc(thingsize, KM_NOSLEEP);
		}
		if (thing == NULL) {
			prunlock(pnp);
			goto startover;
		}

		Bprsp = (prstatus_t *)thing;
		thing = NULL;
		prsp = Bprsp;
		oprgetstatus(t, prsp);
		t = p->p_tlist;
		do {
			ASSERT(!(t->t_proc_flag & TP_LWPEXIT));
			ASSERT(nlwp > 0);
			--nlwp;
			oprgetstatus(t, ++prsp);
		} while ((t = t->t_forw) != p->p_tlist);
		ASSERT(nlwp == 0);
		prunlock(pnp);
		if (copyout((caddr_t)Bprsp, cmaddr,
		    (Nlwp+1) * sizeof (prstatus_t)))
			error = EFAULT;

		kmem_free((caddr_t)Bprsp, (Nlwp+1) * sizeof (prstatus_t));
		break;
	}

	case PIOCPSINFO:	/* get ps(1) information */
	{
		prpsinfo_t *psp = &un.prps;

		oprgetpsinfo(p, psp,
		    (pnp->pr_type == PR_LWPIDFILE)? pcp->prc_thread : NULL);

		prunlock(pnp);
		if (copyout((caddr_t)&un.prps, cmaddr, sizeof (un.prps)))
			error = EFAULT;
		break;
	}

	case PIOCMAXSIG:	/* get maximum signal number */
	{
		int n = NSIG-1;

		prunlock(pnp);
		if (copyout((caddr_t)&n, cmaddr, sizeof (int)))
			error = EFAULT;
		break;
	}

	case PIOCACTION:	/* get signal action structures */
	{
		register u_int sig;
		register struct sigaction *sap = (struct sigaction *)thing;

		up = prumap(p);
		for (sig = 1; sig < NSIG; sig++)
			prgetaction(p, up, sig, &sap[sig-1]);
		prunmap(p);
		prunlock(pnp);
		if (copyout((caddr_t)sap, cmaddr,
		    (NSIG-1) * sizeof (struct sigaction)))
			error = EFAULT;
		kmem_free((caddr_t)sap, (NSIG-1) * sizeof (struct sigaction));
		thing = NULL;
		break;
	}

	case PIOCGHOLD:		/* get signal-hold mask */
	{
		sigktou(&t->t_hold, &un.holdmask);
		prunlock(pnp);
		if (copyout((caddr_t)&un.holdmask, cmaddr,
		    sizeof (un.holdmask)))
			error = EFAULT;
		break;
	}

	case PIOCSHOLD:		/* set signal-hold mask */
	{
		thread_lock(t);
		sigutok(&un.holdmask, &t->t_hold);
		sigdiffset(&t->t_hold, &cantmask);
		if (t->t_state == TS_SLEEP &&
		    (t->t_flag & T_WAKEABLE) &&
		    (fsig(&p->p_sig, t) || fsig(&t->t_sig, t)))
			setrun_locked(t);
		t->t_sig_check = 1;	/* so thread will see new holdmask */
		thread_unlock(t);
		prunlock(pnp);
		break;
	}

	case PIOCNMAP:		/* get number of memory mappings */
	{
		int n;
		struct as *as = p->p_as;

		if ((p->p_flag & SSYS) || as == &kas)
			n = 0;
		else {
			mutex_exit(&p->p_lock);
			AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);
			n = prnsegs(as, 0);
			AS_LOCK_EXIT(as, &as->a_lock);
			mutex_enter(&p->p_lock);
		}
		prunlock(pnp);
		if (copyout((caddr_t)&n, cmaddr, sizeof (int)))
			error = EFAULT;
		break;
	}

	case PIOCMAP:		/* get memory map information */
	{
		int n;
		prmap_t *prmapp;
		size_t size;
		struct as *as = p->p_as;
		int issys = ((p->p_flag & SSYS) || as == &kas);

		if (issys) {
			n = 1;
			prmapp = &un.prmap;
			bzero(prmapp, sizeof (*prmapp));
		} else {
			mutex_exit(&p->p_lock);
			AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);
			n = oprgetmap(p, &prmapp, &size);
			AS_LOCK_EXIT(as, &as->a_lock);
			mutex_enter(&p->p_lock);
		}
		prunlock(pnp);

		if (copyout((caddr_t)prmapp, cmaddr, n * sizeof (prmap_t)))
			error = EFAULT;
		if (!issys)
			kmem_free(prmapp, size);
		break;
	}

	case PIOCGFAULT:	/* get mask of traced faults */
		prassignset(&un.fltmask, &p->p_fltmask);
		prunlock(pnp);
		if (copyout((caddr_t)&un.fltmask, cmaddr, sizeof (un.fltmask)))
			error = EFAULT;
		break;

	case PIOCSFAULT:	/* set mask of traced faults */
		prassignset(&p->p_fltmask, &un.fltmask);
		if (!prisempty(&p->p_fltmask))
			p->p_flag |= SPROCTR;
		else if (sigisempty(&p->p_sigmask)) {
			up = prumap(p);
			if (up->u_systrap == 0)
				p->p_flag &= ~SPROCTR;
			prunmap(p);
		}
		prunlock(pnp);
		break;

	case PIOCCFAULT:	/* clear current fault */
		lwp->lwp_curflt = 0;
		prunlock(pnp);
		break;

	case PIOCCRED:		/* get process credentials */
	{
		register struct cred *cp;

		mutex_enter(&p->p_crlock);
		cp = p->p_cred;
		un.prcred.pr_euid = cp->cr_uid;
		un.prcred.pr_ruid = cp->cr_ruid;
		un.prcred.pr_suid = cp->cr_suid;
		un.prcred.pr_egid = cp->cr_gid;
		un.prcred.pr_rgid = cp->cr_rgid;
		un.prcred.pr_sgid = cp->cr_sgid;
		un.prcred.pr_ngroups = cp->cr_ngroups;
		mutex_exit(&p->p_crlock);

		prunlock(pnp);
		if (copyout((caddr_t)&un.prcred, cmaddr, sizeof (un.prcred)))
			error = EFAULT;
		break;
	}

	case PIOCGROUPS:	/* get supplementary groups */
	{
		register struct cred *cp;

		mutex_enter(&p->p_crlock);
		cp = p->p_cred;
		crhold(cp);
		mutex_exit(&p->p_crlock);

		prunlock(pnp);
		if (copyout((caddr_t)&cp->cr_groups[0], cmaddr,
		    max(cp->cr_ngroups, 1) * sizeof (cp->cr_groups[0])))
			error = EFAULT;
		crfree(cp);
		break;
	}

	case PIOCUSAGE:		/* get usage info */
	{
		/*
		 * For an lwp file descriptor, return just the lwp usage.
		 * For a process file descriptor, return total usage,
		 * all current lwps plus all defunct lwps.
		 */
		register prhusage_t *pup = &un.prhusage;

		bzero((caddr_t)pup, sizeof (prhusage_t));
		pup->pr_tstamp = gethrtime();

		if (pnp->pr_type == PR_LWPIDFILE) {
			t = pcp->prc_thread;
			if (t != NULL)
				prgetusage(t, pup);
			else
				error = ENOENT;
		} else {
			pup->pr_count  = p->p_defunct;
			pup->pr_create = p->p_mstart;
			pup->pr_term   = p->p_mterm;

			pup->pr_rtime    = p->p_mlreal;
			pup->pr_utime    = p->p_acct[LMS_USER];
			pup->pr_stime    = p->p_acct[LMS_SYSTEM];
			pup->pr_ttime    = p->p_acct[LMS_TRAP];
			pup->pr_tftime   = p->p_acct[LMS_TFAULT];
			pup->pr_dftime   = p->p_acct[LMS_DFAULT];
			pup->pr_kftime   = p->p_acct[LMS_KFAULT];
			pup->pr_ltime    = p->p_acct[LMS_USER_LOCK];
			pup->pr_slptime  = p->p_acct[LMS_SLEEP];
			pup->pr_wtime    = p->p_acct[LMS_WAIT_CPU];
			pup->pr_stoptime = p->p_acct[LMS_STOPPED];

			pup->pr_minf  = p->p_ru.minflt;
			pup->pr_majf  = p->p_ru.majflt;
			pup->pr_nswap = p->p_ru.nswap;
			pup->pr_inblk = p->p_ru.inblock;
			pup->pr_oublk = p->p_ru.oublock;
			pup->pr_msnd  = p->p_ru.msgsnd;
			pup->pr_mrcv  = p->p_ru.msgrcv;
			pup->pr_sigs  = p->p_ru.nsignals;
			pup->pr_vctx  = p->p_ru.nvcsw;
			pup->pr_ictx  = p->p_ru.nivcsw;
			pup->pr_sysc  = p->p_ru.sysc;
			pup->pr_ioch  = p->p_ru.ioch;

			/*
			 * Add the usage information for each active lwp.
			 */
			if ((t = p->p_tlist) != NULL &&
			    !(pcp->prc_flags & PRC_DESTROY)) {
				do {
					ASSERT(!(t->t_proc_flag & TP_LWPEXIT));
					pup->pr_count++;
					praddusage(t, pup);
				} while ((t = t->t_forw) != p->p_tlist);
			}
		}

		prunlock(pnp);
		prcvtusage(&un.prhusage);
		if (copyout((caddr_t)&un.prusage, cmaddr, sizeof (un.prusage)))
			error = EFAULT;
		break;
	}

	case PIOCLUSAGE:	/* get detailed usage info */
	{
		int Nlwp;
		register int nlwp;
		prhusage_t *Bpup;
		register prhusage_t *pup;
		hrtime_t curtime;

		nlwp = Nlwp = (pcp->prc_flags & PRC_DESTROY)? 0 : p->p_lwpcnt;

		if (thing && thingsize != (Nlwp+1) * sizeof (prhusage_t)) {
			kmem_free(thing, thingsize);
			thing = NULL;
		}
		if (thing == NULL) {
			thingsize = (Nlwp+1) * sizeof (prhusage_t);
			thing = kmem_alloc(thingsize, KM_NOSLEEP);
		}
		if (thing == NULL) {
			prunlock(pnp);
			goto startover;
		}

		pup = Bpup = (prhusage_t *)thing;
		thing = NULL;
		ASSERT(p == pcp->prc_proc);

		curtime = gethrtime();

		/*
		 * First the summation over defunct lwps.
		 */
		bzero((caddr_t)pup, sizeof (prhusage_t));
		pup->pr_count  = p->p_defunct;
		pup->pr_tstamp = curtime;
		pup->pr_create = p->p_mstart;
		pup->pr_term   = p->p_mterm;

		pup->pr_rtime    = p->p_mlreal;
		pup->pr_utime    = p->p_acct[LMS_USER];
		pup->pr_stime    = p->p_acct[LMS_SYSTEM];
		pup->pr_ttime    = p->p_acct[LMS_TRAP];
		pup->pr_tftime   = p->p_acct[LMS_TFAULT];
		pup->pr_dftime   = p->p_acct[LMS_DFAULT];
		pup->pr_kftime   = p->p_acct[LMS_KFAULT];
		pup->pr_ltime    = p->p_acct[LMS_USER_LOCK];
		pup->pr_slptime  = p->p_acct[LMS_SLEEP];
		pup->pr_wtime    = p->p_acct[LMS_WAIT_CPU];
		pup->pr_stoptime = p->p_acct[LMS_STOPPED];

		pup->pr_minf  = p->p_ru.minflt;
		pup->pr_majf  = p->p_ru.majflt;
		pup->pr_nswap = p->p_ru.nswap;
		pup->pr_inblk = p->p_ru.inblock;
		pup->pr_oublk = p->p_ru.oublock;
		pup->pr_msnd  = p->p_ru.msgsnd;
		pup->pr_mrcv  = p->p_ru.msgrcv;
		pup->pr_sigs  = p->p_ru.nsignals;
		pup->pr_vctx  = p->p_ru.nvcsw;
		pup->pr_ictx  = p->p_ru.nivcsw;
		pup->pr_sysc  = p->p_ru.sysc;
		pup->pr_ioch  = p->p_ru.ioch;

		prcvtusage(pup);

		/*
		 * Fill one prusage struct for each active lwp.
		 */
		if ((t = p->p_tlist) != NULL &&
		    !(pcp->prc_flags & PRC_DESTROY)) {
			do {
				ASSERT(!(t->t_proc_flag & TP_LWPEXIT));
				ASSERT(nlwp > 0);
				--nlwp;
				pup++;
				bzero((caddr_t)pup, sizeof (prhusage_t));
				pup->pr_tstamp = curtime;
				prgetusage(t, pup);
				prcvtusage(pup);
			} while ((t = t->t_forw) != p->p_tlist);
		}
		ASSERT(nlwp == 0);

		prunlock(pnp);
		if (copyout((caddr_t)Bpup, cmaddr,
		    (Nlwp+1) * sizeof (prusage_t)))
			error = EFAULT;
		kmem_free((caddr_t)Bpup, (Nlwp+1) * sizeof (prhusage_t));
		break;
	}

	case PIOCNAUXV:		/* get number of aux vector entries */
	{
		int n = NUM_AUX_VECTORS;

		prunlock(pnp);
		if (copyout((caddr_t)&n, cmaddr, sizeof (int)))
			error = EFAULT;
		break;
	}

	case PIOCAUXV:		/* get aux vector (see sys/auxv.h) */
	{
		up = prumap(p);
		bcopy((caddr_t)up->u_auxv, (caddr_t)un.auxv,
		    NUM_AUX_VECTORS * sizeof (auxv_t));
		prunmap(p);
		prunlock(pnp);
		if (copyout((caddr_t)un.auxv, cmaddr,
		    NUM_AUX_VECTORS * sizeof (auxv_t)))
			error = EFAULT;
		break;
	}

#if i386 && 0	/* USL, not Solaris */
	case PIOCGDBREG:	/* get debug registers */
	{
		dbregset_t *dbregs =  (dbregset_t *)thing;

		bzero((caddr_t)dbregs, sizeof (dbregset_t));
		prgetdbregs(lwp, dbregs);
		prunlock(pnp);
		if (copyout((caddr_t)dbregs, cmaddr, sizeof (dbregset_t)))
			error = EFAULT;
		kmem_free(dbregs, sizeof (dbregset_t));
		thing = NULL;
		break;
	}

	case PIOCSDBREG:	/* set debug registers */
	{
		dbregset_t *dbregs =  (dbregset_t *)thing;

		if (!ISTOPPED(t) && !VSTOPPED(t))
			error = EBUSY;
		else {
			if (copyin(cmaddr, (caddr_t)dbregs,
			    sizeof (dbregset_t)))
				error = EFAULT;
			else
				prsetdbregs(lwp, dbregs);
		}
		prunlock(pnp);
		kmem_free(dbregs, sizeof (dbregset_t));
		thing = NULL;
		break;
	}
#endif	/* i386 */
#if i386
	case PIOCNLDT:		/* get number of LDT entries */
	{
		int n;

		mutex_enter(&p->p_ldtlock);
		n = prnldt(p);
		mutex_exit(&p->p_ldtlock);
		prunlock(pnp);
		if (copyout((caddr_t)&n, cmaddr, sizeof (int)))
			error = EFAULT;
		break;
	}

	case PIOCLDT:		/* get LDT entries */
	{
		struct ssd *ssd;
		int n;

		mutex_enter(&p->p_ldtlock);
		n = prnldt(p);

		if (thing && thingsize != (n+1) * sizeof (struct ssd)) {
			kmem_free(thing, thingsize);
			thing = NULL;
		}
		if (thing == NULL) {
			thingsize = (n+1) * sizeof (struct ssd);
			thing = kmem_alloc(thingsize, KM_NOSLEEP);
		}
		if (thing == NULL) {
			mutex_exit(&p->p_ldtlock);
			prunlock(pnp);
			goto startover;
		}

		ssd = (struct ssd *)thing;
		thing = NULL;
		if (n != 0)
			prgetldt(p, ssd);
		mutex_exit(&p->p_ldtlock);
		prunlock(pnp);

		/* mark the end of the list with a null entry */
		bzero((caddr_t)&ssd[n], sizeof (struct ssd));
		if (copyout((caddr_t)ssd, cmaddr, (n+1) * sizeof (struct ssd)))
			error = EFAULT;
		kmem_free((caddr_t)ssd, (n+1) * sizeof (struct ssd));
		break;
	}
#endif	/* i386 */
#if sparc
	case PIOCGWIN:		/* get gwindows_t (see sys/reg.h) */
	{
		gwindows_t *gwp = (gwindows_t *)thing;

		/* drop p->p_lock while touching the stack */
		mutex_exit(&p->p_lock);
		bzero((caddr_t)gwp, sizeof (gwindows_t));
		prgetwindows(lwp, gwp);
		mutex_enter(&p->p_lock);
		prunlock(pnp);
		if (copyout((caddr_t)gwp, cmaddr, sizeof (gwindows_t)))
			error = EFAULT;
		kmem_free((caddr_t)gwp, sizeof (gwindows_t));
		thing = NULL;
		break;
	}
#endif	/* sparc */

	default:
		prunlock(pnp);
		error = EINVAL;
		break;

	}

	ASSERT(thing == NULL);
	ASSERT(xpnp == NULL);
	return (error);
}

/*
 * Distinguish "writeable" ioctl requests from others.
 */
static int
isprwrioctl(cmd)
	int cmd;
{
	switch (cmd) {
	case PIOCSTOP:
	case PIOCRUN:
	case PIOCSTRACE:
	case PIOCSSIG:
	case PIOCKILL:
	case PIOCUNKILL:
	case PIOCNICE:
	case PIOCSENTRY:
	case PIOCSEXIT:
	case PIOCSRLC:
	case PIOCRRLC:
	case PIOCSREG:
	case PIOCSFPREG:
	case PIOCSXREG:
	case PIOCSHOLD:
	case PIOCSFAULT:
	case PIOCCFAULT:
	case PIOCSFORK:
	case PIOCRFORK:
	case PIOCSET:
	case PIOCRESET:
		return (1);
	}
	return (0);
}

/*
 * Apply PIOCRUN options.
 */
static void
prsetrun(t, prp)
	register kthread_t *t;
	register prrun_t *prp;
{
	register proc_t *p = ttoproc(t);
	register klwp_t *lwp = ttolwp(t);
	register long flags = prp->pr_flags;
	register user_t *up;
	register int umapped = 0;

	ASSERT(MUTEX_HELD(&p->p_lock));

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
	if (flags & PRSABORT) {
		lwp->lwp_sysabort = 1;
		t->t_pre_sys = 1;	/* may not be stopped in pre_syscall */
	}
	if (flags & PRSHOLD) {
		sigutok(&prp->pr_sighold, &t->t_hold);
		sigdiffset(&t->t_hold, &cantmask);
		t->t_sig_check = 1;	/* so ISSIG will be done */
	}
	if (flags & PRSTRACE) {
		prdelset(&prp->pr_trace, SIGKILL);
		prassignset(&p->p_sigmask, &prp->pr_trace);
		if (!sigisempty(&p->p_sigmask))
			p->p_flag |= SPROCTR;
		else if (prisempty(&p->p_fltmask)) {
			if (umapped == 0) {
				up = prumap(p);
				umapped++;
			}
			if (up->u_systrap == 0)
				p->p_flag &= ~SPROCTR;
		}
	}
	if (flags & PRSFAULT) {
		prassignset(&p->p_fltmask, &prp->pr_fault);
		if (!prisempty(&p->p_fltmask))
			p->p_flag |= SPROCTR;
		else if (sigisempty(&p->p_sigmask)) {
			if (umapped == 0) {
				up = prumap(p);
				umapped++;
			}
			if (up->u_systrap == 0)
				p->p_flag &= ~SPROCTR;
		}
	}
	if (flags & PRCFAULT)
		lwp->lwp_curflt = 0;
	/*
	 * prsvaddr() must be called before prstep() because
	 * stepping can depend on the current value of the PC.
	 * We drop p_lock while touching the lwp's registers (on stack).
	 */
	if (flags & (PRSVADDR|PRSTEP)) {
		mutex_exit(&p->p_lock);
		if (flags & PRSVADDR)
			prsvaddr(lwp, prp->pr_vaddr);
		if (flags & PRSTEP)
			prstep(lwp, 0);
		mutex_enter(&p->p_lock);
	}
	if (umapped)
		prunmap(p);
}

extern u_int timer_resolution;

/*
 * Return old version of process/lwp status.
 * The u-block is mapped in by this routine and unmapped at the end.
 */
void
oprgetstatus(t, sp)
	register kthread_t *t;
	register prstatus_t *sp;
{
	register proc_t *p = ttoproc(t);
	register klwp_t *lwp = ttolwp(t);
	register long flags;
	register user_t *up;
	register kthread_t *aslwptp;
	u_long restonano;
	u_long instr;

	ASSERT(MUTEX_HELD(&p->p_lock));

	up = prumap(p);
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
		flags |= PR_PCOMPAT;
	if (t->t_proc_flag & TP_MSACCT)
		flags |= PR_MSACCT;
	if (t == p->p_aslwptp)
		flags |= PR_ASLWP;
	sp->pr_flags = flags;
	if (VSTOPPED(t)) {
		sp->pr_why   = PR_REQUESTED;
		sp->pr_what  = 0;
	} else {
		sp->pr_why   = t->t_whystop;
		sp->pr_what  = t->t_whatstop;
	}

	if (t->t_whystop == PR_FAULTED)
		bcopy((caddr_t)&lwp->lwp_siginfo,
		    (caddr_t)&sp->pr_info, sizeof (k_siginfo_t));
	else if (lwp->lwp_curinfo)
		bcopy((caddr_t)&lwp->lwp_curinfo->sq_info,
		    (caddr_t)&sp->pr_info, sizeof (k_siginfo_t));

	sp->pr_cursig  = lwp->lwp_cursig;
	if ((aslwptp = p->p_aslwptp) != NULL) {
		k_sigset_t set;

		set = aslwptp->t_sig;
		sigorset(&set, &p->p_notifsigs);
		prassignset(&sp->pr_sigpend, &set);
	} else {
		prassignset(&sp->pr_sigpend, &p->p_sig);
	}
	prassignset(&sp->pr_lwppend, &t->t_sig);
	prassignset(&sp->pr_sighold, &t->t_hold);
	sp->pr_altstack = lwp->lwp_sigaltstack;
	prgetaction(p, up, lwp->lwp_cursig, &sp->pr_action);
	sp->pr_pid   = p->p_pid;
	sp->pr_ppid  = p->p_ppid;
	sp->pr_pgrp  = p->p_pgrp;
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
	bcopy(sclass[t->t_cid].cl_name, sp->pr_clname,
	    min(sizeof (sclass[0].cl_name), sizeof (sp->pr_clname)-1));
	sp->pr_who = t->t_tid;
	sp->pr_nlwp = p->p_lwpcnt;
	sp->pr_brkbase = p->p_brkbase;
	sp->pr_brksize = p->p_brksize;
	sp->pr_stkbase = prgetstackbase(p);
	sp->pr_stksize = p->p_stksize;
	sp->pr_oldcontext = lwp->lwp_oldcontext;
	sp->pr_processor = t->t_cpu->cpu_id;
	sp->pr_bind = t->t_bind_cpu;
	prunmap(p);

	/*
	 * Fetch the current instruction, if not a system process.
	 * We don't attempt this unless the lwp is stopped.
	 */
	if ((p->p_flag & SSYS) || p->p_as == &kas)
		sp->pr_flags |= (PR_ISSYS|PR_PCINVAL);
	else if (!(flags & PR_STOPPED))
		sp->pr_flags |= PR_PCINVAL;
	else if (!prfetchinstr(lwp, &instr))
		sp->pr_flags |= PR_PCINVAL;
	else
		sp->pr_instr = (long)instr;

	/*
	 * Drop p_lock while touching the lwp's stack.
	 */
	mutex_exit(&p->p_lock);
	if (prisstep(lwp))
		sp->pr_flags |= PR_STEP;
	if ((t->t_state == TS_STOPPED &&
	    (t->t_whystop == PR_SYSENTRY || t->t_whystop == PR_SYSEXIT)) ||
	    (flags & PR_ASLEEP)) {
		int i;

		sp->pr_syscall = get_syscall_args(lwp,
			(int *)sp->pr_sysarg, &i);
		sp->pr_nsysarg = (short)i;
	}
	prgetprregs(lwp, sp->pr_reg);
	mutex_enter(&p->p_lock);
}

/*
 * Return old version of information used by ps(1).
 */
void
oprgetpsinfo(p, psp, tp)
	register proc_t *p;
	register prpsinfo_t *psp;
	kthread_t *tp;
{
	kthread_t *t;
	register char c, state;
	user_t *up;
	u_long hztime;
	dev_t d;
	u_long pct;
	clock_t ticks;
	int retval, niceval;
	struct cred *cred;
	struct as *as;

	ASSERT(MUTEX_HELD(&p->p_lock));

	bzero((caddr_t)psp, sizeof (struct prpsinfo));

	if ((t = tp) == NULL)
		t = prchoose(p);	/* returns locked thread */
	else
		thread_lock(t);

	/* kludge: map thread state enum into process state enum */

	if (t == NULL) {
		state = TS_ZOMB;
	} else {
		state = VSTOPPED(t) ? TS_STOPPED : t->t_state;
		thread_unlock(t);
	}

	switch (state) {
	case TS_SLEEP:		state = SSLEEP;		break;
	case TS_RUN:		state = SRUN;		break;
	case TS_ONPROC:		state = SONPROC;	break;
	case TS_ZOMB:		state = SZOMB;		break;
	case TS_STOPPED:	state = SSTOP;		break;
	default:		state = 0;		break;
	}
	switch (state) {
	case SSLEEP:	c = 'S';	break;
	case SRUN:	c = 'R';	break;
	case SZOMB:	c = 'Z';	break;
	case SSTOP:	c = 'T';	break;
	case SIDL:	c = 'I';	break;
	case SONPROC:	c = 'O';	break;
#ifdef SXBRK
	case SXBRK:	c = 'X';	break;
#endif
	default:	c = '?';	break;
	}
	psp->pr_state = state;
	psp->pr_sname = c;
	psp->pr_zomb = (state == SZOMB);
	psp->pr_flag = p->p_flag;

	mutex_enter(&p->p_crlock);
	cred = p->p_cred;
	psp->pr_uid = cred->cr_ruid;
	psp->pr_gid = cred->cr_rgid;
	psp->pr_euid = cred->cr_uid;
	psp->pr_egid = cred->cr_gid;
	mutex_exit(&p->p_crlock);

	psp->pr_pid = p->p_pid;
	psp->pr_ppid = p->p_ppid;
	psp->pr_pgrp = p->p_pgrp;
	psp->pr_sid = p->p_sessp->s_sid;
	psp->pr_addr = prgetpsaddr(p);
	hztime = p->p_utime + p->p_stime;
	psp->pr_time.tv_sec = hztime / timer_resolution;
	psp->pr_time.tv_nsec =
	    (hztime % timer_resolution) * (1000000000 / timer_resolution);
	hztime = p->p_cutime + p->p_cstime;
	psp->pr_ctime.tv_sec = hztime / timer_resolution;
	psp->pr_ctime.tv_nsec =
	    (hztime % timer_resolution) * (1000000000 / timer_resolution);
	if (p->p_aslwptp)
		psp->pr_aslwpid = p->p_aslwptp->t_tid;
	if (state == SZOMB || t == NULL) {
		extern int wstat(int, int);	/* needs a header file */
		int wcode = p->p_wcode;		/* must be atomic read */

		if (wcode)
			psp->pr_wstat = wstat(wcode, p->p_wdata);
		psp->pr_lttydev = PRNODEV;
		psp->pr_ottydev = PRNODEV;
		psp->pr_size = 0;
		psp->pr_rssize = 0;
		psp->pr_pctmem = 0;
	} else {
		up = prumap(p);
		psp->pr_wchan = t->t_wchan;
		psp->pr_pri = t->t_pri;
		bcopy(sclass[t->t_cid].cl_name, psp->pr_clname,
		    min(sizeof (sclass[0].cl_name), sizeof (psp->pr_clname)-1));
		retval = CL_DONICE(t, NULL, 0, &niceval);
		if (retval == 0) {
			psp->pr_oldpri = v.v_maxsyspri - psp->pr_pri;
			psp->pr_nice = niceval + NZERO;
		} else {
			psp->pr_oldpri = 0;
			psp->pr_nice = 0;
		}
		d = cttydev(p);
#ifdef sun
		{
			extern dev_t rwsconsdev, rconsdev, uconsdev;
			/*
			 * If the controlling terminal is the real
			 * or workstation console device, map to what the
			 * user thinks is the console device.
			 */
			if (d == rwsconsdev || d == rconsdev)
				d = uconsdev;
		}
#endif
		psp->pr_lttydev = (d == NODEV) ? PRNODEV : d;
		psp->pr_ottydev = cmpdev(d);
		psp->pr_start.tv_sec = up->u_start;
		psp->pr_start.tv_nsec = 0L;
		bcopy(up->u_comm, psp->pr_fname,
		    min(sizeof (up->u_comm), sizeof (psp->pr_fname)-1));
		bcopy(up->u_psargs, psp->pr_psargs,
		    min(PRARGSZ-1, PSARGSZ));
		psp->pr_syscall = t->t_sysnum;
		psp->pr_argc = up->u_argc;
		psp->pr_argv = up->u_argv;
		psp->pr_envp = up->u_envp;
		prunmap(p);

		/* compute %cpu for the lwp or process */
		ticks = lbolt;
		pct = 0;
		if ((t = tp) == NULL)
			t = p->p_tlist;
		do {
			pct += cpu_decay(t->t_pctcpu, ticks - t->t_lbolt - 1);
			if (tp != NULL)		/* just do the one lwp */
				break;
		} while ((t = t->t_forw) != p->p_tlist);

		/* prorate over the online cpus so we don't exceed 100% */
		if (ncpus > 1)
			pct /= ncpus;
		if (pct > 0x8000)	/* might happen, due to rounding */
			pct = 0x8000;
		psp->pr_pctcpu = pct;
		psp->pr_cpu = (pct*100 + 0x6000) >> 15;	/* [0..99] */
		if (psp->pr_cpu > 99)
			psp->pr_cpu = 99;

		if ((p->p_flag & SSYS) || (as = p->p_as) == &kas) {
			psp->pr_size = 0;
			psp->pr_rssize = 0;
			psp->pr_pctmem = 0;
		} else {
			mutex_exit(&p->p_lock);
			AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
			psp->pr_size = btoc(rm_assize(as));
			psp->pr_rssize = rm_asrss(as);
			psp->pr_pctmem = rm_pctmemory(as);
			AS_LOCK_EXIT(as, &as->a_lock);
			mutex_enter(&p->p_lock);
		}
	}
	psp->pr_bysize = ctob(psp->pr_size);
	psp->pr_byrssize = ctob(psp->pr_rssize);
}

/*
 * Return an array of structures with memory map information.
 * In addition to the "real" segments we need space for the
 * zero-filled entry that terminates the list.
 * We allocate here; the caller must deallocate.
 */
#define	MAPSIZE	8192
int
oprgetmap(proc_t *p, prmap_t **prmapp, size_t *sizep)
{
	struct as *as = p->p_as;
	int nmaps = 0;
	prmap_t *mp;
	size_t size;
	struct seg *seg;
	struct seg *brkseg, *stkseg;
	int prot;

	ASSERT(as != &kas && AS_WRITE_HELD(as, &as->a_lock));

	/* initial allocation */
	*sizep = size = MAPSIZE;
	*prmapp = mp = kmem_alloc(MAPSIZE, KM_SLEEP);

	if ((seg = AS_SEGP(as, as->a_segs)) == NULL) {
		bzero(mp, sizeof (*mp));
		return (1);
	}

	brkseg = as_segat(as, p->p_brkbase + p->p_brksize - 1);
	stkseg = as_segat(as, prgetstackbase(p));

	do {
		caddr_t naddr;
		caddr_t saddr = seg->s_base;
		caddr_t eaddr = seg->s_base + seg->s_size;
		void *tmp = NULL;

		do {
			prot = pr_getprot(seg, 0, &tmp, &saddr, &naddr);
			if (saddr == naddr)
				continue;
			/* reallocate if necessary */
			if ((nmaps + 2) * sizeof (prmap_t) > size) {
				size_t newsize = size + MAPSIZE;
				prmap_t *newmp = kmem_alloc(newsize, KM_SLEEP);

				bcopy(*prmapp, newmp, nmaps * sizeof (prmap_t));
				kmem_free(*prmapp, size);
				*sizep = size = newsize;
				*prmapp = newmp;
				mp = newmp + nmaps;
			}
			bzero(mp, sizeof (*mp));
			mp->pr_vaddr = saddr;
			mp->pr_size = naddr - saddr;
			mp->pr_off = SEGOP_GETOFFSET(seg, saddr);
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
			else if (seg == stkseg)
				mp->pr_mflags |= MA_STACK;
			mp->pr_pagesize = PAGESIZE;
			mp++;
			nmaps++;
		} while ((saddr = naddr) != eaddr);
		ASSERT(tmp == NULL);
	} while ((seg = AS_SEGP(as, seg->s_next)) != NULL);

	bzero(mp, sizeof (*mp));
	return (nmaps + 1);
}

/*
 * Return the size of the old /proc page data file.
 */
long
oprpdsize(as)
	register struct as *as;
{
	register struct seg *seg;
	register long size;

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
				size += sizeof (prasmap_t) + round4(npage);
		} while ((saddr = naddr) != eaddr);
		ASSERT(tmp == NULL);
	} while ((seg = AS_SEGP(as, seg->s_next)) != NULL);

	return (size);
}

/*
 * Read old /proc page data information.
 * The process is locked and the address space will not grow.
 */
int
oprpdread(as, hatid, uiop)
	struct as *as;
	u_int hatid;
	struct uio *uiop;
{
	caddr_t buf;
	long size;
	register prpageheader_t *php;
	register prasmap_t *pmp;
	struct seg *seg;
	int error;

	AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);

	if ((seg = AS_SEGP(as, as->a_segs)) == NULL) {
		AS_LOCK_EXIT(as, &as->a_lock);
		return (0);
	}
	size = oprpdsize(as);
	ASSERT(size > 0);
	if (uiop->uio_resid < size) {
		AS_LOCK_EXIT(as, &as->a_lock);
		return (E2BIG);
	}

	buf = kmem_alloc(size, KM_SLEEP);
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
			register u_int len;
			register int npage;
			register int prot;

			prot = pr_getprot(seg, 0, &tmp, &saddr, &naddr);
			if ((len = naddr - saddr) == 0)
				continue;
			npage = len/PAGESIZE;
			ASSERT(npage > 0);
			php->pr_nmap++;
			php->pr_npage += npage;
			pmp->pr_vaddr = saddr;
			pmp->pr_npage = npage;
			pmp->pr_off = SEGOP_GETOFFSET(seg, saddr);
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
			hat_getstatby(as, saddr, len, hatid,
			    (char *)(pmp+1), 1);
			pmp = (prasmap_t *)((caddr_t)(pmp+1) + round4(npage));
		} while ((saddr = naddr) != eaddr);
		ASSERT(tmp == NULL);
	} while ((seg = AS_SEGP(as, seg->s_next)) != NULL);

	AS_LOCK_EXIT(as, &as->a_lock);

	ASSERT((caddr_t)pmp == buf+size);
	error = uiomove(buf, size, UIO_READ, uiop);
	kmem_free(buf, size);

	return (error);
}
