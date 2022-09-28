/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1996 Sun Microsystems, Inc.
 * All rights reserved.
 */


#ident	"@(#)rlimit.c	1.13	96/07/01 SMI"	/* from SVr4.0 1.78 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/tuneable.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/ulimit.h>
#include <sys/debug.h>
#include <vm/as.h>


/*
 * ulimit could be moved into a user library, as calls to getrlimit and
 * setrlimit, were it not for binary compatibility restrictions
 */

long
ulimit(int cmd, long arg)
{
	proc_t *p = ttoproc(curthread);
	user_t *up = PTOU(p);
	long	retval;
	rlim64_t filesize;

	switch (cmd) {

	case UL_GFILLIM: /* Return current file size limit. */
	{
		/*
		 * Large Files: File size is returned in blocks for ulimit.
		 * This function is deprecated and therefore LFS API
		 * didn't define the behaviour of ulimit.
		 * Here we return maximum value of file size possible
		 * so that applications that does not check error
		 * work properly.
		 */

		filesize = U_CURLIMIT(up, RLIMIT_FSIZE);
		if (filesize > MAXOFF_T)
			filesize = MAXOFF_T;
		retval = ((int)filesize >> SCTRSHFT);
		break;
	}
	case UL_SFILLIM: /* Set new file size limit. */
	{
		register int error = 0;
		register rlim64_t lim;

		lim = (rlim64_t)arg << SCTRSHFT;

		if (error = rlimit(RLIMIT_FSIZE, lim, lim)) {
			return (set_errno(error));
		}
		retval = arg;
		break;
	}

	case UL_GMEMLIM: /* Return maximum possible break value. */
	{
		register struct seg *seg;
		register struct seg *nextseg;
		register struct as *as = p->p_as;
		register caddr_t brkend;
		register caddr_t brkbase;
		uint size;

		brkend = (caddr_t)((int)
			(p->p_brkbase + p->p_brksize + PAGEOFFSET) & PAGEMASK);

		/*
		 * Find the segment with a virtual address
		 * greater than the end of the current break.
		 */
		nextseg = NULL;
		mutex_enter(&p->p_lock);
		brkbase = (caddr_t)p->p_brkbase;
		brkend = (caddr_t)p->p_brkbase + p->p_brksize;
		mutex_exit(&p->p_lock);

		AS_LOCK_ENTER(as, &as->a_lock, RW_READER);
		for (seg = as_findseg(as, brkend, 0); seg != NULL;
		    seg = AS_SEGP(as, seg->s_next)) {
			if (seg->s_base >= brkend) {
				nextseg = seg;
				break;
			}
		}

		/*
		 * We reduce the max break value (base+rlimit[DATA])
		 * if we run into another segment, the ublock or
		 * the end of memory.  We also are limited by
		 * rlimit[VMEM].
		 */
		retval = (long)
			(brkbase + (rlim_t)U_CURLIMIT(up, RLIMIT_DATA));
		/*
		 * Since we are casting the above RLIMIT_DATA value
		 * to an rlim_t (32 bit value) we have to pass this
		 * assertion.
		 */
		ASSERT(U_CURLIMIT(up, RLIMIT_DATA) <= INT_MAX);
		if (nextseg != NULL)
			retval = umin(retval, (u_long)nextseg->s_base);
		AS_LOCK_EXIT(as, &as->a_lock);

		/*
		 * Also handle case where rlimit[VMEM] has been
		 * lowered below the current address space size.
		 */

		size = (uint)(U_CURLIMIT(up, RLIMIT_VMEM) & PAGEMASK);

		/*
		 * Large Files: The following assertion has to
		 * pass through to ensure the correctness of the
		 * above casting.
		 */

		ASSERT(U_CURLIMIT(up, RLIMIT_VMEM) <= ULONG_MAX);

		if (as->a_size < size)
			size -= as->a_size;
		else
			size = 0;
		retval = umin(retval, (u_long)(brkend + size));

		/* truncate to 8-byte boundary to match sbrk */

		retval = retval & ~(8-1);
		break;
	}

	case UL_GDESLIM: /* Return approximate number of open files */

		ASSERT(U_CURLIMIT(up, RLIMIT_NOFILE) <= INT_MAX);
		retval = (rlim_t)U_CURLIMIT(up, RLIMIT_NOFILE);
		break;

	default:
		return (set_errno(EINVAL));

	}
	return (retval);
}

/*
 * Large Files: Getrlimit returns RLIM_SAVED_CUR or RLIM_SAVED_MAX when
 * rlim_cur or rlim_max is not representable in 32-bit rlim_t. These
 * values are just tokens which will be used in setrlimit to set the
 * correct limits. The current limits are saved in the saved_rlimit members
 * in user structures when the token is returned. setrlimit restores
 * the limit values to these saved values when the token is passed.
 * Consider the following common scenario of the apps:
 * 		limit = getrlimit();
 *		savedlimit = limit;
 * 		limit = limit1;
 *		setrlimit(limit)
 *		execute all processes in the new rlimit state.
 *		setrlimit(savedlimit) !!restore the old values.
 * Most apps don't check error returns from getrlimit setrlimit
 * and this is exactly why we return tokens when the correct value
 * cannot be represented in rlim_t. For more discussions refer to
 * the LFS API document.
 */

int
getrlimit(int resource, struct rlimit *rlp)
{
	struct rlimit rlimit;
	struct rlimit64 lrlimit;
	struct proc *p = ttoproc(curthread);
	struct user *up = PTOU(p);
	int savecur = 0;
	int savemax = 0;

	if (resource < 0 || resource >= RLIM_NLIMITS) {
		return (set_errno(EINVAL));
	}
	mutex_enter(&p->p_lock);
	lrlimit = up->u_rlimit[resource];
	mutex_exit(&p->p_lock);

	if (lrlimit.rlim_max > (rlim64_t)UINT_MAX) {

		if (lrlimit.rlim_max == RLIM64_INFINITY)
			rlimit.rlim_max = RLIM_INFINITY;
		else {
			savemax = 1;
			rlimit.rlim_max = RLIM_SAVED_MAX;
			ASSERT(resource == RLIMIT_FSIZE);
		}

		if (lrlimit.rlim_cur == RLIM64_INFINITY)
			rlimit.rlim_cur = RLIM_INFINITY;
		else if (lrlimit.rlim_cur == lrlimit.rlim_max) {
			savecur = 1;
			ASSERT(resource == RLIMIT_FSIZE);
			rlimit.rlim_cur = RLIM_SAVED_MAX;
		} else if (lrlimit.rlim_cur > (rlim64_t)UINT_MAX) {
			savecur = 1;
			rlimit.rlim_cur = RLIM_SAVED_CUR;
			ASSERT(resource == RLIMIT_FSIZE);
		} else
			rlimit.rlim_cur = lrlimit.rlim_cur;

		/*
		 * save the current limits in user structure.
		 */

		if (resource == RLIMIT_FSIZE) {
			mutex_enter(&p->p_lock);

			    if (savemax)
				up->u_saved_rlimit.rlim_max = lrlimit.rlim_max;
			    if (savecur)
				up->u_saved_rlimit.rlim_cur = lrlimit.rlim_cur;

			mutex_exit(&p->p_lock);
		}

	} else {
		ASSERT(lrlimit.rlim_cur <= (rlim64_t)UINT_MAX);
		rlimit.rlim_max = lrlimit.rlim_max;
		rlimit.rlim_cur = lrlimit.rlim_cur;
	}

	if (copyout((caddr_t)&rlimit, (caddr_t)rlp, sizeof (struct rlimit))) {
		return (set_errno(EFAULT));
	}
	return (0);
}

/*
 * See comments above getrlimit(). When thetokens are passed in the
 * rlimit structure the values are considered equal to the values
 * stored in saved_rlimit members of user structure.
 * When the user passes RLIM_INFINITY to set the resource limit to
 * unlimited internally understand this value as RLIM64_INFINITY and
 * let rlimit() do the job.
 */

int
setrlimit(int resource, struct rlimit *rlp)
{
	struct rlimit rlim;
	struct rlimit64 lfrlim;
	struct rlimit64 saved_rlim;
	int	error;
	struct proc *p = ttoproc(curthread);
	struct user *up = PTOU(p);

	if (resource < 0 || resource >= RLIM_NLIMITS)
		return (set_errno(EINVAL));
	if (copyin((caddr_t)rlp, (caddr_t)&rlim, sizeof (rlim)))
		return (set_errno(EFAULT));

	mutex_enter(&p->p_lock);
	saved_rlim = up->u_saved_rlimit;
	mutex_exit(&p->p_lock);

	switch (rlim.rlim_cur) {

	case RLIM_INFINITY:
		lfrlim.rlim_cur = RLIM64_INFINITY;
		break;
	case RLIM_SAVED_CUR:
		lfrlim.rlim_cur = saved_rlim.rlim_cur;
		break;
	case RLIM_SAVED_MAX:
		lfrlim.rlim_cur = saved_rlim.rlim_max;
		break;
	default:
		lfrlim.rlim_cur = (rlim64_t)rlim.rlim_cur;
		break;
	}

	switch (rlim.rlim_max) {

	case RLIM_INFINITY:
		lfrlim.rlim_max = RLIM64_INFINITY;
		break;
	case RLIM_SAVED_MAX:
		lfrlim.rlim_max = saved_rlim.rlim_max;
		break;
	case RLIM_SAVED_CUR:
		lfrlim.rlim_max = saved_rlim.rlim_cur;
		break;
	default:
		lfrlim.rlim_max = (rlim64_t)rlim.rlim_max;
		break;
	}

	if (error = rlimit(resource, lfrlim.rlim_cur,
					lfrlim.rlim_max))
		return (set_errno(error));
	return (0);
}

int
getrlimit64(int resource, struct rlimit64 *rlp)
{
	struct rlimit64 lrlimit;
	struct proc *p = ttoproc(curthread);
	struct user *up = PTOU(p);

	if (resource < 0 || resource >= RLIM_NLIMITS) {
		return (set_errno(EINVAL));
	}
	mutex_enter(&p->p_lock);
	lrlimit = up->u_rlimit[resource];
	mutex_exit(&p->p_lock);

	if (copyout((caddr_t)&lrlimit, (caddr_t)rlp,
				sizeof (struct rlimit64))) {
		return (set_errno(EFAULT));
	}
	return (0);
}

int
setrlimit64(int resource, struct rlimit64 *rlp)
{
	struct rlimit64 lrlim;
	int	error;

	if (resource < 0 || resource >= RLIM_NLIMITS)
		return (set_errno(EINVAL));
	if (copyin((caddr_t)rlp, (caddr_t)&lrlim, sizeof (struct rlimit64)))
		return (set_errno(EFAULT));
	if (error = rlimit(resource, lrlim.rlim_cur, lrlim.rlim_max))
		return (set_errno(error));
	return (0);
}
