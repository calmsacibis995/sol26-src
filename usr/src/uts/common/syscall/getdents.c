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
 * 	(c) 1986, 1987, 1988, 1989, 1996  Sun Microsystems, Inc
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *			All rights reserved.
 *
 */

#ident	"@(#)getdents.c	1.9	96/08/25 SMI"	/* SVr4 1.103	*/

#include <sys/param.h>
#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/cred.h>
#include <sys/dirent.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/mode.h>
#include <sys/uio.h>
#include <sys/ioreq.h>
#include <sys/filio.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>

/*
 * Get directory entries in a file system-independent format.
 *
 * Largefiles: This function now allocates a buffer to grab the
 * directory entries in dirent64 formats from VOP_READDIR routines.
 * The dirent64 structures are converted to dirent structures and
 * copied to the user space.
 * Libc now uses getdents64() and therefore we don't expect any
 * major performance impact due to the extra kmem_alloc's done
 * in this routine.
 */

#define	MAXGETDENTS_SIZE	(64 * 1024)

int
getdents(int fd, char *buf, int count)
{
	register vnode_t *vp;
	file_t *fp;
	struct uio auio;
	struct iovec aiov;
	register int error;
	int sink;
	char *newbuf;
	char *obuf;
	int bufsize;
	int osize, nsize;
	struct dirent64 *dp;
	struct dirent *op;

	if (count < sizeof (struct dirent))
		return (set_errno(EINVAL));

	if ((fp = GETF(fd)) == NULL)
		return (set_errno(EBADF));
	vp = fp->f_vnode;
	if (vp->v_type != VDIR) {
		RELEASEF(fd);
		return (set_errno(ENOTDIR));
	}

	/*
	 * We limit the maximum size read by getdents() to 64 KB so as
	 * not to have a user hog all the kernel memory.
	 */

	if (count > MAXGETDENTS_SIZE) {
		count = MAXGETDENTS_SIZE;
	}

	bufsize = count;
	newbuf = kmem_zalloc(bufsize, KM_SLEEP);
	obuf = kmem_zalloc(bufsize, KM_SLEEP);

	aiov.iov_base = newbuf;
	aiov.iov_len = count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_loffset = fp->f_offset;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_resid = count;
	auio.uio_fmode = 0;
	VOP_RWLOCK(vp, 0);
	error = VOP_READDIR(vp, &auio, fp->f_cred, &sink);
	VOP_RWUNLOCK(vp, 0);
	if (error)
		goto out;
	count = count - auio.uio_resid;
	fp->f_offset = auio.uio_loffset;

	dp = (struct dirent64 *)newbuf;
	op = (struct dirent *)obuf;
	osize = 0;
	nsize = 0;

	while (nsize < count) {
		if (dp->d_ino > (ino64_t)UINT_MAX ||
		    dp->d_off > (offset_t)MAXOFF_T) {
			error = EOVERFLOW;
			goto out;
		}
		op->d_ino = (ino_t)dp->d_ino;
		op->d_off = (off_t)dp->d_off;
		(void) strcpy(op->d_name, dp->d_name);
		op->d_reclen = DIRENT32_RECLEN(strlen(op->d_name));
		nsize += (u_int)dp->d_reclen;
		osize += (u_int)op->d_reclen;
		dp = (struct dirent64 *)((char *)dp + (u_int)dp->d_reclen);
		op = (struct dirent *)((char *)op + (u_int)op->d_reclen);
	}

	ASSERT(osize <= count);
	ASSERT((char *)op <= (char *)obuf + bufsize);
	ASSERT((char *)dp <= (char *)newbuf + bufsize);

	if ((error = copyout(obuf, buf, osize)) < 0)
		error = EFAULT;

out:
	kmem_free(newbuf, bufsize);
	kmem_free(obuf, bufsize);

	if (error) {
		RELEASEF(fd);
		return (set_errno(error));
	}

	RELEASEF(fd);
	return (osize);
}

int
getdents64(int fd, char *buf, int count)
{
	register vnode_t *vp;
	file_t *fp;
	struct uio auio;
	struct iovec aiov;
	register int error;
	int sink;

	if (count < sizeof (struct dirent64))
		return (set_errno(EINVAL));

	if ((fp = GETF(fd)) == NULL)
		return (set_errno(EBADF));
	vp = fp->f_vnode;
	if (vp->v_type != VDIR) {
		RELEASEF(fd);
		return (set_errno(ENOTDIR));
	}
	aiov.iov_base = buf;
	aiov.iov_len = count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_loffset = fp->f_offset;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_resid = count;
	auio.uio_fmode = 0;
	VOP_RWLOCK(vp, 0);
	error = VOP_READDIR(vp, &auio, fp->f_cred, &sink);
	VOP_RWUNLOCK(vp, 0);
	if (error) {
		RELEASEF(fd);
		return (set_errno(error));
	}
	count = count - auio.uio_resid;
	fp->f_offset = auio.uio_loffset;
	RELEASEF(fd);
	return (count);
}
