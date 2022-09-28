/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1996 Sun Microsystems, Inc.
 * All rights reserved.
 */
#ident	"@(#)prvfsops.c 1.30	96/06/18 SMI"	/* SVr4.0 1.25  */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/procfs.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/mode.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/mount.h>
#include <sys/bitmap.h>
#include <fs/fs_subr.h>
#include <fs/proc/prdata.h>

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

extern struct vfsops prvfsops;
static int prinit();

static struct vfssw vfw = {
	"proc",
	prinit,
	&prvfsops,
	0
};

/*
 * Module linkage information for the kernel.
 */
extern struct mod_ops mod_fsops;

static struct modlfs modlfs = {
	&mod_fsops, "filesystem for proc", &vfw
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlfs, NULL
};

_init()
{
	return (mod_install(&modlinkage));
}

_info(modinfop)
	struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * N.B.
 * No _fini routine. The module cannot be unloaded once loaded.
 * The NO_UNLOAD_STUB in modstubs.s must change if this module
 * is ever modified to become unloadable.
 */

int		nproc_highbit;		/* highbit(v.v_nproc) */
int		procfstype;
dev_t		procdev;
struct vfs	*procvfs;		/* Points to /proc vfs entry. */
int		prmounted;		/* Set to 1 if /proc is mounted. */
struct prnode	prrootnode;
kmutex_t	pr_mount_lock;

/*
 * /proc VFS operations vector.
 */
static int	prmount(), prunmount(), prroot(), prstatvfs();

struct vfsops prvfsops = {
	prmount,
	prunmount,
	prroot,
	prstatvfs,
	fs_sync,
	fs_nosys,	/* vget */
	fs_nosys,	/* mountroot */
	fs_nosys	/* swapvp */
};

static void
prinitrootnode(pnp)
	register prnode_t *pnp;
{
	register struct vnode *vp = PTOV(pnp);

	bzero((caddr_t)pnp, sizeof (*pnp));
	mutex_init(&pnp->pr_mutex, "prrootnode mutex", MUTEX_DEFAULT,
	    DEFAULT_WT);
	mutex_init(&vp->v_lock, "prrootnode v_lock", MUTEX_DEFAULT,
	    DEFAULT_WT);
	vp->v_flag = VROOT|VNOCACHE|VNOMAP|VNOSWAP|VNOMOUNT;
	vp->v_count = 1;
	vp->v_op = &prvnodeops;
	vp->v_type = VDIR;
	vp->v_data = (caddr_t)pnp;
	cv_init(&vp->v_cv, "prrootnode v_cv", CV_DEFAULT, NULL);
	pnp->pr_mode = 0555;	/* read-search by everyone */
}

static int
prinit(vswp, fstype)
	register struct vfssw *vswp;
	int fstype;
{
	register int dev;

	nproc_highbit = highbit(v.v_proc);
	procfstype = fstype;
	ASSERT(procfstype != 0);
	/*
	 * Associate VFS ops vector with this fstype.
	 */
	vswp->vsw_vfsops = &prvfsops;

	/*
	 * Assign a unique "device" number (used by stat(2)).
	 */
	if ((dev = getudev()) == -1) {
		cmn_err(CE_WARN, "prinit: can't get unique device number");
		dev = 0;
	}
	procdev = makedevice(dev, 0);
	prmounted = 0;
	mutex_init(&pr_mount_lock, "procfs mount lock",
				MUTEX_DEFAULT, DEFAULT_WT);
	prinitrootnode(&prrootnode);
	prrootnode.pr_type = PR_PROCDIR;

	return (0);
}

/* ARGSUSED */
static int
prmount(vfsp, mvp, uap, cr)
	struct vfs *vfsp;
	struct vnode *mvp;
	struct mounta *uap;
	struct cred *cr;
{
	register struct prnode *pnp;
	register int error = 0;

	if (!suser(cr))
		return (EPERM);
	if (mvp->v_type != VDIR)
		return (ENOTDIR);
	mutex_enter(&pr_mount_lock);

	mutex_enter(&mvp->v_lock);
	if ((uap->flags & MS_OVERLAY) == 0 &&
	    (mvp->v_count > 1 || (mvp->v_flag & VROOT))) {
		mutex_exit(&mvp->v_lock);
		error = EBUSY;
		goto out;
	}
	mutex_exit(&mvp->v_lock);
	/*
	 * Prevent duplicate mount.
	 */
	if (prmounted) {
		error = EBUSY;
		goto out;
	}

	pnp = &prrootnode;
	PTOV(pnp)->v_vfsp = vfsp;
	vfsp->vfs_fstype = procfstype;
	vfsp->vfs_data = (caddr_t)pnp;
	vfsp->vfs_dev = procdev;
	vfsp->vfs_fsid.val[0] = procdev;
	vfsp->vfs_fsid.val[1] = procfstype;
	vfsp->vfs_bsize = DEV_BSIZE;
	procvfs = vfsp;
	prmounted = 1;

out:
	mutex_exit(&pr_mount_lock);
	return (error);
}

/* ARGSUSED */
static int
prunmount(vfsp, cr)
	struct vfs *vfsp;
	struct cred *cr;
{
	register prnode_t *pnp = (prnode_t *)vfsp->vfs_data;
	register vnode_t *vp = PTOV(&prrootnode);

	ASSERT(pnp == &prrootnode);

	mutex_enter(&pr_mount_lock);
	if (!suser(cr) || !prmounted) {
		mutex_exit(&pr_mount_lock);
		return (EPERM);
	}

	/*
	 * Ensure that no /proc vnodes are in use.
	 */
	mutex_enter(&vp->v_lock);
	if (vp->v_count > 1) {
		mutex_exit(&vp->v_lock);
		mutex_exit(&pr_mount_lock);
		return (EBUSY);
	}

	PTOV(pnp)->v_vfsp = NULL;
	prmounted = 0;
	procvfs = NULL;
	mutex_exit(&vp->v_lock);
	mutex_exit(&pr_mount_lock);
	return (0);
}

/* ARGSUSED */
static int
prroot(vfsp, vpp)
	struct vfs *vfsp;
	struct vnode **vpp;
{
	register prnode_t *pnp = (prnode_t *)vfsp->vfs_data;
	register struct vnode *vp = PTOV(pnp);

	VN_HOLD(vp);
	*vpp = vp;
	return (0);
}

static int
prstatvfs(vfsp, sp)
	struct vfs *vfsp;
	register struct statvfs64 *sp;
{
	register int i, n;

	mutex_enter(&pidlock);
	for (n = v.v_proc, i = 0; i < v.v_proc; i++)
		if (pid_entry(i) != NULL)
			n--;
	mutex_exit(&pidlock);

	bzero((caddr_t)sp, sizeof (*sp));
	sp->f_bsize	= DEV_BSIZE;
	sp->f_frsize	= DEV_BSIZE;
	sp->f_blocks	= (fsblkcnt64_t)0;
	sp->f_bfree	= (fsblkcnt64_t)0;
	sp->f_bavail	= (fsblkcnt64_t)0;
	sp->f_files	= (fsfilcnt64_t)v.v_proc + 2;
	sp->f_ffree	= (fsfilcnt64_t)n;
	sp->f_favail	= (fsfilcnt64_t)n;
	sp->f_fsid	= vfsp->vfs_dev;
	strcpy(sp->f_basetype, vfssw[procfstype].vsw_name);
	sp->f_flag = vf_to_stf(vfsp->vfs_flag);
	sp->f_namemax = PNSIZ;
	strcpy(sp->f_fstr, "/proc");
	strcpy(&sp->f_fstr[6], "/proc");
	return (0);
}
