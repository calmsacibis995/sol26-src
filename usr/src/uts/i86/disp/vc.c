/*      Copyright (c) 1990, 1991 UNIX System Laboratories, Inc. */
/*      Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T   */
/*        All Rights Reserved   */

/*      THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF          */
/*      UNIX System Laboratories, Inc.                          */
/*      The copyright notice above does not evidence any        */
/*      actual or intended publication of such source code.     */

#pragma	ident	"@(#)vc.c	1.20	96/10/17	SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/priocntl.h>
#include <sys/class.h>
#include <sys/disp.h>
#include <sys/procset.h>
#include <sys/debug.h>
#include <sys/vc.h>
#include <sys/vcpriocntl.h>
#include <sys/kmem.h>
#include <sys/errno.h>
#include <sys/cpuvar.h>
#include <sys/systm.h>
#include <sys/vmsystm.h>
#include <sys/cmn_err.h>

#include <vm/rm.h>
#include <vm/seg_kmem.h>

#ifdef _VPIX

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

char _depends_on[] = "sched/VC_DPTBL";
extern void vc_init();

static struct sclass csw = {
	"VC",
	vc_init,
	0
};

extern struct mod_ops mod_schedops;

/*
 * Module linkage information for the kernel.
 */
static struct modlsched modlsched = {
	&mod_schedops, "VP/ix process sched class", &csw
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlsched, NULL
};

static int module_keepcnt = 0;	/* ==0 means the module is unloadable */

_init()
{
	return (mod_install(&modlinkage));
}

_fini()
{
	if (module_keepcnt != 0)
		return (EBUSY);

	return (mod_remove(&modlinkage));
}

_info(modinfop)
	struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * Class specific code for the VP/ix process class
 */


/*
 * Extern declarations for variables defined in the vc master file
 */
#define	VCMAXUPRI 20

pri_t	vc_maxupri = VCMAXUPRI;	/* max VP/ix process user priority */
pri_t	vc_maxumdpri;		/* maximum user mode vc priority */

vcdpent_t  *vc_dptbl;	/* VP/ix process disp parameter table */
pri_t	*vc_kmdpris;	/* array of global pris used by vc procs when */
			/*  sleeping or running in kernel after sleep */


#define	VCPMEMSZ	 2048	/* request size for vcproc memory allocation */

#define	vcumdpri	(vcpp->vc_umdpri)
#define	vcmedumdpri	(vc_maxumdpri >> 1)

/*
 *  VC_NEWUMDPRI is similar to TS_NEWUMDPRI except that we forbid both
 *  cpupri and umdpri from taking on the reserved busywait priorities
 *  that end in the digits 0, 1, 2.  We have to restrict both cpupri and
 *  umdpri because they are both used as indices into the dispatch table.
 *  VC_NEWUMDPRI should never be applied to a busywaiting process.
 */
#define VC_NEWUMDPRI(vcpp)      \
{ \
int tmp; \
if ((tmp = (vcpp)->vc_cpupri % 10) <= 2) \
	(vcpp)->vc_cpupri += 3 - tmp; \
if (((vcpp)->vc_umdpri = (vcpp)->vc_cpupri + (vcpp)->vc_upri) > vc_maxumdpri) \
	(vcpp)->vc_umdpri = vc_maxumdpri; \
else if ((vcpp)->vc_umdpri < 0) \
	(vcpp)->vc_umdpri = 0; \
if ((tmp = (vcpp)->vc_umdpri % 10) <= 2) \
	(vcpp)->vc_umdpri += 3 - tmp; \
}

void		vc_boost(), vc_busywait(), vc_init();
static int	vc_admin(), vc_enterclass(), vc_fork(), vc_getclinfo();
static int	vc_nosys(), vc_parmsin(), vc_parmsout(), vc_parmsset();
static void	vc_exitclass(), vc_forkret();
static void	vc_nullsys(), vc_parmsget(), vc_preempt();
static void	vc_setrun(), vc_sleep();
static pri_t	vc_swapin(kthread_id_t, int), vc_swapout(kthread_id_t, int);
static void	vc_set_process_group(pid_t sid, pid_t bg_pgid, pid_t fg_pgid);
static void	vc_tick(), vc_trapret(), vc_update(), vc_wakeup();
static cpu_t *	vc_cpu_choose(kthread_id_t t, pri_t tpri);
static pri_t	vc_globpri(vcproc_t *vcprocp);
extern vcdpent_t *vc_getdptbl(void);
extern pri_t *vc_getkmdpris(void);
extern pri_t vc_getmaxumdpri(void);
extern pri_t vc_maxkmdpri; /* maximum kernel mode vc priority */

static pri_t	vc_maxglobpri;	/* maximum global priority used by vc class */
static id_t	vc_cid;		/* VP/ix process class ID */
static vcproc_t	vc_plisthead;	/* dummy vcproc at head of vcproc list */
kmutex_t	vc_dptblock;	/* protects VP/ix process dispatch table */
kmutex_t	vc_list_lock;	/* protects VP/ix class thread list */


static struct classfuncs vc_classfuncs = {
	/* class functions */
	vc_admin,
	vc_getclinfo,
	vc_parmsin,
	vc_parmsout,

	/* thread functions */
	vc_enterclass,
	vc_exitclass,
	vc_fork,
	vc_forkret,
	vc_parmsget,
	vc_parmsset,
	vc_nullsys,	/* stop */
	vc_swapin,
	vc_swapout,
	vc_trapret,
	vc_preempt,
	vc_setrun,
	vc_sleep,
	vc_tick,
	vc_wakeup,
	vc_nosys,	/* donice */
	vc_cpu_choose,
	vc_globpri,
	vc_set_process_group,
};

/*
 * Time sharing class initialization.  Called by dispinit() at boot time.
 * We can ignore the clparmsz argument since we know that the smallest
 * possible parameter buffer is big enough for us.
 */
/* ARGSUSED */
void
vc_init(cid, clparmsz, clfuncspp, maxglobprip)
	id_t		cid;
	int		clparmsz;
	classfuncs_t	**clfuncspp;
	pri_t		*maxglobprip;
{

	vc_dptbl = vc_getdptbl();
	vc_kmdpris = vc_getkmdpris();
	vc_maxumdpri = vc_getmaxumdpri();
	vc_maxglobpri = max(vc_kmdpris[vc_maxkmdpri],
	    vc_dptbl[vc_maxumdpri].vc_globpri);

	vc_cid = cid;		/* Record our class ID */

	/*
	 * Initialize the vcproc list.
	 */
	vc_plisthead.vc_next = vc_plisthead.vc_prev = &vc_plisthead;

	/*
	 * We're required to return a pointer to our classfuncs
	 * structure and the highest global priority value we use.
	 */
	*clfuncspp = &vc_classfuncs;
	*maxglobprip = vc_maxglobpri;
	mutex_init(&vc_dptblock, "vc dispatch tbl lock", MUTEX_DEFAULT, NULL);
	mutex_init(&vc_dptblock, "vc list lock", MUTEX_DEFAULT, NULL);
}


/*
 * Get or reset the vc_dptbl values per the user's request.
 */
/* ARGSUSED */
static int
vc_admin(uaddr, reqpcredp)
	caddr_t	uaddr;
	cred_t	*reqpcredp;
{
	vcadmin_t		vcadmin;
	register vcdpent_t	*tmpdpp;
	register int		userdpsz;
	register int		i;
	register int		vcdpsz;

	if (copyin(uaddr, (caddr_t)&vcadmin, sizeof (vcadmin_t)))
		return (EFAULT);

	vcdpsz = (vc_maxumdpri + 1) * sizeof (vcdpent_t);

	switch (vcadmin.vc_cmd) {

	case VC_GETDPSIZE:

		vcadmin.vc_ndpents = vc_maxumdpri + 1;
		if (copyout((caddr_t)&vcadmin, uaddr, sizeof (vcadmin_t))) {
			return (EFAULT);
		}
		break;

	case VC_GETDPTBL:

		userdpsz = min(vcadmin.vc_ndpents * sizeof (vcdpent_t),
		    vcdpsz);
		if (copyout((caddr_t)vc_dptbl,
		    (caddr_t)vcadmin.vc_dpents, userdpsz)) {
			return (EFAULT);
		}

		vcadmin.vc_ndpents = userdpsz / sizeof (vcdpent_t);
		if (copyout((caddr_t)&vcadmin, uaddr, sizeof (vcadmin_t))) {
			return (EFAULT);
		}
		break;

	case VC_SETDPTBL:

		/*
		 * We require that the requesting process have super user
		 * priveleges.  We also require that the table supplied by
		 * the user exactly match the current vc_dptbl in size.
		 */
		if (!suser(reqpcredp)) {
			return (EPERM);
		}
		if (vcadmin.vc_ndpents * sizeof (vcdpent_t) != vcdpsz) {
			return (EINVAL);
		}

		/*
		 * We read the user supplied table into a temporary buffer
		 * where it is validated before being copied over the
		 * vc_dptbl.
		 */
		tmpdpp = kmem_alloc(vcdpsz, KM_SLEEP);
		if (copyin((caddr_t)vcadmin.vc_dpents, (caddr_t)tmpdpp,
		    vcdpsz)) {
			kmem_free(tmpdpp, vcdpsz);
			return (EFAULT);
		}
		for (i = 0; i < vcadmin.vc_ndpents; i++) {

			/*
			 * Validate the user supplied values.  All we are doing
			 * here is verifying that the values are within their
			 * allowable ranges and will not panic the system.  We
			 * make no attempt to ensure that the resulting
			 * configuration makes sense or results in reasonable
			 * performance.
			 */
			if (tmpdpp[i].vc_quantum <= 0) {
				kmem_free(tmpdpp, vcdpsz);
				return (EINVAL);
			}
			if (tmpdpp[i].vc_tqexp > vc_maxumdpri ||
			    tmpdpp[i].vc_tqexp < 0) {
				kmem_free(tmpdpp, vcdpsz);
				return (EINVAL);
			}
			if (tmpdpp[i].vc_slpret > vc_maxumdpri ||
			    tmpdpp[i].vc_slpret < 0) {
				kmem_free(tmpdpp, vcdpsz);
				return (EINVAL);
			}
			if (tmpdpp[i].vc_maxwait < 0) {
				kmem_free(tmpdpp, vcdpsz);
				return (EINVAL);
			}
			if (tmpdpp[i].vc_lwait > vc_maxumdpri ||
			    tmpdpp[i].vc_lwait < 0) {
				kmem_free(tmpdpp, vcdpsz);
				return (EINVAL);
			}
		}

		/*
		 * Copy the user supplied values over the current vc_dptbl
		 * values.  The vc_globpri member is read-only so we don't
		 * overwrite it.
		 */
		mutex_enter(&vc_dptblock);
		for (i = 0; i < vcadmin.vc_ndpents; i++) {
			vc_dptbl[i].vc_quantum = tmpdpp[i].vc_quantum;
			vc_dptbl[i].vc_tqexp = tmpdpp[i].vc_tqexp;
			vc_dptbl[i].vc_slpret = tmpdpp[i].vc_slpret;
			vc_dptbl[i].vc_maxwait = tmpdpp[i].vc_maxwait;
			vc_dptbl[i].vc_lwait = tmpdpp[i].vc_lwait;
		}
		mutex_exit(&vc_dptblock);
		kmem_free(tmpdpp, vcdpsz);
		break;
	default:
		return (EINVAL);
	}
	return (0);
}


/*
 * Allocate a VP/ix process class specific thread structure and
 * initialize it with the parameters supplied. Also move the thread
 * to specified VP/ix process priority.
 */
/* ARGSUSED */
static int
vc_enterclass(t, cid, vcparmsp, reqpcredp)
	kthread_id_t	t;
	id_t		cid;
	vcparms_t	*vcparmsp;
	cred_t		*reqpcredp;
{
	vcproc_t		*vcpp;
	register pri_t		reqtsuprilim;
	register pri_t		reqtsupri;
	register int 		oldlvl;
	boolean_t		wasonq;
	static int		vcpexists = 0;	/* set on first occurence of */
						/*   a VP/ix class process */

	/*  Although VP/ix class processes have some aspects of real
	 *  time processes, unlike the real time process class we do
 	 *  not force the process to be loaded at this point.
 	 *  Typically it will be loaded because we are running v86init()
 	 *  in the process itself.  Even if future development introduces
 	 *  a new way to create a VP/ix process, it would probably still
 	 *  not be necessary to force the process in.
 	 */

	module_keepcnt++;
	vcpp = kmem_alloc(sizeof (vcproc_t), KM_SLEEP);

	/*
	 * Initialize the vcproc structure.
	 */
	if (vcparmsp == NULL) {
		/*
		 * Use default values.
		 */
		vcpp->vc_uprilim = vcpp->vc_upri = 0;
		vcpp->vc_nice = 20;
		vcpp->vc_umdpri = vcpp->vc_cpupri = vcmedumdpri;
	} else {
		/*
		 * Use supplied values.
		 */
		if (vcparmsp->vc_uprilim == VC_NOCHANGE)
				reqtsuprilim = 0;
		else {
			if (vcparmsp->vc_uprilim > 0 && !suser(reqpcredp))
				return (EPERM);
			reqtsuprilim = vcparmsp->vc_uprilim;
		}

		if (vcparmsp->vc_upri == VC_NOCHANGE) {
			reqtsupri = reqtsuprilim;
		} else {
			if (vcparmsp->vc_upri > 0 && !suser(reqpcredp))
				return (EPERM);
			/*
			 * Set the user priority to the requested value
			 * or the upri limit, whichever is lower.
			 */
			reqtsupri = vcparmsp->vc_upri;
			if (reqtsupri > reqtsuprilim)
				reqtsupri = reqtsuprilim;
		}


		vcpp->vc_uprilim = reqtsuprilim;
		vcpp->vc_upri = reqtsupri;
		vcpp->vc_nice = 20 - (20 * reqtsupri) / vc_maxupri;
		vcpp->vc_cpupri = vcmedumdpri;
		VC_NEWUMDPRI(vcpp);
	}

	vcpp->vc_dispwait = 0;
	vcpp->vc_flags = 0;
	vcpp->vc_tp = t;

	/*
	 * Reset priority. Process goes to a "user mode" priority
	 * here regardless of whether or not it has slept since
	 * entering the kernel.
	 */
	thread_lock(t);			/* get dispatcher lock on thread */
	t->t_clfuncs = &(sclass[cid].cl_funcs->thread);
	t->t_cid = cid;
	t->t_cldata = (void *)vcpp;
	if (t == curthread) {
		t->t_pri = vc_dptbl[vcumdpri].vc_globpri;
		if (DISP_PRIO(t) > DISP_MAXRUNPRI(t))
			vcpp->vc_timeleft = vc_dptbl[vcumdpri].vc_quantum;
		else {
			vcpp->vc_flags |= VCBACKQ;
			cpu_surrender(t);
		}
	} else {
		wasonq = dispdeq(t);
		t->t_pri = vc_dptbl[vcumdpri].vc_globpri;
		vcpp->vc_timeleft = vc_dptbl[vcumdpri].vc_quantum;
		if (wasonq == B_TRUE) {
			setbackdq(t);
		} else {
			vcpp->vc_flags |= VCBACKQ;
			if (t->t_state == TS_ONPROC)
				cpu_surrender(t);
		}
	}
	thread_unlock(t);

	/*
	 * Link new structure into vcproc list.
	 */
	mutex_enter(&vc_list_lock);
	vcpp->vc_next = vc_plisthead.vc_next;
	vcpp->vc_prev = &vc_plisthead;
	vc_plisthead.vc_next->vc_prev = vcpp;
	vc_plisthead.vc_next = vcpp;

	/*
	 * If this is the first VP/ix process thread to occur since
	 * boot we set up the initial call to vc_update() here.
	 */
	if (vcpexists)
		mutex_exit(&vc_list_lock);
	else {
		vcpexists = 1;
		mutex_exit(&vc_list_lock);
		(void) timeout(vc_update, 0, hz);
	}

	return (0);
}


/*
 * Free vcproc structure of thread.
 */
static void
vc_exitclass(vcprocp)
	vcproc_t *vcprocp;
{
	mutex_enter(&vc_list_lock);
	vcprocp->vc_prev->vc_next = vcprocp->vc_next;
	vcprocp->vc_next->vc_prev = vcprocp->vc_prev;
	mutex_exit(&vc_list_lock);
	kmem_free((caddr_t)vcprocp, sizeof (vcproc_t));
	module_keepcnt--;
}


static int
vc_fork(t, ct)
	kthread_id_t t;
	kthread_id_t ct;
{
	register vcproc_t	*ptspp; /* ptr to parent's vcproc structure */
	register vcproc_t	*ctspp; /* ptr to child's vcproc structure */

	ASSERT(MUTEX_HELD(&ttoproc(t)->p_lock));

	ctspp = kmem_alloc(sizeof (vcproc_t), KM_SLEEP);
	module_keepcnt++;
	ptspp = (vcproc_t *)t->t_cldata;
	/*
	 * Initialize child's vcproc structure.
	 */
	thread_lock(t);
	ctspp->vc_timeleft = vc_dptbl[ptspp->vc_umdpri].vc_quantum;
	ctspp->vc_umdpri = ptspp->vc_umdpri;
	ctspp->vc_cpupri = ptspp->vc_cpupri;
	ctspp->vc_uprilim = ptspp->vc_uprilim;
	ctspp->vc_upri = ptspp->vc_upri;
	ctspp->vc_nice = ptspp->vc_nice;
	ctspp->vc_dispwait = 0;
	ctspp->vc_flags = ptspp->vc_flags;
	ctspp->vc_tp = ct;
	thread_unlock(t);

	/*
	 * Link new structure into vcproc list.
	 */
	ct->t_cldata = (void *)ctspp;
	mutex_enter(&vc_list_lock);
	ctspp->vc_next = vc_plisthead.vc_next;
	ctspp->vc_prev = &vc_plisthead;
	vc_plisthead.vc_next->vc_prev = ctspp;
	vc_plisthead.vc_next = ctspp;
	mutex_exit(&vc_list_lock);
	return (0);
}

/*
 * Child is placed at back of dispatcher queue and parent gives
 * up processor so that the child runs first after the fork.
 * This allows the child immediately execing to break the multiple
 * use of copy on write pages with no disk home. The parent will
 * get to steal them back rather than uselessly copying them.
 *
 * XXX Note: We are assuming that a VP/ix process will never have more than
 *	     one lwp. This routine needs review if we need to support 
 *	     multiple lwps.
 */

static void
vc_forkret(t, ct)
	register kthread_id_t	t;
	register kthread_id_t	ct;
{
	vcproc_t *vcpp;
	proc_t	*pp = ttoproc(t);
	proc_t	*cp = ttoproc(ct);
	id_t	tsclass;
	id_t	oldcid;

	ASSERT(t == curthread);
	ASSERT(MUTEX_HELD(&pidlock));

	/*
	 * If time-sharing process class is properly configured and
	 * we can successfully enroll this process in it, switch it
	 * to time-sharing.
	 */
	vcpp = (vcproc_t *)ct->t_cldata;
	oldcid  = ct->t_cid;
	mutex_enter(&cp->p_lock);	/* hold p_lock for CL_ENTERCLASS() */
	if (getcid("TS", &tsclass) == 0 && (tsclass > 0) &&
		CL_ENTERCLASS(ct, tsclass, NULL, NULL) == 0) {
		/* Withdraw process from old class */
		CL_EXITCLASS(oldcid, vcpp); 
		/* XXX - temporary workaround to make ins/outs work for ECT 
		 *	 (see v86init()).
		 */
		lwptoregs(ttolwp(ct))->r_ps &= ~PS_IOPL; /* make IOPL 0 */
	}
	else {
		/*
		 * For some reason we were unable to switch the process back
	 	 * to time-sharing.  There is no provision for failure from this
	 	 * routine, so just let the process remain as a VP/ix one.  We
	 	 * do not really expect this to happen, so maybe a warning would
	 	 * be appropriate.
		 * XXX - ???
	 	 */  
		cmn_err(CE_WARN, 
			"vc_forkret: failed to enter a proc in the TS class");
	}

	/*
	 * Hold the child's p_lock before dropping pidlock to ensure
	 * the process does not disappear before we set it running.
	 */
	mutex_exit(&pidlock);
	continuelwps(cp);
	mutex_exit (&cp->p_lock);

	mutex_enter (&pp->p_lock);
	continuelwps(pp);
	mutex_exit (&pp->p_lock);

	thread_lock(t);
	vcpp = (vcproc_t *)(t->t_cldata);
	vcpp->vc_cpupri = vc_dptbl[vcpp->vc_cpupri].vc_tqexp;
	VC_NEWUMDPRI(vcpp);
	vcpp->vc_timeleft = vc_dptbl[vcumdpri].vc_quantum;
	vcpp->vc_dispwait = 0;
	t->t_pri = vc_dptbl[vcumdpri].vc_globpri;
	vcpp->vc_flags &= ~(VCKPRI | VCSLEPT);
	vc_setrun(t);
	thread_unlock(t);

	swtch();
}


/*
 * Get information about the VP/ix process class into the buffer
 * pointed to by vcinfop. The maximum configured user priority
 * is the only information we supply.  We ignore the class and
 * credential arguments because anyone can have this information.
 */
/* ARGSUSED */
static int
vc_getclinfo(vcinfop, reqpcid, reqpcredp)
	vcinfo_t	*vcinfop;
	id_t		reqpcid;
	cred_t		*reqpcredp;
{
	vcinfop->vc_maxupri = vc_maxupri;
	return (0);
}


static int
vc_nosys()
{
	return (ENOSYS);
}


static void
vc_nullsys()
{
}


/*
 * Get the VP/ix process parameters of the thread pointed to by
 * vcprocp into the buffer pointed to by vcparmsp.
 */
static void
vc_parmsget(vcprocp, vcparmsp)
	vcproc_t	*vcprocp;
	vcparms_t	*vcparmsp;
{
	vcparmsp->vc_uprilim = vcprocp->vc_uprilim;
	vcparmsp->vc_upri = vcprocp->vc_upri;
}


/*
 * Check the validity of the VP/ix process parameters in the buffer
 * pointed to by vcparmsp. If our caller passes us a non-NULL
 * reqpcredp pointer we also verify that the requesting thread
 * (whose credentials are pointed to by reqpcredp) has the necessary
 * permissions to set these parameters for the target thread.
 */
/* ARGSUSED */
static int
vc_parmsin(vcparmsp, reqpcid, reqpcredp, targpcid, targpcredp, vcpp)
	register vcparms_t	*vcparmsp;
	id_t			reqpcid;
	cred_t			*reqpcredp;
	id_t			targpcid;
	cred_t			*targpcredp;
	vcproc_t		*vcpp;
{
	/*
	 * Check validity of parameters.
	 */
	if ((vcparmsp->vc_uprilim > vc_maxupri ||
	    vcparmsp->vc_uprilim < -vc_maxupri) &&
	    vcparmsp->vc_uprilim != VC_NOCHANGE)
		return (EINVAL);

	if ((vcparmsp->vc_upri > vc_maxupri ||
	    vcparmsp->vc_upri < -vc_maxupri) &&
	    vcparmsp->vc_upri != VC_NOCHANGE)
		return (EINVAL);

	if (reqpcredp == NULL || vcparmsp->vc_uprilim == VC_NOCHANGE)
		return (0);

	/*
	 * Our caller passed us non-NULL credential pointers so
	 * we are being asked to check permissions as well as
	 * the validity of the parameters.  The basic rules are
	 * that the calling thread must be super-user in order
	 * to raise the target thread' upri limit above its
	 * current value.  If the target thread is not currently
	 * VP/ix process, the calling thread must be super-user in
	 * order to set a upri limit greater than zero.
	 */
	if (targpcid == vc_cid) {
		if (vcparmsp->vc_uprilim > vcpp->vc_uprilim &&
		    !suser(reqpcredp))
			return (EPERM);
	} else {
		if (vcparmsp->vc_uprilim > 0 && !suser(reqpcredp))
			return (EPERM);
	}

	return (0);
}


/*
 * Nothing to do here but return success.
 */
static int
vc_parmsout()
{
	return (0);
}


/*
 * Set the scheduling parameters of the thread pointed to by vcprocp
 * to those specified in the buffer pointed to by vcparmsp.
 */
/* ARGSUSED */
static int
vc_parmsset(vcparmsp, vcpp, reqpcid, reqpcredp)
	register vcparms_t	*vcparmsp;
	register vcproc_t	*vcpp;
	id_t			reqpcid;
	cred_t			*reqpcredp;
{
	register int		oldlvl;
	boolean_t		wasonq;
	register char		nice;
	register pri_t		reqtsuprilim;
	register pri_t		reqtsupri;
	kthread_id_t		tx;

	ASSERT(MUTEX_HELD(&pidlock));

	if (vcparmsp->vc_uprilim == VC_NOCHANGE)
			reqtsuprilim = vcpp->vc_uprilim;
	else
		reqtsuprilim = vcparmsp->vc_uprilim;

	if (vcparmsp->vc_upri == VC_NOCHANGE)
		reqtsupri = vcpp->vc_upri;
	else
		reqtsupri = vcparmsp->vc_upri;

	/*
	 * Make sure the user priority doesn't exceed the upri limit.
	 */
	if (reqtsupri > reqtsuprilim)
		reqtsupri = reqtsuprilim;

	/*
	 * Basic permissions enforced by generic kernel code
	 * for all classes require that a thread attempting
	 * to change the scheduling parameters of a target
	 * thread be super-user or have a real or effective
	 * UID matching that of the target thread. We are not
	 * called unless these basic permission checks have
	 * already passed. The VP/ix process class requires in
	 * addition that the calling thread be super-user if it
	 * is attempting to raise the upri limit above its current
	 * value This may have been checked previously but if our
	 * caller passed us a non-NULL credential pointer we assume
	 * it hasn't and we check it here.
	 */
	if (reqpcredp != NULL) {
		if (reqtsuprilim > vcpp->vc_uprilim && !suser(reqpcredp)) {
			return (EPERM);
		}
	}

	vcpp->vc_uprilim = reqtsuprilim;
	vcpp->vc_upri = reqtsupri;

	/*  Recalculate priority unless process is busywaiting.  If it is,
	 *  the new parameters may still have some influence when the
	 *  process stops busywaiting.
	 */
	if ((vcpp->vc_flags & VCBSYWT) == 0)
		VC_NEWUMDPRI(vcpp);

	/*
	 * Set vc_nice to the nice value corresponding to the user
	 * priority we are setting.
	 */
	nice = 20 - (vcparmsp->vc_upri * 20) / vc_maxupri;
	if (nice == 40)
		nice = 39;
	vcpp->vc_nice = nice;

	if ((vcpp->vc_flags & VCKPRI) != 0) {
		return (0);
	}

	tx = vcpp->vc_tp;
	thread_lock(tx);
	vcpp->vc_dispwait = 0;
	if (tx == curthread) {
		tx->t_pri = vc_dptbl[vcumdpri].vc_globpri;
		if (DISP_PRIO(tx) > DISP_MAXRUNPRI(tx))
			vcpp->vc_timeleft = vc_dptbl[vcumdpri].vc_quantum;
		else {
			vcpp->vc_flags |= VCBACKQ;
			cpu_surrender(curthread);
		}
	} else {
		wasonq = dispdeq(tx);
		tx->t_pri = vc_dptbl[vcumdpri].vc_globpri;
		if (wasonq == B_TRUE) {
			vcpp->vc_timeleft = vc_dptbl[vcumdpri].vc_quantum;
			setbackdq(tx);
		} else {
			vcpp->vc_flags |= VCBACKQ;
			if (tx->t_state == TS_ONPROC)
				cpu_surrender(tx);
		}
	}
	thread_unlock(tx);
	return (0);
}

/*
 * Return the global scheduling priority that would be assigned
 * to a thread entering the VP/ix process class with the vc_upri
 * value specified in the vcparms buffer.
 */
static pri_t
vc_globpri(vcproc_t *vcprocp)
{
	register pri_t	vcpri;

	vcpri = vcmedumdpri + vcprocp->vc_upri;
	if (vcpri > vc_maxumdpri)
		vcpri = vc_maxumdpri;
	else if (vcpri < 0)
		vcpri = 0;
	return (vc_dptbl[vcpri].vc_globpri);
}

/*
 * Arrange for thread to be placed in appropriate location
 * on dispatcher queue.
 *
 * This is called with the current thread in TS_ONPROC and locked.
 */
static void
vc_preempt(t)
	kthread_id_t	t;
{
	vcproc_t	*vcpp = (vcproc_t *)(t->t_cldata);
	register klwp_t	*lwp;
	extern int	kslice;

	ASSERT(t == curthread);
	ASSERT(THREAD_LOCK_HELD(curthread));

	/*
	 * If preempted in the kernel, make sure the thread has
	 * a kernel priority if needed.
	 */
	lwp = curthread->t_lwp;
	if (!(vcpp->vc_flags & VCKPRI) && lwp != NULL && t->t_kpri_req) {
		vcpp->vc_flags |= VCKPRI;
		t->t_pri = vc_kmdpris[0];
		t->t_trapret = 1;		/* so vc_trapret will run */
		aston(t);
	}

	/*
	 * If preempted in user-land mark the thread
	 * as swappable because I know it isn't holding any locks.
	 */
	ASSERT(t->t_schedflag & TS_DONT_SWAP);
	if (lwp != NULL && lwp->lwp_state == LWP_USER)
		t->t_schedflag &= ~TS_DONT_SWAP;

	if ((vcpp->vc_flags & (VCBACKQ|VCKPRI)) == VCBACKQ) {
		vcpp->vc_timeleft = vc_dptbl[vcumdpri].vc_quantum;
		vcpp->vc_dispwait = 0;
		vcpp->vc_flags &= ~VCBACKQ;
		setbackdq(t);
	} else if ((vcpp->vc_flags & (VCBACKQ|VCKPRI)) == (VCBACKQ|VCKPRI)) {
		vcpp->vc_flags &= ~VCBACKQ;
		setbackdq(t);
	} else {
		if (kslice)
			setbackdq(t);
		else
			setfrontdq(t);
	}
}

static void
vc_setrun(t)
	kthread_id_t	t;
{
	vcproc_t *vcpp = (vcproc_t *)(t->t_cldata);
	unsigned char flags;

	ASSERT(THREAD_LOCK_HELD(t));	/* t should be in transition */

	flags = vcpp->vc_flags &= ~VCBACKQ;
	if ((flags & VCKPRI) == 0) {
		if (t->t_disp_time != lbolt) {
			vcpp->vc_timeleft = vc_dptbl[vcumdpri].vc_quantum;
			vcpp->vc_dispwait = 0;
			setbackdq(t);
		} else {
			setfrontdq(t);
		}
	} else {
		if ((flags & VCSLEPT) &&
		    vcpp->vc_dispwait > vc_dptbl[vcumdpri].vc_maxwait) {
			vcpp->vc_cpupri = vc_dptbl[vcpp->vc_cpupri].vc_slpret;
			VC_NEWUMDPRI(vcpp);
			vcpp->vc_timeleft = vc_dptbl[vcumdpri].vc_quantum;
			vcpp->vc_dispwait = 0;
		}
		setbackdq(t);
	}
}


/*
 * Prepare thread for sleep. We reset the thread priority so it will
 * run at the requested priority (as specified by the disp argument)
 * when it wakes up.
 */
/* ARGSUSED */
static void
vc_sleep(t, disp)
	kthread_id_t		t;
	short			disp;
{
	vcproc_t	*vcpp = (vcproc_t *)(t->t_cldata);
	int		flags;

	ASSERT(t == curthread);
	ASSERT(THREAD_LOCK_HELD(t));

	flags = vcpp->vc_flags;
	if (t->t_kpri_req) {
		vcpp->vc_flags = flags | VCKPRI | VCSLEPT;
		if (disp > vc_maxkmdpri)
			disp = vc_maxkmdpri;
		t->t_pri = vc_kmdpris[disp];
		t->t_trapret = 1;		/* so vc_trapret will run */
		aston(t);
	} else if (flags & VCKPRI) {
		/*
		 * Same as the inside of vc_trapret() (after getting lock).
		 * If thread has blocked in the kernel (as opposed to
		 * being merely preempted), recompute the user mode priority.
		 */
		if ((flags & VCSLEPT) &&
		    vcpp->vc_dispwait > vc_dptbl[vcumdpri].vc_maxwait) {
			vcpp->vc_cpupri = vc_dptbl[vcpp->vc_cpupri].vc_slpret;
			VC_NEWUMDPRI(vcpp);
			vcpp->vc_timeleft = vc_dptbl[vcumdpri].vc_quantum;
			vcpp->vc_dispwait = 0;
		}

		curthread->t_pri = vc_dptbl[vcumdpri].vc_globpri;
		vcpp->vc_flags = flags & ~(VCKPRI | VCSLEPT);

		if (DISP_PRIO(curthread) < DISP_MAXRUNPRI(curthread)) {
			cpu_surrender(curthread);
		}
	}
	t->t_stime = lbolt;		/* time stamp for the swapper */
}

/*
 * Return Values:
 *
 *	-1 if the thread is loaded or is not eligible to be swapped in.
 *
 *	effective priority of the specified thread based on swapout time
 *		and size of process (epri >= 0 , epri <= SHRT_MAX).
 */

/* ARGSUSED */
static pri_t
vc_swapin(t, flags)
	kthread_id_t	t;
	int		flags;
{
	vcproc_t	*vcpp = (vcproc_t *)(t->t_cldata);
	long		epri = -1;
	proc_t		*pp = ttoproc(t);

	ASSERT(THREAD_LOCK_HELD(t));

	/*
	 * We know that pri_t is a short.
	 * Be sure not to overrun its range.
	 */
	if (t->t_state == TS_RUN && (t->t_schedflag & TS_LOAD) == 0) {
		time_t swapout_time;

		swapout_time = (lbolt - t->t_stime) / hz;
		if (INHERITED(t) || (vcpp->vc_flags & VCKPRI))
			epri = (long)DISP_PRIO(t) + swapout_time;
		else {
			/*
			 * Threads which have been out for a long time,
			 * have high user mode priority and are associated
			 * with a small address space are more deserving
			 */
			epri = vc_dptbl[vcumdpri].vc_globpri;
			ASSERT(epri <= vc_maxumdpri);
			epri += swapout_time - pp->p_swrss / nz(maxpgio)/2;
		}
		/*
		 * Scale epri so SHRT_MAX/2 represents zero priority.
		 */
		epri += SHRT_MAX/2 ;
		if (epri < 0)
			epri = 0;
		else if (epri > SHRT_MAX)
			epri = SHRT_MAX;
	}
	return ((pri_t)epri);
}

/*
 * Return Values
 *	-1 if the thread isn't loaded or is not eligible to be swapped out.
 *
 *	effective priority of the specified thread based on if the swapper
 *		is in softswap or hardswap mode.
 *
 *		Softswap:  Return a low effective priority for threads
 *			   sleeping for more than maxslp secs.
 *
 *		Hardswap:  Return an effective priority such that threads
 *			   which have been in memory for a while and are
 *			   associated with a small address space are swapped
 *			   in before others.
 *
 *		(epri >= 0 , epri <= SHRT_MAX).
 */

/*
 * XXX - Need to determine what these values should be.
 */
time_t	vc_minrun = 5;		/* min time on run queue for hardswap */
time_t	vc_minslp = 2;		/* min time on sleep queue for hardswap */

/* ARGSUSED */
static pri_t
vc_swapout(t, flags)
	kthread_id_t	t;
	int		flags;
{
	vcproc_t	*vcpp = (vcproc_t *)(t->t_cldata);
	long		epri = -1;
	proc_t		*pp = ttoproc(t);
	time_t		swapin_time;

	ASSERT(THREAD_LOCK_HELD(t));

	if (INHERITED(t) || (vcpp->vc_flags & VCKPRI) ||
	    (t->t_proc_flag & TP_LWPEXIT) ||
	    (t->t_state & (TS_ZOMB | TS_FREE | TS_STOPPED | TS_ONPROC)) ||
	    !(t->t_schedflag & TS_LOAD) || !SWAP_OK(t))
		return (-1);

	ASSERT(t->t_state & (TS_SLEEP | TS_RUN));

	/*
	 * We know that pri_t is a short.
	 * Be sure not to overrun its range.
	 */
	swapin_time = (lbolt - t->t_stime) / hz;
	if (flags == SOFTSWAP) {
		if (t->t_state == TS_SLEEP && swapin_time > maxslp)
			epri = 0;
	} else {
		pri_t pri;

		if (((t->t_state == TS_SLEEP && swapin_time > vc_minslp) ||
		    (t->t_state == TS_RUN && swapin_time > vc_minrun))) {
			pri = vc_dptbl[vcumdpri].vc_globpri;
			ASSERT(pri <= vc_maxumdpri);
			epri = swapin_time -
			    (rm_asrss(pp->p_as) / nz(maxpgio)/2) - (long)pri;
		}
	}

	/*
	 * Scale epri so SHRT_MAX/2 represents zero priority.
	 */
	epri += SHRT_MAX/2 ;
	if (epri < 0)
		epri = 0;
	else if (epri > SHRT_MAX)
		epri = SHRT_MAX;

	return ((pri_t)epri);
}

/*
 * Check for time slice expiration.  If time slice has expired
 * move thread to priority specified in vcdptbl for time slice expiration
 * and set runrun to cause preemption.
 */

static void
vc_tick(t)
	kthread_id_t	t;
{
	vcproc_t *vcpp = (vcproc_t *)(t->t_cldata);
	boolean_t	wasonq;
	klwp_t *lwp;

	if ((vcpp->vc_flags & VCKPRI) != 0)
		/*
		 * No time slicing of procs at kernel mode priorities.
		 */
		return;

	ASSERT(MUTEX_HELD(&pidlock));
	if (--vcpp->vc_timeleft <= 0) {
		thread_lock(t);
		wasonq = dispdeq(t);

		/*  If process is busywaiting, new user mode priority
		 *  is taken directly from the table.  Otherwise it
		 *  is computed as for time-sharing.
		 */
		if (vcpp->vc_flags & VCBSYWT) {
			vcpp->vc_umdpri = vc_dptbl[vcpp->vc_umdpri].vc_tqexp;
		}
		else {
			vcpp->vc_cpupri = vc_dptbl[vcpp->vc_cpupri].vc_tqexp;
			VC_NEWUMDPRI(vcpp);
		}
		t->t_pri = vc_dptbl[vcumdpri].vc_globpri;
		vcpp->vc_dispwait = 0;
		if (wasonq == B_TRUE) {
			if ((t->t_schedflag & TS_LOAD) && (lwp = t->t_lwp) &&
			    lwp->lwp_state == LWP_USER)
				t->t_schedflag &= ~TS_DONT_SWAP;
			vcpp->vc_timeleft = vc_dptbl[vcumdpri].vc_quantum;
			setbackdq(t);
		} else {
			vcpp->vc_flags |= VCBACKQ;
			cpu_surrender(t);
		}
		thread_unlock(t);
	}
}


/*
 * If thread is currently at a kernel mode priority (has slept or was preempted)
 * we assign it the appropriate user mode priority and time quantum
 * here.  If we are lowering the thread' priority below that of
 * other runnable threads we will normally set runrun here to
 * cause preemption.
 */
static void
vc_trapret()
{
	int		flags;
	vcproc_t	*vcpp = (vcproc_t *)curthread->t_cldata;
	boolean_t	was_boosted;

	ASSERT(THREAD_LOCK_HELD(curthread));

	/*
	 *  Note whether process priority had been boosted, then
	 *  turn off the flag.
	 */
	was_boosted = (vcpp->vc_flags & VCBOOST) ? B_TRUE : B_FALSE;
	if (was_boosted) {
		vcpp->vc_flags &= ~VCBOOST;
	}

	if ((flags = vcpp->vc_flags) & VCKPRI) {
		/*
		 * If thread has blocked in the kernel (as opposed to
		 * being merely preempted), recompute the user mode priority.
		 */
		if (flags & VCSLEPT) {
			vcpp->vc_cpupri = vc_dptbl[vcpp->vc_cpupri].vc_slpret;
 
			/*
	 		 * If priority was not boosted and is not busywaiting,
	 		 * calculate a suitable new priority. If it was boosted,
	 		 * vc_boost() already did so. If it is busywaiting, it
	 		 * will receive a new priority when it receives or 
			 * misses its timeslice or when it stops busywaiting.
	 		 */
			if (was_boosted == B_FALSE && 
				(vcpp->vc_flags & VCBSYWT) == 0 ) {
				VC_NEWUMDPRI(vcpp);
			}
/*
 * XXX - this happens too frequently, allowing users with heavy system
 * XXX - call activity to run unpreempted for a long time.

			vcpp->vc_timeleft = vc_dptbl[vcumdpri].vc_quantum;
			vcpp->vc_dispwait = 0;
 */
		}

		curthread->t_pri = vc_dptbl[vcumdpri].vc_globpri;
		vcpp->vc_flags &= ~(VCKPRI | VCSLEPT);

		if (DISP_PRIO(curthread) < DISP_MAXRUNPRI(curthread)) {
			if (was_boosted == B_FALSE)
				cpu_surrender(curthread);
		}
	}

	/*
	 * Swapout lwp if the swapper is waiting for this thread to
	 * reach a safe point.
	 */
	if (curthread->t_schedflag & TS_SWAPENQ) {
		thread_unlock(curthread);
		swapout_lwp(ttolwp(curthread));
		thread_lock(curthread);
	}
}


/*
 * Update the vc_dispwait values of all vpix class threads that
 * are currently runnable at a user mode priority and bump the priority
 * if vc_dispwait exceeds vc_maxwait.  Called once per second via
 * timeout which we reset here.
 *
 * In the time-sharing class, the equivalent routine is called
 * every second whereas the time limits for processes to receive
 * their quanta are all 5 seconds.  Processes exceeding their
 * time limits will tend to do so at different invocations of the
 * update routine.
 *
 * In the VP/ix class, the time limits are all 1 second and the
 * update routine runs once per second.  Processes exceeding their
 * time limits will tend to do so in lock step.  The intent is that
 * they will update on-screen clocks at much the same time.  If this
 * strategy turns out to be inappropriate, the correct answer is
 * probably to express the time limits in clock ticks and run vc_update
 * more frequently, perhaps with the frequency depending on the number
 * of VP/ix processes (perhaps vpixprocs >= 10 ? hz/10 : hz / vpixprocs).
 *
 */
static void
vc_update()
{
	register vcproc_t	*vcpp;
	register int		oldlvl;
	boolean_t		wasonq;
	kthread_id_t		tx;

	mutex_enter(&vc_list_lock);
	for (vcpp = vc_plisthead.vc_next; vcpp != &vc_plisthead;
	    vcpp = vcpp->vc_next) {
		tx = vcpp->vc_tp;
		if ((vcpp->vc_flags & VCKPRI) != 0 ||
		    !(tx->t_state & (TS_RUN | TS_ONPROC)))
			continue;
		thread_lock(tx);
		if (tx->t_clfuncs != &vc_classfuncs.thread)
			goto next;
		if ((vcpp->vc_flags & VCKPRI) != 0 || tx->t_state != TS_RUN)
			goto next;
		vcpp->vc_dispwait++;
		if (vcpp->vc_dispwait <= vc_dptbl[vcumdpri].vc_maxwait)
			goto next;

		/*  If process is busywaiting, get its new priority directly
		 *  from the table.  Otherwise set cpupri from the table
		 *  and recalculate.
		 */

		if (vcpp->vc_flags & VCBSYWT) {
			vcpp->vc_umdpri = vc_dptbl[vcpp->vc_umdpri].vc_lwait;
		}
		else {
			vcpp->vc_cpupri = vc_dptbl[vcpp->vc_cpupri].vc_lwait;
			VC_NEWUMDPRI(vcpp);
		}

		vcpp->vc_dispwait = 0;
		wasonq = dispdeq(tx);
		tx->t_pri = vc_dptbl[vcumdpri].vc_globpri;
		if (wasonq == B_TRUE) {
			vcpp->vc_timeleft = vc_dptbl[vcumdpri].vc_quantum;
			setbackdq(tx);
		} else {
			vcpp->vc_flags |= VCBACKQ;
			cpu_surrender(tx);
		}
next:
		thread_unlock(tx);
	}
	mutex_exit(&vc_list_lock);
	(void) timeout(vc_update, 0, hz);
}


/*
 * Processes waking up go to the back of their queue.  We don't
 * need to assign a time quantum here because thread is still
 * at a kernel mode priority and the time slicing is not done
 * for threads running in the kernel after sleeping.  The proper
 * time quantum will be assigned by vc_trapret before the thread
 * returns to user mode.
 */
/* ARGSUSED */
static void
vc_wakeup(t)
	kthread_id_t	t;
{
	vcproc_t	*vcprocp = (vcproc_t *)(t->t_cldata);

	ASSERT(THREAD_LOCK_HELD(t));

	t->t_stime = lbolt;		/* time stamp for the swapper */

	vcprocp->vc_flags &= ~VCBACKQ;
	setbackdq(t);
}

/*
 * This routine is called from v86setint() to boost the priority of a
 * VP/ix process in response to a pseudorupt.
 *
 */
void
vc_boost(t, urgent)
	kthread_id_t	t;
	boolean_t	urgent;
{
	register vcproc_t	*vcpp;
	register boolean_t	wasonq;
	short			newpri;
	int			newglobpri;

	/* Do nothing if not a VP/ix process */
	if (t->t_cid != vc_cid)
		return;

	ASSERT(THREAD_LOCK_HELD(t));

	/* Find the class-specific process structure */
	vcpp = t->t_cldata;

	/* Remember that we boosted the process and
	 * turn off any busywait indication
	 */
	vcpp->vc_flags |= VCBOOST;
	vcpp->vc_flags &= ~VCBSYWT;
	t->t_trapret = 1;		/* so vc_trapret will run */
	aston(t);

	/* Calculate the priority we are planning to give the process.
	 * For urgent events use the maximum class priority.  For others
	 * use the next highest.
	 */
	newpri = (urgent ? vc_maxumdpri : vc_maxumdpri - 1);

	/*  If process is at kernel priority it already has higher
	 *  priority than newpri.  Set the user mode priority for
	 *  use when returning to user mode.
	 */
	if (vcpp->vc_flags & VCKPRI) {
		vcpp->vc_umdpri = newpri;
		return;
	}

	/* Return immediately if priority would not change.  Otherwise
	 * process would move to the back of the queue it is already on.
	 */
	if (newpri == vcpp->vc_umdpri)
		return;

	/* Assign the new priority. */
	vcpp->vc_umdpri = newpri;
	/*
	 * Remove process from dispatcher queue if it is on one.
	 * Remember whether it was.
	 */
	wasonq = dispdeq(t);
	t->t_pri = vc_dptbl[newpri].vc_globpri;
	if (wasonq == B_TRUE)
		setbackdq(t);
	/*
	 * If current thread is lower priority than the newly
	 * boosted one, request a process switch.
	 *
	 */
	if (curthread->t_pri < t->t_pri)
		cpu_surrender(curthread);
}

void
vc_busywait(t, level)
	kthread_id_t	t;
	int		level;
{
	register vcproc_t	*vcpp;
	register boolean_t	wasonq;
	short			newpri;

	/* Do nothing if not a VP/ix process */
	if (t->t_cid != vc_cid)
		return;

	ASSERT(THREAD_LOCK_HELD(t));

	/* Find the class-specific process structure */
	vcpp = t->t_cldata;

	if (level) {
		/*  ECT has indicated that process is busywaiting.
		 *  Turn on busywait flag.  If we are already at the
		 *  appropriate busywait level, do nothing.  Make sure
		 *  busywaiting level is within assumed range.
		 */
		if (level > 3) {
			level = 3;
		}
		vcpp->vc_flags |= VCBSYWT;
		if (vcpp->vc_umdpri % 10 == level - 1)
			return;
		newpri = level - 1;
	}
	else {
		if ((vcpp->vc_flags & VCBSYWT) == 0)
			return;
		vcpp->vc_flags &= ~VCBSYWT;
	}

	/* Remove process from dispatcher queue if it is on one.
	 * Remember whether it was.
	 */
	wasonq = dispdeq(t);

	/* Assign the new priority.
	 */
	if (vcpp->vc_flags & VCBSYWT)
		vcpp->vc_umdpri = newpri;
	else {
		VC_NEWUMDPRI(vcpp);
		newpri = vcpp->vc_umdpri;
	}

	if (t == curthread)
		t->t_pri = vc_dptbl[vcumdpri].vc_globpri;

	/* Put process back on a queue if we got it off one */
	if (wasonq)
		setbackdq(t);
}

/*
 * Parameter which determines how recently a thread must have run
 * on the CPU to be considered loosely-bound to that CPU to reduce
 * cold cache effects.
 */
static clock_t	vc_rechoose_interval = 3; /* XXX interval in hertz */

/*
 * Select a CPU for this thread to run on.
 */
static cpu_t *
vc_cpu_choose(kthread_id_t t, pri_t tpri)
{
	cpu_t   *bestcpu;

	/*
	 * Only search for best CPU if the thread is at a high priority
	 * (from inheritance) or hasn't run for awhile.
	 */
	bestcpu = t->t_cpu;		/* start with last CPU used */
	if (tpri >= kpreemptpri ||
	    ((lbolt - t->t_disp_time) > vc_rechoose_interval && t != curthread))
		bestcpu = disp_lowpri_cpu(bestcpu->cpu_next);
	return (bestcpu);
}

static void
vc_set_process_group(pid_t sid, pid_t bg_pgid, pid_t fg_pgid)
{

}
#endif /* _VPIX */
