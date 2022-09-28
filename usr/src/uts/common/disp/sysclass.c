/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sysclass.c	1.41	96/06/18 SMI"	/* from SVr4.0 1.12 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/pcb.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/sysinfo.h>
#include <sys/var.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/proc.h>
#include <sys/debug.h>
#include <sys/inline.h>
#include <sys/disp.h>
#include <sys/class.h>
#include <sys/kmem.h>
#include <sys/cpuvar.h>
#include <sys/priocntl.h>

extern pri_t	maxclsyspri;
/*
 * Class specific code for the sys class. There are no
 * class specific data structures associated with
 * the sys class and the scheduling policy is trivially
 * simple. There is no time slicing.
 */

void		sys_init(id_t, int, classfuncs_t **, pri_t *, size_t *);
static int	sys_getclpri(pcpri_t *);
static int	sys_fork(kthread_id_t, kthread_id_t, void *);
static int	sys_enterclass(kthread_id_t, id_t, void *, cred_t *, void *);
static int	sys_nosys();
static int	sys_donice(kthread_id_t, cred_t *, int, int *);
static void	sys_forkret(kthread_id_t, kthread_id_t);
static void	sys_nullsys();
static pri_t	sys_swappri(kthread_id_t, int);

struct classfuncs sys_classfuncs = {
	/* messages to class manager */
	{
		sys_nosys,	/* admin */
		sys_nosys,	/* getclinfo */
		sys_nosys,	/* parmsin */
		sys_nosys,	/* parmsout */
		sys_getclpri,	/* getclpri */
	},
	/* operations on threads */
	{
		sys_enterclass,	/* enterclass */
		sys_nullsys,	/* exitclass */
		sys_fork,
		sys_forkret,	/* forkret */
		sys_nullsys,	/* parmsget */
		sys_nosys,	/* parmsset */
		sys_nullsys,	/* stop */
		sys_swappri,	/* swapin */
		sys_swappri,	/* swapout */
		sys_nullsys,	/* trapret */
#ifdef KSLICE
		sys_preempt,
#else
		setfrontdq,
#endif
		setbackdq,	/* setrun */
		sys_nullsys,	/* sleep */
		sys_nullsys,	/* tick */
		setbackdq,	/* wakeup */
		sys_donice,
		(pri_t (*)())sys_nosys,	/* globpri */
		sys_nullsys,	/* set_process_group */
		sys_nullsys,	/* yield */
	}

};


/* ARGSUSED */
void
sys_init(cid, clparmsz, clfuncspp, maxglobprip, clprocsz)
	id_t		cid;
	int		clparmsz;
	classfuncs_t	**clfuncspp;
	pri_t		*maxglobprip;
	size_t		*clprocsz;
{
	*clfuncspp = &sys_classfuncs;

	if (v.v_maxsyspri < PSLEP) {
		cmn_err(CE_WARN,
		    "Max system class priority must be >= %d, "
		    "configured value is %d\n- resetting v.v_maxsyspri to %d\n",
		    PSLEP, v.v_maxsyspri, PSLEP);
		v.v_maxsyspri = PSLEP;
	}
	*maxglobprip = (pri_t)v.v_maxsyspri;
	*clprocsz = 0;
}

/*
 * Get maximum and minimum priorities enjoyed by sysclass threads
 */
static int
sys_getclpri(pcpri_t *pcprip)
{
	pcprip->pc_clpmax = maxclsyspri;
	pcprip->pc_clpmin = 0;
	return (0);
}

/* ARGSUSED */
static int
sys_enterclass(t, cid, parmsp, reqpcredp, bufp)
	kthread_id_t	t;
	id_t		cid;
	void		*parmsp;
	cred_t		*reqpcredp;
	void		*bufp;
{
	return (0);
}

/* ARGSUSED */
static int
sys_fork(t, ct, bufp)
	kthread_id_t t;
	kthread_id_t ct;
	void	*bufp;
{
	/*
	 * No class specific data structure
	 */
	return (0);
}


/* ARGSUSED */
static void
sys_forkret(t, ct)
	kthread_id_t t;
	kthread_id_t ct;
{
	register proc_t *pp = ttoproc(t);
	register proc_t *cp = ttoproc(ct);

	ASSERT(t == curthread);
	ASSERT(MUTEX_HELD(&pidlock));

	/*
	 * Grab the child's p_lock before dropping pidlock to ensure
	 * the process does not disappear before we set it running.
	 */
	mutex_enter(&cp->p_lock);
	mutex_exit(&pidlock);
	continuelwps(cp);
	mutex_exit(&cp->p_lock);

	mutex_enter(&pp->p_lock);
	continuelwps(pp);
	mutex_exit(&pp->p_lock);
}

/* ARGSUSED */
static pri_t
sys_swappri(t, flags)
	kthread_id_t	t;
	int		flags;
{
	return (-1);
}

static int
sys_nosys()
{
	return (ENOSYS);
}


static void
sys_nullsys()
{
}

#ifdef KSLICE
static void
sys_preempt(t)
	kthread_id_t	t;
{
	extern int	kslice;

	if (kslice)
		setbackdq(t);
	else
		setfrontdq(t);
}
#endif


/* ARGSUSED */
static int
sys_donice(t, cr, incr, retvalp)
	kthread_id_t	t;
	cred_t		*cr;
	int		incr;
	int		*retvalp;
{
	return (EINVAL);
}
