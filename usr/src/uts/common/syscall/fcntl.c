/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	Copyright (c) 1986,1987,1988,1989,1994,1996 by Sun Microsystems, Inc
 * 	Copyright (c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	All rights reserved.
 *
 */

#ident	"@(#)fcntl.c	1.12	96/07/01 SMI"	/* SVr4 1.103	*/

#include <sys/param.h>
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/mode.h>
#include <sys/proc.h>
#include <sys/filio.h>
#include <sys/share.h>
#include <sys/debug.h>

static int flock_check(vnode_t *, flock64_t *, offset_t, offset_t);
/*
 * File control.
 */

int
fcntl(int fdes, int cmd, int arg)
{
	file_t *fp;
	register int retval = 0;
	vnode_t *vp;
	u_offset_t offset;
	int flag;
	struct flock sbf;
	struct flock64 bf;
	struct o_flock obf;
	struct fshare fsh;
	struct shrlock shr;
	struct shr_locowner shr_own;
	extern int dupfdes(), dupdest();

	if ((fp = GETF(fdes)) == NULL)
		return (set_errno(EBADF));
	vp = fp->f_vnode;
	flag = fp->f_flag;
	offset = fp->f_offset;

	switch (cmd) {

	case F_DUPFD:
		if (arg < 0 || arg >= U_CURLIMIT(&u, RLIMIT_NOFILE))
			retval = set_errno(EINVAL);
		else
			retval = dupfdes(fp, arg);
		break;

	case F_DUP2FD:
		if (arg < 0 || arg >= U_CURLIMIT(&u, RLIMIT_NOFILE))
			retval = set_errno(EBADF);
		else if (fdes == arg)
			retval = arg;
		else
			retval = dupdest(fp, arg);
		break;

	case F_GETFD:
		retval = getpof(fdes);
		break;

	case F_SETFD:
		(void) setpof(fdes, (char)arg);
		break;

	case F_GETFL:
		mutex_enter(&fp->f_tlock);
		retval = fp->f_flag+FOPEN;
		mutex_exit(&fp->f_tlock);
		break;

	case F_SETFL:
		if ((arg & (FNONBLOCK|FNDELAY)) == (FNONBLOCK|FNDELAY))
			arg &= ~FNDELAY;
		if (retval = VOP_SETFL(vp, flag, arg, fp->f_cred)) {
			retval = set_errno(retval);
		} else {
			arg &= FMASK;
			mutex_enter(&fp->f_tlock);
			fp->f_flag &= (FREAD|FWRITE);
			fp->f_flag |= (arg-FOPEN) & ~(FREAD|FWRITE);
			mutex_exit(&fp->f_tlock);
		}
		break;

	/*
	 * Large Files: File system and vnode layers understand and implement
	 * locking with flock64 structures. So here once we pass through
	 * the test for compatibility as defined by LFS API, (for F_SETLK,
	 * F_SETLKW, F_GETLK, F_GETLKW, F_FREESP) we transform
	 * the flock structure to a flock64 structure and send it to the
	 * lower layers. Similarly in case of GETLK the returned flock64
	 * structure is transformed to a flock structure if everything fits
	 * in nicely otherwise we return EOVERFLOW.
	 */

	case F_GETLK:
	case F_O_GETLK:
	case F_SETLK:
	case F_SETLKW:
		/*
		 * Copy in input fields only.
		 */
		if (cmd == F_O_GETLK) {
			if (copyin((caddr_t)arg, (caddr_t)&sbf, sizeof (obf))) {
				retval = set_errno(EFAULT);
				break;
			}
		} else {
			if (copyin((caddr_t)arg, (caddr_t)&sbf, sizeof (sbf))) {
				retval = set_errno(EFAULT);
				break;
			}
		}

		bf.l_type = sbf.l_type;
		bf.l_whence = sbf.l_whence;
		bf.l_start = (offset_t)sbf.l_start;
		bf.l_len = (offset_t)sbf.l_len;
		bf.l_sysid = sbf.l_sysid;
		bf.l_pid = sbf.l_pid;


		/*
		 * 64-bit support: check for overflow for 32-bit lock ops
		 */
		if ((retval = flock_check(vp, &bf, offset, MAXOFF_T))) {
			retval = set_errno(retval);
			break;
		}


		if (retval =
		    VOP_FRLOCK(vp, cmd, &bf, flag, offset, fp->f_cred)) {
			retval = set_errno(retval);
			break;
		}

		/*
		 * If command is GETLK and no lock is found, only
		 * the type field is changed.
		 */
		if ((cmd == F_O_GETLK || cmd == F_GETLK) &&
		    bf.l_type == F_UNLCK) {
			if (copyout((caddr_t)&bf.l_type,
			    (caddr_t)&((struct flock *)arg)->l_type,
			    sizeof (bf.l_type)))
				retval = set_errno(EFAULT);
			break;
		}

		if (cmd == F_O_GETLK) {
			/*
			 * Return an SVR3 flock structure to the user.
			 */
			obf.l_type = bf.l_type;
			obf.l_whence = bf.l_whence;
			obf.l_start = bf.l_start;
			obf.l_len = bf.l_len;
			if (bf.l_sysid > SHRT_MAX || bf.l_pid > SHRT_MAX) {
				/*
				 * One or both values for the above fields
				 * is too large to store in an SVR3 flock
				 * structure.
				 */
				retval = set_errno(EOVERFLOW);
				break;
			}
			obf.l_sysid = (short)bf.l_sysid;
			obf.l_pid = (o_pid_t)bf.l_pid;
			if (copyout((caddr_t)&obf, (caddr_t)arg, sizeof (obf)))
				retval = set_errno(EFAULT);
		} else if (cmd == F_GETLK) {
			/*
			 * Copy out SVR4 flock.
			 */
			int i;

			for (i = 0; i < 4; i++)
				sbf.l_pad[i] = 0;
				if (bf.l_start > MAXOFF_T ||
					bf.l_len > MAXOFF_T) {
					retval = set_errno(EOVERFLOW);
				} else {
					sbf.l_type = bf.l_type;
					sbf.l_whence = bf.l_whence;
					sbf.l_start = (off_t)bf.l_start;
					sbf.l_len = (off_t)bf.l_len;
					sbf.l_sysid = bf.l_sysid;
					sbf.l_pid = bf.l_pid;
					if (copyout((caddr_t)&sbf, (caddr_t)arg,
						sizeof (sbf)))
					retval = set_errno(EFAULT);
				}
		}
		break;

	case F_CHKFL:
		/*
		 * This is for internal use only, to allow the vnode layer
		 * to validate a flags setting before applying it.  User
		 * programs can't issue it.
		 */
		retval = set_errno(EINVAL);
		break;

	case F_ALLOCSP:
	case F_FREESP:
		if ((flag & FWRITE) == 0)
			retval = EBADF;
		else if (vp->v_type != VREG)
			retval = EINVAL;
		/*
		 * For compatibility we overlay an SVR3 flock on an SVR4
		 * flock.  This works because the input field offsets
		 * in "struct flock" were preserved.
		 */
		else if (copyin((caddr_t)arg, (caddr_t)&sbf, sizeof (obf)))
			retval = EFAULT;
		else {
			bf.l_type = sbf.l_type;
			bf.l_whence = sbf.l_whence;
			bf.l_start = sbf.l_start;
			bf.l_len = sbf.l_len;
			bf.l_sysid = sbf.l_sysid;
			bf.l_pid = sbf.l_pid;

			if ((retval = flock_check(vp, &bf,
			    offset, MAXOFF_T))) {
				retval = set_errno(retval);
				break;
			}

			retval = VOP_SPACE(vp, cmd, &bf, flag, offset,
			    fp->f_cred);
		}
		if (retval)
			retval = set_errno(retval);
		break;

	case F_GETLK64:
	case F_SETLK64:
	case F_SETLKW64:
		/*
		 * Large Files: Here we set cmd as *LK and send it to
		 * lower layers. *LK64 is only for the user land.
		 * Most of the comments described above for F_SETLK
		 * applies here too.
		 */

		if (cmd == F_GETLK64)
			cmd = F_GETLK;
		else if (cmd == F_SETLK64)
			cmd = F_SETLK;
		else if (cmd == F_SETLKW64)
			cmd = F_SETLKW;

		if (copyin((caddr_t)arg, (caddr_t)&bf, sizeof (bf))) {
			retval = set_errno(EFAULT);
			break;
		}

		if ((retval = flock_check(vp, &bf,
		    offset, MAXOFFSET_T))) {
			retval = set_errno(retval);
			break;
		}

		if (retval =
		    VOP_FRLOCK(vp, cmd, &bf, flag, offset, fp->f_cred)) {
			retval = set_errno(retval);
			break;
		}

		if ((cmd == F_GETLK) && bf.l_type == F_UNLCK) {
			if (copyout((caddr_t)&bf.l_type,
			    (caddr_t)&((struct flock *)arg)->l_type,
			    sizeof (bf.l_type)))
				retval = set_errno(EFAULT);
			break;
		}

		if (cmd == F_GETLK) {
			int i;

			for (i = 0; i < 4; i++) {
				bf.l_pad[i] = 0;
			}
			if (copyout((caddr_t)&bf, (caddr_t)arg,
					sizeof (bf))) {
				retval = set_errno(EFAULT);
			}
		}
		break;

	case F_FREESP64:
		cmd = F_FREESP;
		if ((flag & FWRITE) == 0)
			retval = EBADF;
		else if (vp->v_type != VREG)
			retval = EINVAL;
		else if (copyin((caddr_t)arg, (caddr_t)&bf, sizeof (bf)))
			retval = EFAULT;
		else {
			if ((retval = flock_check(vp, &bf,
			    offset, MAXOFFSET_T))) {
				retval = set_errno(retval);
				break;
			}
			retval = VOP_SPACE(vp, cmd, &bf, flag, offset,
			    fp->f_cred);
		}
		if (retval)
			retval = set_errno(retval);
		break;

	case F_SHARE:
	case F_UNSHARE:
		/*
		 * Copy in input fields only.
		 */
		if (copyin((caddr_t)arg, (caddr_t)&fsh, sizeof (fsh))) {
			retval = set_errno(EFAULT);
			break;
		}

		/*
		 * Local share reservations always have this simple form
		 */
		shr.access = fsh.f_access;
		shr.deny = fsh.f_deny;
		shr.sysid = 0;
		shr.pid = ttoproc(curthread)->p_pid;
		shr_own.pid = shr.pid;
		shr_own.id = fsh.f_id;
		shr.own_len = sizeof (shr_own);
		shr.owner = (caddr_t)&shr_own;
		if (retval =
		    VOP_SHRLOCK(vp, cmd, &shr, flag)) {
			retval = set_errno(retval);
			break;
		}
		break;

	default:
		retval = set_errno(EINVAL);
		break;
	}

	RELEASEF(fdes);
	return (retval);
}

int
flock_check(vnode_t *vp, flock64_t *flp, offset_t offset,
		offset_t max)
{
	struct vattr	vattr;
	int	error;
	u_offset_t start, end;

	/*
	 * Determine the starting point of the request
	 */
	switch (flp->l_whence) {
	case 0:		/* SEEK_SET */
		start = (u_offset_t) flp->l_start;
		if (start > max)
			return (EINVAL);
		break;
	case 1:		/* SEEK_CUR */
		if (flp->l_start > (max - offset))
			return (EOVERFLOW);
		start = (u_offset_t) (flp->l_start + offset);
		if (start > max)
			return (EINVAL);
		break;
	case 2:		/* SEEK_END */
		vattr.va_mask = AT_SIZE;
		if (error = VOP_GETATTR(vp, &vattr, 0, CRED()))
			return (error);
		if (flp->l_start > (max - (offset_t)vattr.va_size))
			return (EOVERFLOW);
		start = (u_offset_t) (flp->l_start + (offset_t)vattr.va_size);
		if (start > max)
			return (EINVAL);
		break;
	default:
		return (EINVAL);
	}

	/*
	 * Determine the range covered by the request.
	 */
	if (flp->l_len == 0)
		end = MAXEND;
	else if ((offset_t) flp->l_len > 0) {
		if (flp->l_len > (max - start + 1))
			return (EOVERFLOW);
		end = (u_offset_t) (start + (flp->l_len - 1));
		ASSERT(end <= max);
	} else {
		/*
		 * Negative length; why do we even allow this ?
		 * Because this allows easy specification of
		 * the last n bytes of the file.
		 */
		end = start;
		start += (u_offset_t) flp->l_len;
		(start)++;
		if (start > max)
			return (EINVAL);
		ASSERT(end <= max);
	}
	ASSERT(start <= max);
	if (flp->l_type == F_UNLCK && flp->l_len > 0 &&
			end == (offset_t)max) {
		flp->l_len = 0;
	}
	if (start  > end)
		return (EINVAL);
	return (0);
}
