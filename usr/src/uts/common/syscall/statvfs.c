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
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988,1989,1993,1994, 1996  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

#pragma ident	"@(#)statvfs.c	1.7	96/04/19 SMI"	/* SVr4 1.42	*/

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/fstyp.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/statvfs.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/pathname.h>

#include <vm/page.h>

/*
 * Get file system statistics (statvfs and fstatvfs).
 */

static int	cstatvfs(struct vfs *, struct statvfs *);
static int	cstatvfs64(struct vfs *, struct statvfs64 *);

int
statvfs(char *fname, struct statvfs *sbp)
{
	vnode_t *vp;
	register int error;

	if (error = lookupname(fname, UIO_USERSPACE, FOLLOW, NULLVPP, &vp))
		return (set_errno(error));
	error = cstatvfs(vp->v_vfsp, sbp);
	VN_RELE(vp);
	return (error);
}

int
fstatvfs(int fdes, struct statvfs *sbp)
{
	struct file *fp;
	register int error;

	if ((fp = GETF(fdes)) == NULL)
		return (set_errno(EBADF));
	error  = cstatvfs(fp->f_vnode->v_vfsp, sbp);
	RELEASEF(fdes);
	return (error);
}

int
statvfs64(char *fname, struct statvfs64 *sbp)
{
	vnode_t *vp;
	register int error;

	if (error = lookupname(fname, UIO_USERSPACE, FOLLOW, NULLVPP, &vp))
		return (set_errno(error));
	error = cstatvfs64(vp->v_vfsp, sbp);
	VN_RELE(vp);
	return (error);

}

int
fstatvfs64(int fdes, struct statvfs64 *sbp)
{
	struct file *fp;
	register int error;

	if ((fp = GETF(fdes)) == NULL)
		return (set_errno(EBADF));
	error  = cstatvfs64(fp->f_vnode->v_vfsp, sbp);
	RELEASEF(fdes);
	return (error);
}

/*
 * Common routine for statvfs and fstatvfs.
 */
static int
cstatvfs(register struct vfs *vfsp, struct statvfs *ubp)
{
	struct statvfs64 lf_ds;
	struct statvfs ds;
	register int error;

	struct_zero((caddr_t)&lf_ds, sizeof (lf_ds));
	if (error = VFS_STATVFS(vfsp, &lf_ds))
		return (set_errno(error));

	/*
	 * Large Files: The following check for -1 is due to some file systems
	 * returning -1 in the fields that are irrelevant or nonessential
	 * and we do not want to return EOVERFLOW for this. NFS returns
	 * -1 for some of these fields. For example df is expected to
	 * show -1 in their output for nfs mounted systems.
	 */

	if (lf_ds.f_files == (fsfilcnt64_t)-1)
		lf_ds.f_files = UINT_MAX;
	if (lf_ds.f_ffree == (fsfilcnt64_t)-1)
		lf_ds.f_ffree = UINT_MAX;
	if (lf_ds.f_favail == (fsfilcnt64_t)-1)
		lf_ds.f_favail = UINT_MAX;
	if (lf_ds.f_bavail == (fsblkcnt64_t)-1)
		lf_ds.f_bavail = UINT_MAX;
	if (lf_ds.f_bfree == (fsblkcnt64_t)-1)
		lf_ds.f_bfree = UINT_MAX;

	if (lf_ds.f_blocks > UINT_MAX || lf_ds.f_bfree > UINT_MAX ||
		lf_ds.f_bavail > UINT_MAX || lf_ds.f_files > UINT_MAX ||
		lf_ds.f_ffree > UINT_MAX || lf_ds.f_favail > UINT_MAX)
			return (set_errno(EOVERFLOW));
	else {
		struct_zero((caddr_t)&ds, sizeof (ds));
		ds.f_bsize = lf_ds.f_bsize;
		ds.f_frsize = lf_ds.f_frsize;
		ds.f_blocks = lf_ds.f_blocks;
		ds.f_bfree = lf_ds.f_bfree;
		ds.f_bavail = lf_ds.f_bavail;
		ds.f_files = lf_ds.f_files;
		ds.f_ffree = lf_ds.f_ffree;
		ds.f_favail = lf_ds.f_favail;
		ds.f_fsid = lf_ds.f_fsid;
		strcpy(ds.f_basetype, lf_ds.f_basetype);
		ds.f_flag = lf_ds.f_flag;
		ds.f_namemax = lf_ds.f_namemax;
		strcpy(ds.f_fstr, lf_ds.f_fstr);

		if (copyout((caddr_t)&ds, (caddr_t)ubp, sizeof (ds)))
			return (set_errno(EFAULT));
	}
	return (0);
}

static int
cstatvfs64(register struct vfs *vfsp, struct statvfs64 *ubp)
{
	struct statvfs64 lf_ds;
	register int error;

	struct_zero((caddr_t)&lf_ds, sizeof (lf_ds));
	if (error = VFS_STATVFS(vfsp, &lf_ds))
		return (set_errno(error));
	if (copyout((caddr_t)&lf_ds, (caddr_t)ubp, sizeof (lf_ds)))
		return (set_errno(EFAULT));
	return (0);
}
