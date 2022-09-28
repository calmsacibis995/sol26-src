/*	Copyright (c) 1994, by Syn Microsystems, Inc */
/*	  All Rights Reserved	*/

#ident	"@(#)signotify.c	1.2	96/05/15 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/procset.h>
#include <sys/fault.h>
#include <sys/signal.h>
#include <sys/siginfo.h>
#include <sys/debug.h>

extern int	get_lwpchan(struct as *, caddr_t, int type, quad *lwpchan);

struct signotifya {
	int		cmd;
	siginfo_t	*siginfo;
	signotify_id_t	*sn_id;
};

static u_longlong_t get_sigid(proc_t *p, caddr_t addr);

#define	SIGN_PTR(p, n)	&((signotifyq_t *)(&p->p_signhdr[1]))[n];

/* ARGSUSED */
int
signotify(uap, rvp)
	register struct signotifya *uap;
	rval_t	*rvp;
{
	k_siginfo_t		info;
	signotify_id_t		id;
	register proc_t		*p;
	register proc_t		*cp = curproc;
	register signotifyq_t	*snqp;
	register struct cred	*cr;
	register sigqueue_t	*sqp;
	sigqhdr_t		*sqh;
	u_longlong_t		sid;


	if (copyin((caddr_t)uap->sn_id, (caddr_t)&id, sizeof (signotify_id_t)))
		return (EFAULT);

	if (id.sn_index >= _SIGNOTIFY_MAX || id.sn_index < 0)
		return (EINVAL);

	switch (uap->cmd) {

	case SN_PROC:

		/* get snid for the given user address of signotifyid_t */
		sid = get_sigid(cp, (caddr_t)uap->sn_id);

		if (id.sn_pid > 0) {
			mutex_enter(&pidlock);
			if ((p = prfind(id.sn_pid)) != NULL) {
				mutex_enter(&p->p_lock);
				if (p->p_signhdr != NULL) {
					snqp = SIGN_PTR(p, id.sn_index);
					if (snqp->sn_snid == sid) {
						mutex_exit(&p->p_lock);
						mutex_exit(&pidlock);
						return (EBUSY);
					}
				}
				mutex_exit(&p->p_lock);
			}
			mutex_exit(&pidlock);
		}

		if (copyin((caddr_t)uap->siginfo,
			(caddr_t)&info, sizeof (k_siginfo_t))) {
			return (EFAULT);
		}


		if (cp->p_signhdr == NULL) {
			if (sigqhdralloc(&sqh, sizeof (signotifyq_t),
						_SIGNOTIFY_MAX) < 0) {
				return (EAGAIN);
			} else {

				mutex_enter(&cp->p_lock);
				if (cp->p_signhdr == NULL) {
					cp->p_signhdr = sqh;
				} else {
					sigqhdrfree(&sqh);
				}
			}
		} else {
			mutex_enter(&cp->p_lock);
		}

		sqp = sigqalloc(&cp->p_signhdr);

		if (sqp == NULL) {
			mutex_exit(&cp->p_lock);
			return (EAGAIN);
		}

		cr = CRED();
		sqp->sq_info = info;
		sqp->sq_info.si_pid = cp->p_pid;
		sqp->sq_info.si_uid = cr->cr_ruid;
		sqp->sq_func = sigqrel;
		sqp->sq_next = NULL;

		/* fill the signotifyq_t fields */
		((signotifyq_t *)sqp)->sn_snid = sid;

		mutex_exit(&cp->p_lock);

		/* complete the signotify_id_t fields */
		id.sn_index = (signotifyq_t *)sqp - SIGN_PTR(cp, 0);
		id.sn_pid = cp->p_pid;

		break;

	case SN_CANCEL:
	case SN_SEND:

		mutex_enter(&pidlock);
		if ((id.sn_pid <= 0) || ((p = prfind(id.sn_pid)) == NULL)) {
			mutex_exit(&pidlock);
			return (EINVAL);
		}
		mutex_enter(&p->p_lock);
		mutex_exit(&pidlock);

		if (p->p_signhdr == NULL) {
			mutex_exit(&p->p_lock);
			return (EINVAL);
		}

		snqp = SIGN_PTR(p, id.sn_index);

		if (snqp->sn_snid == 0) {
			mutex_exit(&p->p_lock);
			return (EINVAL);
		}

		if (snqp->sn_snid != get_sigid(cp, (caddr_t)uap->sn_id)) {
			mutex_exit(&p->p_lock);
			return (EINVAL);
		}

		snqp->sn_snid = 0;

		if (uap->cmd == SN_SEND) {
			sigaddqa(p, 0, (sigqueue_t *)snqp);
		} else {
			/* uap->cmd == SN_CANCEL */
			sigqrel((sigqueue_t *)snqp);
		}
		mutex_exit(&p->p_lock);

		id.sn_pid = 0;
		id.sn_index = 0;

		break;

	default :
		return (EINVAL);
	}

	if (copyout((caddr_t)&id, (caddr_t)uap->sn_id, sizeof (signotify_id_t)))
		return (EFAULT);

	return (0);
}

/*
 * To find secured 64 bit id for signotify() call
 * This depends upon get_lwpchan() which returns
 * unique vnode/offset for a user virtual address.
 */
static u_longlong_t
get_sigid(proc_t *p, caddr_t addr)
{
	u_longlong_t snid;

	if (!get_lwpchan(p->p_as, addr, 1, (quad *)&snid)) {
		snid = 0;
	}
	return (snid);
}
