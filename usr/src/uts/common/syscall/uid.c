/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	Copyright (c) 1994 Sun Microsystems, Inc. */
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)uid.c	1.9	96/05/30 SMI"	/* from SVr4.0 1.78 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/tuneable.h>
#include <sys/cred.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/debug.h>

int
setuid(uid_t uid)
{
	register proc_t *p;
	int error = 0;
	cred_t	*cr, *newcr;
	uid_t oldruid = uid;

	if (uid < 0 || uid > MAXUID)
		return (set_errno(EINVAL));

	/*
	 * Need to pre-allocate the new cred structure before grabbing
	 * the p_crlock mutex.
	 */
	newcr = crget();
	p = ttoproc(curthread);
	mutex_enter(&p->p_crlock);
	cr = p->p_cred;
	if (cr->cr_uid && (uid == cr->cr_ruid || uid == cr->cr_suid)) {
		crcopy_to(cr, newcr);
		p->p_cred = newcr;
		newcr->cr_uid = uid;
	} else if (suser(cr)) {
		oldruid = cr->cr_ruid;
		crcopy_to(cr, newcr);
		p->p_cred = newcr;
		newcr->cr_ruid = uid;
		newcr->cr_suid = uid;
		newcr->cr_uid = uid;
	} else {
		error = EPERM;
		crfree(newcr);
	}
	mutex_exit(&p->p_crlock);

	if (oldruid != uid) {
		mutex_enter(&pidlock);
		upcount_dec(oldruid);
		upcount_inc(uid);
		mutex_exit(&pidlock);
	}

	if (error == 0) {
		crset(p, newcr);	/* broadcast to process threads */
		return (0);
	}
	return (set_errno(error));
}

longlong_t
getuid(void)
{
	rval_t	r;
	cred_t *cr;

	cr = curthread->t_cred;
	r.r_val1 = cr->cr_ruid;
	r.r_val2 = cr->cr_uid;
	return (r.r_vals);
}

int
seteuid(uid_t uid)
{
	register proc_t *p;
	int error = 0;
	cred_t	*cr, *newcr;

	if (uid < 0 || uid > MAXUID)
		return (set_errno(EINVAL));

	/*
	 * Need to pre-allocate the new cred structure before grabbing
	 * the p_crlock mutex.
	 */
	newcr = crget();
	p = ttoproc(curthread);
	mutex_enter(&p->p_crlock);
	cr = p->p_cred;
	if (uid == cr->cr_ruid || uid == cr->cr_uid || uid == cr->cr_suid ||
	    suser(cr)) {
		crcopy_to(cr, newcr);
		p->p_cred = newcr;
		newcr->cr_uid = uid;
	} else {
		error = EPERM;
		crfree(newcr);
	}
	mutex_exit(&p->p_crlock);

	if (error == 0) {
		crset(p, newcr);	/* broadcast to process threads */
		return (0);
	}
	return (set_errno(error));
}

/*
 * Buy-back from SunOS 4.x
 *
 * Like setuid() and seteuid() combined -except- that non-root users
 * can change cr_ruid to cr_uid, and the semantics of cr_suid are
 * subtly different.
 */
int
setreuid(uid_t ruid, uid_t euid)
{
	proc_t *p;
	int error = 0;
	uid_t oldruid = ruid;
	cred_t *cr, *newcr;

	if ((ruid != -1 && (ruid < 0 || ruid > MAXUID)) ||
	    (euid != -1 && (euid < 0 || euid > MAXUID)))
		return (set_errno(EINVAL));

	/*
	 * Need to pre-allocate the new cred structure before grabbing
	 * the p_crlock mutex.
	 */
	newcr = crget();

	p = ttoproc(curthread);
	mutex_enter(&p->p_crlock);
	cr = p->p_cred;

	if (ruid != -1 &&
	    ruid != cr->cr_ruid && ruid != cr->cr_uid && !suser(cr)) {
		error = EPERM;
	} else if (euid != -1 &&
	    euid != cr->cr_ruid && euid != cr->cr_uid &&
	    euid != cr->cr_suid && !suser(cr)) {
		error = EPERM;
	} else {
		crcopy_to(cr, newcr);
		p->p_cred = newcr;

		if (euid != -1)
			newcr->cr_uid = euid;
		if (ruid != -1) {
			oldruid = newcr->cr_ruid;
			newcr->cr_ruid = ruid;
		}
		/*
		 * "If the real uid is being changed, or the effective uid is
		 * being changed to a value not equal to the real uid, the
		 * saved uid is set to the new effective uid."
		 */
		if (ruid != -1 ||
		    (euid != -1 && newcr->cr_uid != newcr->cr_ruid))
			newcr->cr_suid = newcr->cr_uid;
	}
	mutex_exit(&p->p_crlock);

	if (oldruid != ruid) {
		ASSERT(oldruid != -1 && ruid != -1);
		mutex_enter(&pidlock);
		upcount_dec(oldruid);
		upcount_inc(ruid);
		mutex_exit(&pidlock);
	}

	if (error == 0) {
		crset(p, newcr);	/* broadcast to process threads */
		return (0);
	}
	crfree(newcr);
	return (set_errno(error));
}
