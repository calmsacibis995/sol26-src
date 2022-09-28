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
 * 	(c) 1986, 1987, 1988, 1989, 1994, 1996  Sun Microsystems, Inc
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *			All rights reserved.
 *
 */

#ident	"@(#)rw.c	1.13	96/10/24 SMI"	/* SVr4 1.103	*/

#include <sys/param.h>
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/cpuvar.h>
#include <sys/uio.h>
#include <sys/ioreq.h>
#include <sys/debug.h>

/*
 * read, write, pread, pwrite, readv, and writev syscalls.
 * Large Files:  the behaviour of read depends on whether the fd
 * corresponds to large open or not.
 * Regular open: FOFFMAX flag not set.
 *		read until MAXOFF_T - 1 and read at MAXOFF_T returns
 *		EOVERFLOW if count is non-zero and if size of file
 *		is > MAXOFF_T. If size of file is <= MAXOFF_T read
 *		at >= MAXOFF_T returns EOF.
 */

int
read(int fdes, char *cbuf, size_t count)
{
	struct uio auio;
	struct iovec aiov;
	file_t *fp;
	register vnode_t *vp;
	struct cpu *cp;
	int cnt, fflag, ioflag, rwflag, bcount;
	int error = 0;
	u_offset_t fileoff;

	rwflag = 0;
	if ((cnt = (int)count) < 0)
		return (set_errno(EINVAL));


	if ((fp = GETF(fdes)) == NULL)
		return (set_errno(EBADF));
	if (((fflag = fp->f_flag) & FREAD) == 0) {
		error = EBADF;
		goto out;
	}
	vp = fp->f_vnode;

	if (vp->v_type == VREG && cnt == 0) {
		goto out;
	}

	aiov.iov_base = (caddr_t)cbuf;
	aiov.iov_len = cnt;
	VOP_RWLOCK(vp, rwflag);

	/*
	 * We do the following checks inside VOP_RWLOCK so as to
	 * prevent file size from changing while these checks are
	 * being done. Also, we load fp's offset to the local
	 * variable fileoff because we can have a parallel lseek
	 * going on (f_offset is not protected by any lock) which
	 * could change f_offset. We need to see the value only
	 * once here and take a decision. Seeing it more than once
	 * can lead to incorrect functionality.
	 */

	fileoff = (u_offset_t)fp->f_offset;
	if (fileoff >= OFFSET_MAX(fp) && (vp->v_type == VREG)) {
		struct vattr va;
		va.va_mask = AT_SIZE;
		if ((error = VOP_GETATTR(vp, &va, 0, fp->f_cred)))  {
			VOP_RWUNLOCK(vp, rwflag);
			goto out;
		}
		if (fileoff >= va.va_size) {
			cnt = 0;
			VOP_RWUNLOCK(vp, rwflag);
			goto out;
		} else {
			error = EOVERFLOW;
			VOP_RWUNLOCK(vp, rwflag);
			goto out;
		}
	}
	if ((vp->v_type == VREG) &&
	    (fileoff + cnt > OFFSET_MAX(fp))) {
		cnt = (int)(OFFSET_MAX(fp) - fileoff);
	}
	auio.uio_loffset = fileoff;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = bcount = cnt;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_llimit = U_CURLIMIT(&u, RLIMIT_FSIZE);
	auio.uio_fmode = fflag;

	ioflag = auio.uio_fmode & (FAPPEND|FSYNC|FDSYNC|FRSYNC);

	/* If read sync is not asked for, filter sync flags */
	if ((ioflag & FRSYNC) == 0)
		ioflag &= ~(FSYNC|FDSYNC);
	error = VOP_READ(vp, &auio, ioflag, fp->f_cred);
	cnt -= auio.uio_resid;
	CPU_STAT_ENTER_K();
	cp = CPU;
	CPU_STAT_ADDQ(cp, cpu_sysinfo.sysread, 1);
	CPU_STAT_ADDQ(cp, cpu_sysinfo.readch, (unsigned)cnt);
	CPU_STAT_EXIT_K();
	ttolwp(curthread)->lwp_ru.ioch += (unsigned)cnt;

	if (vp->v_type == VFIFO)	/* Backward compatibility */
		fp->f_offset = cnt;
	else if (((fp->f_flag & FAPPEND) == 0) ||
		(vp->v_type != VREG) || (bcount != 0))	/* POSIX */
		fp->f_offset = auio.uio_loffset;
	VOP_RWUNLOCK(vp, rwflag);

	if (error == EINTR && cnt != 0)
		error = 0;
out:
	RELEASEF(fdes);
	if (error)
		return (set_errno(error));
	return (cnt);
}

int
write(int fdes, char *cbuf, size_t count)
{
	struct uio auio;
	struct iovec aiov;
	file_t *fp;
	register vnode_t *vp;
	struct cpu *cp;
	int cnt, fflag, ioflag, rwflag, bcount;
	int error = 0;
	u_offset_t fileoff;

	if ((cnt = (int)count) < 0)
		return (set_errno(EINVAL));
	if ((fp = GETF(fdes)) == NULL)
		return (set_errno(EBADF));
	if (((fflag = fp->f_flag) & FWRITE) == 0) {
		error = EBADF;
		goto out;
	}
	vp = fp->f_vnode;

	if (vp->v_type == VREG && cnt == 0) {
		goto out;
	}

	rwflag = 1;
	aiov.iov_base = (caddr_t)cbuf;
	aiov.iov_len = cnt;

	VOP_RWLOCK(vp, rwflag);

	fileoff = fp->f_offset;
	if (vp->v_type == VREG) {

		/*
		 * We raise psignal if write for >0 bytes causes
		 * it to exceed the ulimit.
		 */
		if (fileoff >= U_CURLIMIT(&u, RLIMIT_FSIZE)) {
			VOP_RWUNLOCK(vp, rwflag);
			psignal(ttoproc(curthread), SIGXFSZ);
			error = EFBIG;
			goto out;
		}
		/*
		 * We  return EFBIG if write is done at an offset
		 * greater than the offset maximum for this file structure.
		 */

		if (fileoff >= OFFSET_MAX(fp)) {
			VOP_RWUNLOCK(vp, rwflag);
			error = EFBIG;
			goto out;
		}
		/*
		 * Limit the bytes to be written  upto offset maximum for
		 * this open file structure.
		 */
		if (fileoff + cnt > OFFSET_MAX(fp))
			cnt = (int)(OFFSET_MAX(fp)- fileoff);
	}
	auio.uio_loffset = fileoff;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = bcount = cnt;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_llimit = U_CURLIMIT(&u, RLIMIT_FSIZE);
	auio.uio_fmode = fflag;

	ioflag = auio.uio_fmode & (FAPPEND|FSYNC|FDSYNC|FRSYNC);

	error = VOP_WRITE(vp, &auio, ioflag, fp->f_cred);
	cnt -= auio.uio_resid;
	CPU_STAT_ENTER_K();
	cp = CPU;
	CPU_STAT_ADDQ(cp, cpu_sysinfo.syswrite, 1);
	CPU_STAT_ADDQ(cp, cpu_sysinfo.writech, (unsigned)cnt);
	CPU_STAT_EXIT_K();
	ttolwp(curthread)->lwp_ru.ioch += (unsigned)cnt;

	if (vp->v_type == VFIFO)	/* Backward compatibility */
		fp->f_offset = cnt;
	else if (((fp->f_flag & FAPPEND) == 0) ||
		(vp->v_type != VREG) || (bcount != 0))	/* POSIX */
		fp->f_offset = auio.uio_loffset;
	VOP_RWUNLOCK(vp, rwflag);

	if (error == EINTR && cnt != 0)
		error = 0;
out:
	RELEASEF(fdes);
	if (error)
		return (set_errno(error));
	return (cnt);
}

int
pread(int fdes, char *cbuf, size_t count, off_t offset)
{
	struct uio auio;
	struct iovec aiov;
	file_t *fp;
	register vnode_t *vp;
	struct cpu *cp;
	int fflag, ioflag, rwflag, bcount;
	int error = 0;
	u_offset_t fileoff = (u_offset_t)offset;

	if ((bcount = (int)count) < 0)
		return (set_errno(EINVAL));


	if ((fp = GETF(fdes)) == NULL)
		return (set_errno(EBADF));
	if (((fflag = fp->f_flag) & (FREAD)) == 0) {
		error = EBADF;
		goto out;
	}
	vp = fp->f_vnode;
	if (vp->v_type == VREG && bcount == 0) {
		goto out;
	}

	rwflag = 0;
	if (vp->v_type == VREG) {
		/*
		 * Return EINVAL if an invalid offset comes to pread.
		 * Negative offset from user will cause this error.
		 */

		if (fileoff > MAXOFF_T) {
			error = EINVAL;
			goto out;
		}
		/*
		 * Limit offset such that we don't read or write
		 * a file beyond the MAXOFF_T representable in an off_t
		 * structure.
		 */
		if (fileoff + bcount > MAXOFF_T)
			bcount = (int)((offset_t)MAXOFF_T - fileoff);
	}


	aiov.iov_base = (caddr_t)cbuf;
	aiov.iov_len = bcount;
	VOP_RWLOCK(vp, rwflag);
	if ((vp->v_type == VREG) && (fileoff == (u_offset_t)MAXOFF_T)) {
		struct vattr va;
		va.va_mask = AT_SIZE;
		if ((error = VOP_GETATTR(vp, &va, 0, fp->f_cred))) {
			VOP_RWUNLOCK(vp, rwflag);
			goto out;
		}
		VOP_RWUNLOCK(vp, rwflag);

		/*
		 * We have to return EOF if fileoff is >= file size.
		 */
		if (fileoff >= va.va_size) {
			bcount = 0;
			goto out;
		}

		/*
		 * File is greater than or equal to MAXOFF_T and therefore
		 * we return EOVERFLOW.
		 */
		error = EOVERFLOW;
		goto out;
	}
	auio._uio_offset._p._u = 0;
	auio._uio_offset._p._l = (off_t)fileoff;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = bcount;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_llimit = U_CURLIMIT(&u, RLIMIT_FSIZE);
	auio.uio_fmode = fflag;

	ioflag = auio.uio_fmode & (FAPPEND|FSYNC|FDSYNC|FRSYNC);

	/* If read sync is not asked for, filter sync flags */
	if ((ioflag & FRSYNC) == 0)
		ioflag &= ~(FSYNC|FDSYNC);
	error = VOP_READ(vp, &auio, ioflag, fp->f_cred);
	bcount -= auio.uio_resid;
	CPU_STAT_ENTER_K();
	cp = CPU;
	CPU_STAT_ADDQ(cp, cpu_sysinfo.sysread, 1);
	CPU_STAT_ADDQ(cp, cpu_sysinfo.readch, (unsigned)bcount);
	CPU_STAT_EXIT_K();
	ttolwp(curthread)->lwp_ru.ioch += (unsigned)bcount;
	VOP_RWUNLOCK(vp, rwflag);

	if (error == EINTR && bcount != 0)
		error = 0;
out:
	RELEASEF(fdes);
	if (error)
		return (set_errno(error));
	return (bcount);
}

int
pwrite(int fdes, char *cbuf, size_t count, off_t offset)
{
	struct uio auio;
	struct iovec aiov;
	file_t *fp;
	register vnode_t *vp;
	struct cpu *cp;
	int fflag, ioflag, rwflag, bcount;
	int error = 0;
	u_offset_t fileoff = (u_offset_t)offset;

	if ((bcount = (int)count) < 0) {
		return (set_errno(EINVAL));
	}
	if ((fp = GETF(fdes)) == NULL)
		return (set_errno(EBADF));
	if (((fflag = fp->f_flag) & (FWRITE)) == 0) {
		error = EBADF;
		goto out;
	}
	rwflag = 1;
	vp = fp->f_vnode;
	if (vp->v_type == VREG && bcount == 0) {
		goto out;
	}

	if (vp->v_type == VREG) {
		/*
		 * return EINVAL for offsets that cannot be
		 * represented in an off_t.
		 */
		if (fileoff > MAXOFF_T) {
			error = EINVAL;
			goto out;
		}
		/*
		 * Raise signal if we are trying to write above
		 * the resource limit.
		 */
		if (fileoff >= U_CURLIMIT(&u, RLIMIT_FSIZE)) {
			psignal(ttoproc(curthread), SIGXFSZ);
			error = EFBIG;
			goto out;
		}
		/*
		 * Don't allow pwrite to cause file sizes to exceed
		 * MAXOFF_T.
		 */
		if (fileoff == MAXOFF_T) {
			error = EFBIG;
			goto out;
		}
		if (fileoff + count > MAXOFF_T)
			bcount = (int)((u_offset_t)MAXOFF_T - fileoff);
	}
	aiov.iov_base = (caddr_t)cbuf;
	aiov.iov_len = bcount;
	VOP_RWLOCK(vp, rwflag);
	auio._uio_offset._p._u = 0;
	auio._uio_offset._p._l = (off_t)fileoff;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = bcount;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_llimit = U_CURLIMIT(&u, RLIMIT_FSIZE);
	auio.uio_fmode = fflag;

	ioflag = auio.uio_fmode & (FAPPEND|FSYNC|FDSYNC|FRSYNC);

	error = VOP_WRITE(vp, &auio, ioflag, fp->f_cred);
	bcount -= auio.uio_resid;
	CPU_STAT_ENTER_K();
	cp = CPU;
	CPU_STAT_ADDQ(cp, cpu_sysinfo.syswrite, 1);
	CPU_STAT_ADDQ(cp, cpu_sysinfo.writech, (unsigned)bcount);
	CPU_STAT_EXIT_K();
	ttolwp(curthread)->lwp_ru.ioch += (unsigned)bcount;
	VOP_RWUNLOCK(vp, rwflag);

	if (error == EINTR && bcount != 0)
		error = 0;
out:
	RELEASEF(fdes);
	if (error)
		return (set_errno(error));
	return (bcount);
}

/*
 * XXX -- The SVID refers to IOV_MAX, but doesn't define it.  Grrrr....
 * XXX -- However, SVVS expects readv() and writev() to fail if
 * XXX -- iovcnt > 16 (yes, it's hard-coded in the SVVS source),
 * XXX -- so I guess that's the "interface".
 */
#define	DEF_IOV_MAX	16

int
readv(int fdes, struct iovec *iovp, int iovcnt)
{
	struct uio auio;
	struct iovec aiov[DEF_IOV_MAX];
	file_t *fp;
	register vnode_t *vp;
	struct cpu *cp;
	int fflag, ioflag, rwflag, count, bcount;
	int error = 0;
	int i;
	u_offset_t fileoff;

	if (iovcnt <= 0 || iovcnt > DEF_IOV_MAX)
		return (set_errno(EINVAL));

	if (copyin((caddr_t)iovp, (caddr_t)aiov,
	    (unsigned)iovcnt * sizeof (struct iovec)))
		return (set_errno(EFAULT));

	count = 0;
	for (i = 0; i < iovcnt; i++) {
		int iovlen = aiov[i].iov_len;
		count += iovlen;
		if (iovlen < 0 || count < 0)
			return (set_errno(EINVAL));
	}
	if ((fp = GETF(fdes)) == NULL)
		return (set_errno(EBADF));
	if (((fflag = fp->f_flag) & FREAD) == 0) {
		error = EBADF;
		goto out;
	}
	vp = fp->f_vnode;
	if (vp->v_type == VREG && count == 0) {
		goto out;
	}

	rwflag = 0;
	VOP_RWLOCK(vp, rwflag);
	fileoff = fp->f_offset;

	/*
	 * Behaviour is same as read. Please see comments in read.
	 */

	if ((vp->v_type == VREG) && (fileoff >= OFFSET_MAX(fp))) {
		struct vattr va;
		va.va_mask = AT_SIZE;
		if ((error = VOP_GETATTR(vp, &va, 0, fp->f_cred)))  {
			VOP_RWUNLOCK(vp, rwflag);
			goto out;
		}
		if (fileoff >= va.va_size) {
			VOP_RWUNLOCK(vp, rwflag);
			count = 0;
			goto out;
		} else {
			VOP_RWUNLOCK(vp, rwflag);
			error = EOVERFLOW;
			goto out;
		}
	}
	if ((vp->v_type == VREG) && (fileoff + count > OFFSET_MAX(fp))) {
		count = (int)(OFFSET_MAX(fp) - fileoff);
	}
	auio.uio_loffset = fileoff;
	auio.uio_iov = aiov;
	auio.uio_iovcnt = iovcnt;
	auio.uio_resid = bcount = count;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_llimit = U_CURLIMIT(&u, RLIMIT_FSIZE);
	auio.uio_fmode = fflag;

	ioflag = auio.uio_fmode & (FAPPEND|FSYNC|FDSYNC|FRSYNC);

	/* If read sync is not asked for, filter sync flags */
	if ((ioflag & FRSYNC) == 0)
		ioflag &= ~(FSYNC|FDSYNC);
	error = VOP_READ(vp, &auio, ioflag, fp->f_cred);
	count -= auio.uio_resid;
	CPU_STAT_ENTER_K();
	cp = CPU;
	CPU_STAT_ADDQ(cp, cpu_sysinfo.sysread, 1);
	CPU_STAT_ADDQ(cp, cpu_sysinfo.readch, (unsigned)count);
	CPU_STAT_EXIT_K();
	ttolwp(curthread)->lwp_ru.ioch += (unsigned)count;

	if (vp->v_type == VFIFO)	/* Backward compatibility */
		fp->f_offset = count;
	else if (((fp->f_flag & FAPPEND) == 0) ||
		(vp->v_type != VREG) || (bcount != 0))	/* POSIX */
		fp->f_offset = auio.uio_loffset;

	VOP_RWUNLOCK(vp, rwflag);

	if (error == EINTR && count != 0)
		error = 0;
out:
	RELEASEF(fdes);
	if (error)
		return (set_errno(error));
	return (count);
}

int
writev(int fdes, struct iovec *iovp, int iovcnt)
{
	struct uio auio;
	struct iovec aiov[DEF_IOV_MAX];
	file_t *fp;
	register vnode_t *vp;
	struct cpu *cp;
	int fflag, ioflag, rwflag, count, bcount;
	int error = 0;
	int i;
	u_offset_t fileoff;

	if (iovcnt <= 0 || iovcnt > DEF_IOV_MAX) {
		return (set_errno(EINVAL));
	}
	if (copyin((caddr_t)iovp, (caddr_t)aiov,
	    (unsigned)iovcnt * sizeof (struct iovec))) {
		return (set_errno(EFAULT));
	}
	count = 0;
	for (i = 0; i < iovcnt; i++) {
		int iovlen = aiov[i].iov_len;
		count += iovlen;
		if (iovlen < 0 || count < 0) {
			return (set_errno(EINVAL));
		}
	}


	if ((fp = GETF(fdes)) == NULL)
		return (set_errno(EBADF));
	if (((fflag = fp->f_flag) & FWRITE) == 0) {
		error = EBADF;
		goto out;
	}
	vp = fp->f_vnode;
	if (vp->v_type == VREG && count == 0) {
		goto out;
	}

	rwflag = 1;

	VOP_RWLOCK(vp, rwflag);

	fileoff = fp->f_offset;

	/*
	 * Behaviour is same as write. Please see comments for write.
	 */

	if (vp->v_type == VREG) {
		if (fileoff >= U_CURLIMIT(&u, RLIMIT_FSIZE)) {
			psignal(ttoproc(curthread), SIGXFSZ);
			VOP_RWUNLOCK(vp, rwflag);
			error = EFBIG;
			goto out;
		}
		if (fileoff >= OFFSET_MAX(fp)) {
			VOP_RWUNLOCK(vp, rwflag);
			error = EFBIG;
			goto out;
		}
		if (fileoff + count > OFFSET_MAX(fp))
		    count = (int)(OFFSET_MAX(fp) - fileoff);
	}
	auio.uio_loffset = fileoff;
	auio.uio_iov = aiov;
	auio.uio_iovcnt = iovcnt;
	auio.uio_resid = bcount = count;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_llimit = U_CURLIMIT(&u, RLIMIT_FSIZE);
	auio.uio_fmode = fflag;

	ioflag = auio.uio_fmode & (FAPPEND|FSYNC|FDSYNC|FRSYNC);

	error = VOP_WRITE(vp, &auio, ioflag, fp->f_cred);
	count -= auio.uio_resid;
	CPU_STAT_ENTER_K();
	cp = CPU;
	CPU_STAT_ADDQ(cp, cpu_sysinfo.syswrite, 1);
	CPU_STAT_ADDQ(cp, cpu_sysinfo.writech, (unsigned)count);
	CPU_STAT_EXIT_K();
	ttolwp(curthread)->lwp_ru.ioch += (unsigned)count;

	if (vp->v_type == VFIFO)	/* Backward compatibility */
		fp->f_offset = count;
	else if (((fp->f_flag & FAPPEND) == 0) ||
		(vp->v_type != VREG) || (bcount != 0))	/* POSIX */
		fp->f_offset = auio.uio_loffset;
	VOP_RWUNLOCK(vp, rwflag);


	if (error == EINTR && count != 0)
		error = 0;
out:
	RELEASEF(fdes);
	if (error)
		return (set_errno(error));
	return (count);
}

int
pread64(int fdes, char *cbuf, size_t count, off64_t offset)
{
	struct uio auio;
	struct iovec aiov;
	file_t *fp;
	register vnode_t *vp;
	struct cpu *cp;
	int fflag, ioflag, rwflag, bcount;
	int error = 0;
	u_offset_t fileoff = (u_offset_t)offset;

	if ((bcount = (int)count) < 0) {
		return (set_errno(EINVAL));
	}

	if ((fp = GETF(fdes)) == NULL)
		return (set_errno(EBADF));
	if (((fflag = fp->f_flag) & (FREAD)) == 0) {
		error = EBADF;
		goto out;
	}
	vp = fp->f_vnode;
	if (vp->v_type == VREG && bcount == 0) {
		goto out;
	}

	rwflag = 0;
	if (vp->v_type == VREG) {
		/*
		 * Same as pread. See comments in pread.
		 */

		if (fileoff > MAXOFFSET_T) {
			error = EINVAL;
			goto out;
		}
		if (fileoff + bcount > MAXOFFSET_T) {
			bcount = (int)(MAXOFFSET_T - fileoff);
		}
	}
	aiov.iov_base = (caddr_t)cbuf;
	aiov.iov_len = bcount;
	VOP_RWLOCK(vp, rwflag);
	auio.uio_loffset = fileoff;

	/*
	 * Note: File size can never be greater than MAXOFFSET_T.
	 * If ever we start supporting 128 bit files the code
	 * similar to the one in pread at this place should be here.
	 * Here we avoid the unnecessary VOP_GETATTR() when we
	 * know that fileoff == MAXOFFSET_T implies that it is always
	 * greater than or equal to file size.
	 */
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = bcount;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_llimit = U_CURLIMIT(&u, RLIMIT_FSIZE);
	auio.uio_fmode = fflag;

	ioflag = auio.uio_fmode & (FAPPEND|FSYNC|FDSYNC|FRSYNC);

	/* If read sync is not asked for, filter sync flags */
	if ((ioflag & FRSYNC) == 0)
		ioflag &= ~(FSYNC|FDSYNC);
	error = VOP_READ(vp, &auio, ioflag, fp->f_cred);
	bcount -= auio.uio_resid;
	CPU_STAT_ENTER_K();
	cp = CPU;
	CPU_STAT_ADDQ(cp, cpu_sysinfo.sysread, 1);
	CPU_STAT_ADDQ(cp, cpu_sysinfo.readch, (unsigned)bcount);
	CPU_STAT_EXIT_K();
	ttolwp(curthread)->lwp_ru.ioch += (unsigned)bcount;
	VOP_RWUNLOCK(vp, rwflag);

	if (error == EINTR && bcount != 0)
		error = 0;
out:
	RELEASEF(fdes);
	if (error)
		return (set_errno(error));
	return (bcount);
}

int
pwrite64(int fdes, char *cbuf, size_t count, off64_t offset)
{
	struct uio auio;
	struct iovec aiov;
	file_t *fp;
	register vnode_t *vp;
	struct cpu *cp;
	int fflag, ioflag, rwflag, bcount;
	int error = 0;
	u_offset_t fileoff = (u_offset_t)offset;

	if ((bcount = (int)count) < 0) {
		return (set_errno(EINVAL));
	}
	if ((fp = GETF(fdes)) == NULL)
		return (set_errno(EBADF));
	if (((fflag = fp->f_flag) & (FWRITE)) == 0) {
		error = EBADF;
		goto out;
	}
	vp = fp->f_vnode;
	if (vp->v_type == VREG && bcount == 0) {
		goto out;
	}

	rwflag = 1;
	if ((vp->v_type == VREG) && (fileoff > MAXOFFSET_T)) {
		error = EINVAL;
		goto out;
	}
	if (vp->v_type == VREG) {
		/*
		 * See comments in pwrite.
		 */

		if (fileoff >= U_CURLIMIT(&u, RLIMIT_FSIZE)) {
			psignal(ttoproc(curthread), SIGXFSZ);
			error = EFBIG;
			goto out;
		}
		if (fileoff == MAXOFFSET_T) {
			error = EFBIG;
			goto out;
		}
		if (fileoff + bcount > MAXOFFSET_T)
			bcount = (int)((u_offset_t)MAXOFFSET_T - fileoff);
	}
	aiov.iov_base = (caddr_t)cbuf;
	aiov.iov_len = bcount;
	VOP_RWLOCK(vp, rwflag);
	auio.uio_loffset = fileoff;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = bcount;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_llimit = U_CURLIMIT(&u, RLIMIT_FSIZE);
	auio.uio_fmode = fflag;

	ioflag = auio.uio_fmode & (FAPPEND|FSYNC|FDSYNC|FRSYNC);

	error = VOP_WRITE(vp, &auio, ioflag, fp->f_cred);
	bcount -= auio.uio_resid;
	CPU_STAT_ENTER_K();
	cp = CPU;
	CPU_STAT_ADDQ(cp, cpu_sysinfo.syswrite, 1);
	CPU_STAT_ADDQ(cp, cpu_sysinfo.writech, (unsigned)bcount);
	CPU_STAT_EXIT_K();
	ttolwp(curthread)->lwp_ru.ioch += (unsigned)bcount;
	VOP_RWUNLOCK(vp, rwflag);

	if (error == EINTR && bcount != 0)
		error = 0;
out:
	RELEASEF(fdes);
	if (error)
		return (set_errno(error));
	return (bcount);
}
