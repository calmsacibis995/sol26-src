/*
 *	Copyright (c) 1993-1996 by Sun Microsystems, Inc.
 *	All Rights Reserved.
 */

#pragma ident   "@(#)auto_vfsops.c 1.25     96/10/17 SMI"

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/kmem.h>
#include <sys/cred.h>
#include <sys/statvfs.h>
#include <sys/mount.h>
#include <sys/tiuser.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/mkdev.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/pathname.h>
#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <sys/dnlc.h>
#include <fs/fs_subr.h>
#include <sys/fs/autofs.h>
#include <rpcsvc/autofs_prot.h>
#include <sys/note.h>
#include <sys/modctl.h>

static int autofs_init(vfssw_t *, int);

int autofs_major;
int autofs_minor;
kmutex_t autofs_minor_lock;
kmutex_t fnnode_list_lock;
struct fnnode *fnnode_list;

static vfssw_t vfw = {
	"autofs",
	autofs_init,
	&auto_vfsops,
	0
};

/*
 * Module linkage information for the kernel.
 */
static struct modlfs modlfs = {
	&mod_fsops, "filesystem for autofs", &vfw
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlfs, NULL
};

/*
 * There are not enough stubs for rpcmod so we must force load it
 */
char _depends_on[] = "strmod/rpcmod";

/*
 * This is the module initialization routine.
 */
_init(void)
{
	return (mod_install(&modlinkage));
}

_fini(void)
{
	/*
	 * Don't allow the autofs module to be unloaded for now.
	 */
	return (EBUSY);
}

_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int autofs_fstype;

/*
 * autofs VFS operations
 */
static int auto_mount(vfs_t *, vnode_t *, struct mounta *, cred_t *);
static int auto_unmount(vfs_t *, cred_t *);
static int auto_root(vfs_t *, vnode_t **);
static int auto_statvfs(vfs_t *, statvfs64_t *);

struct vfsops auto_vfsops = {
	auto_mount,	/* mount */
	auto_unmount,	/* unmount */
	auto_root,	/* root */
	auto_statvfs,	/* statvfs */
	fs_sync,	/* sync */
	fs_nosys,	/* vget */
	fs_nosys,	/* mountroot */
	fs_nosys	/* swapvp */
};

static void
unmount_init(void)
{
	while (thread_create(NULL, NULL, auto_do_unmount,
	    NULL, 0, &p0, TS_RUN, 60) == NULL) {
		/*
		 * couldn't create unmount thread, most likely because
		 * we're low in memory, delay 20 seconds and try again.
		 */
		cmn_err(CE_WARN,
		    "autofs: unmount thread create failure - retrying\n");
		delay(20 * hz);
	}
}

int
autofs_init(
	vfssw_t *vswp,
	int fstype
)
{
	autofs_fstype = fstype;
	ASSERT(autofs_fstype != 0);
	/*
	 * Associate VFS ops vector with this fstype
	 */
	vswp->vsw_vfsops = &auto_vfsops;

	mutex_init(&autofs_minor_lock, "autofs minor lock",
		MUTEX_DEFAULT, NULL);
	mutex_init(&fnnode_count_lock, "autofs count lock",
		MUTEX_DEFAULT, NULL);
	mutex_init(&fnnode_list_lock, "autofs list lock",
		MUTEX_DEFAULT, NULL);
	fnnode_list = NULL;

	/*
	 * Assign unique major number for all autofs mounts
	 */
	if ((autofs_major = getudev()) == -1) {
		cmn_err(CE_WARN,
			"autofs: autofs_init: can't get unique device number");
		mutex_destroy(&autofs_minor_lock);
		mutex_destroy(&fnnode_count_lock);
		mutex_destroy(&fnnode_list_lock);
		return (1);
	}

	unmount_init();

	return (0);
}


/*
 * assumes vp is a VN_HELD vnode, it makes itself responsible for
 * VN_RELEasing it.
 */
int
auto_inkernel_mount(struct mounta *uap, vnode_t *vp)
{
	register struct vfs *vfsp;
	struct vfssw *vswp;
	struct vfsops *vfsops;
	register int error;
	int remount = 0, ovflags;

	/*
	 * The mountpoint must have been VN_HOLD already.
	 */

	if (vn_vfslock(vp)) {
		VN_RELE(vp);
		return (EBUSY);
	}
	if (vp->v_vfsmountedhere != NULL) {
		vn_vfsunlock(vp);
		VN_RELE(vp);
		return (EBUSY);
	}
	if (vp->v_flag & VNOMOUNT) {
		vn_vfsunlock(vp);
		VN_RELE(vp);
		return (EINVAL);
	}

	/*
	 * Backward compatibility: require the user program to
	 * supply a flag indicating a new-style mount, otherwise
	 * assume the fstype of the root file system and zero
	 * values for dataptr and datalen.  MS_FSS indicates an
	 * SVR3 4-argument mount; MS_DATA is the preferred way
	 * and indicates a 6-argument mount.
	 */
	if (uap->flags & (MS_DATA|MS_FSS)) {
		u_int n, fstype;
		char fsname[FSTYPSZ];

		/*
		 * Even funnier: we want a user-supplied fstype name here,
		 * but for backward compatibility we have to accept a
		 * number if one is provided.  The heuristic used is to
		 * assume that a "pointer" with a numeric value of less
		 * than 256 is really an int.
		 */
		if ((fstype = (u_int)uap->fstype) < 256) {
			if (fstype == 0 || fstype >= nfstype ||
			    !ALLOCATED_VFSSW(&vfssw[fstype])) {
				vn_vfsunlock(vp);
				VN_RELE(vp);
				return (EINVAL);
			}
			strcpy(fsname, vfssw[fstype].vsw_name);
		} else {
			if (uap->flags & MS_SYSSPACE)
				error = copystr(uap->fstype, fsname,
					FSTYPSZ, &n);
			else
				error = copyinstr(uap->fstype, fsname,
					FSTYPSZ, &n);
			if (error)  {
				if (error == ENAMETOOLONG)
					error = EINVAL;
				vn_vfsunlock(vp);
				VN_RELE(vp);
				return (error);
			}
		}
		if ((vswp = vfs_getvfssw(fsname)) == NULL) { /* Locks vfssw */
			vn_vfsunlock(vp);
			VN_RELE(vp);
			return (EINVAL);
		} else {
			vfsops = vswp->vsw_vfsops;
		}
	} else {
		vfsops = rootvfs->vfs_op;
		RLOCK_VFSSW();
	}

	/* vfssw was implicitly locked in vfs_getvfssw or explicitly here */
	ASSERT(VFSSW_LOCKED());

	if ((uap->flags & MS_DATA) == 0) {
		uap->dataptr = NULL;
		uap->datalen = 0;
	}

	/*
	 * If this is a remount we don't want to create a new VFS.
	 * Instead we pass the existing one with a remount flag.
	 */
	if (uap->flags & MS_REMOUNT) {
		remount = 1;
		/*
		 * Confirm that the vfsp associated with the mount point
		 * has already been mounted on.
		 */
		if ((vp->v_flag & VROOT) == 0) {
			vn_vfsunlock(vp);
			VN_RELE(vp);
			RUNLOCK_VFSSW();
			return (ENOENT);
		}
		/*
		 * Disallow making file systems read-only.  Ignore other flags.
		 */
		if (uap->flags & MS_RDONLY) {
			vn_vfsunlock(vp);
			VN_RELE(vp);
			RUNLOCK_VFSSW();
			return (EINVAL);
		}
		vfsp = vp->v_vfsp;
		ovflags = vfsp->vfs_flag;
		vfsp->vfs_flag |= VFS_REMOUNT;
		vfsp->vfs_flag &= ~VFS_RDONLY;

	} else {
		vfsp = kmem_alloc(sizeof (vfs_t), KM_SLEEP);
		VFS_INIT(vfsp, vfsops, (caddr_t) NULL);
	}

	/*
	 * Lock the vfs so that lookuppn() will not venture into the
	 * covered vnode's subtree.
	 */
	if (error = vfs_lock(vfsp)) {
		vn_vfsunlock(vp);
		VN_RELE(vp);
		if (!remount)
			kmem_free((caddr_t) vfsp, sizeof (struct vfs));
		RUNLOCK_VFSSW();
		return (error);
	}

	error = VFS_MOUNT(vfsp, vp, uap, kcred);
	if (error) {
		vfs_unlock(vfsp);
		if (remount)
			vfsp->vfs_flag = ovflags;
		else
			kmem_free((caddr_t) vfsp, sizeof (struct vfs));
		vn_vfsunlock(vp);
		VN_RELE(vp);
	} else {
		if (remount) {
			vfsp->vfs_flag &= ~VFS_REMOUNT;
			vn_vfsunlock(vp);
			VN_RELE(vp);
		} else {
			vfs_add(vp, vfsp, uap->flags);
			vp->v_vfsp->vfs_nsubmounts++;
			vn_vfsunlock(vp);
		}
		vfs_unlock(vfsp);
	}
	RUNLOCK_VFSSW();

	return (error);
}

/* ARGSUSED */
static int
auto_mount(
	vfs_t *vfsp,
	vnode_t *vp,
	struct mounta *uap,
	cred_t *cr
)
{
	int error = 0;
	int len = 0;
	struct autofs_args args;
	fninfo_t *fnip = NULL;
	vnode_t *rootvp = NULL;
	fnnode_t *rootfnp = NULL;
	char *data = uap->dataptr;
	char datalen = uap->datalen;
	dev_t autofs_dev;
	char strbuff[MAXPATHLEN+1];
	vnode_t *kvp;

	AUTOFS_DPRINT((4, "auto_mount: vfs %x vp %x\n", vfsp, vp));

	if (!suser(cr)) {
		return (EPERM);
	}

	/*
	 * Get arguments
	 */
	if (datalen != sizeof (args)) {
		return (EINVAL);
	}
	if (uap->flags & MS_SYSSPACE)
		error = kcopy(data, (caddr_t) &args, sizeof (args));
	else
		error = copyin(data, (caddr_t) &args, sizeof (args));
	if (error)
		return (EFAULT);

	/*
	 * For a remount, only update mount information
	 * i.e. default mount options, map name, etc.
	 */
	if (uap->flags & MS_REMOUNT) {
		fnip = vfstofni(vfsp);
		if (fnip == NULL)
			return (EINVAL);

		if (args.direct == 1)
			fnip->fi_flags |= MF_DIRECT;
		else
			fnip->fi_flags &= ~MF_DIRECT;
		fnip->fi_mount_to = args.mount_to;
		fnip->fi_rpc_to = args.rpc_to;

		/*
		 * Get default options
		 */
		if (uap->flags & MS_SYSSPACE)
			error = copystr(args.opts, strbuff, sizeof (strbuff),
				(u_int *) &len);
		else
			error = copyinstr(args.opts, strbuff, sizeof (strbuff),
				(u_int *) &len);
		if (error)
			return (EFAULT);

		kmem_free(fnip->fi_opts, fnip->fi_optslen);
		fnip->fi_opts = kmem_alloc(len, KM_SLEEP);
		fnip->fi_optslen = len;
		bcopy(strbuff, fnip->fi_opts, len);

		/*
		 * Get context/map name
		 */
		if (uap->flags & MS_SYSSPACE) {
			error = copystr(args.map, strbuff, sizeof (strbuff),
				(u_int *) &len);
		} else {
			error = copyinstr(args.map, strbuff, sizeof (strbuff),
				(u_int *) &len);
		}
		if (error)
			return (EFAULT);

		kmem_free(fnip->fi_map, fnip->fi_maplen);
		fnip->fi_map = kmem_alloc(len, KM_SLEEP);
		fnip->fi_maplen = len;
		bcopy(strbuff, fnip->fi_map, len);

		return (0);
	}

	/*
	 * Allocate fninfo struct and attach it to vfs
	 */
	fnip = (fninfo_t *) kmem_zalloc(sizeof (*fnip), KM_SLEEP);
	fnip->fi_mountvfs = vfsp;

	fnip->fi_mount_to = args.mount_to;
	fnip->fi_rpc_to = args.rpc_to;
	fnip->fi_refcnt = 0;
	vfsp->vfs_bsize = AUTOFS_BLOCKSIZE;
	vfsp->vfs_fstype = autofs_fstype;

	/*
	 * Assign a unique device id to the mount
	 */
	mutex_enter(&autofs_minor_lock);
	do {
		autofs_minor = (autofs_minor + 1) & MAXMIN;
		autofs_dev = makedevice(autofs_major, autofs_minor);
	} while (vfs_devsearch(autofs_dev));
	mutex_exit(&autofs_minor_lock);

	vfsp->vfs_dev = autofs_dev;
	vfsp->vfs_fsid.val[0] = autofs_dev;
	vfsp->vfs_fsid.val[1] = autofs_fstype;
	vfsp->vfs_data = (caddr_t) fnip;
	vfsp->vfs_bcount = 0;

	/*
	 * Get daemon address
	 */
	fnip->fi_addr.len = args.addr.len;
	fnip->fi_addr.maxlen = fnip->fi_addr.len;
	fnip->fi_addr.buf = (char *) kmem_alloc(args.addr.len, KM_SLEEP);
	if (uap->flags & MS_SYSSPACE)
		error = kcopy(args.addr.buf, fnip->fi_addr.buf, args.addr.len);
	else
		error = copyin(args.addr.buf, fnip->fi_addr.buf, args.addr.len);
	if (error) {
		error = EFAULT;
		goto errout;
	}

	/*
	 * Get path for mountpoint
	 */
	if (uap->flags & MS_SYSSPACE)
		error = copystr(args.path, strbuff, sizeof (strbuff),
			(u_int *) &len);
	else
		error = copyinstr(args.path, strbuff, sizeof (strbuff),
			(u_int *) &len);
	if (error) {
		error = EFAULT;
		goto errout;
	}
	fnip->fi_path = kmem_alloc(len, KM_SLEEP);
	fnip->fi_pathlen = len;
	bcopy(strbuff, fnip->fi_path, len);

	/*
	 * Get default options
	 */
	if (uap->flags & MS_SYSSPACE)
		error = copystr(args.opts, strbuff, sizeof (strbuff),
			(u_int *) &len);
	else
		error = copyinstr(args.opts, strbuff, sizeof (strbuff),
			(u_int *) &len);
	if (error) {
		error = EFAULT;
		goto errout;
	}
	fnip->fi_opts = kmem_alloc(len, KM_SLEEP);
	fnip->fi_optslen = len;
	bcopy(strbuff, fnip->fi_opts, len);

	/*
	 * Get context/map name
	 */
	if (uap->flags & MS_SYSSPACE)
		error = copystr(args.map, strbuff, sizeof (strbuff),
			(u_int *) &len);
	else
		error = copyinstr(args.map, strbuff, sizeof (strbuff),
			(u_int *) &len);
	if (error) {
		error = EFAULT;
		goto errout;
	}
	fnip->fi_map = kmem_alloc(len, KM_SLEEP);
	fnip->fi_maplen = len;
	bcopy(strbuff, fnip->fi_map, len);

	/*
	 * Get subdirectory within map
	 */
	if (uap->flags & MS_SYSSPACE)
		error = copystr(args.subdir, strbuff, sizeof (strbuff),
			(u_int *) &len);
	else
		error = copyinstr(args.subdir, strbuff, sizeof (strbuff),
			(u_int *) &len);
	if (error) {
		error = EFAULT;
		goto errout;
	}
	fnip->fi_subdir = kmem_alloc(len, KM_SLEEP);
	fnip->fi_subdirlen = len;
	bcopy(strbuff, fnip->fi_subdir, len);

	/*
	 * Get the key
	 */
	if (uap->flags & MS_SYSSPACE)
		error = copystr(args.key, strbuff, sizeof (strbuff),
			(u_int *) &len);
	else
		error = copyinstr(args.key, strbuff, sizeof (strbuff),
			(u_int *) &len);
	if (error) {
		error = EFAULT;
		goto errout;
	}
	fnip->fi_key = kmem_alloc(len, KM_SLEEP);
	fnip->fi_keylen = len;
	bcopy(strbuff, fnip->fi_key, len);


	/*
	 * Is this a direct mount?
	 */
	if (args.direct == 1) {
		fnip->fi_flags |= MF_DIRECT;
	}

	/*
	 * Setup netconfig.
	 * Can I pass in knconf as mount argument? what
	 * happens when the daemon gets restarted?
	 */
	if ((error = lookupname("/dev/ticotsord", UIO_SYSSPACE, FOLLOW,
	    NULLVPP, &kvp)) != 0) {
		cmn_err(CE_WARN, "autofs: lookupname: %d\n", error);
		goto errout;
	}

	fnip->fi_knconf.knc_rdev = kvp->v_rdev;
	fnip->fi_knconf.knc_protofmly = NC_LOOPBACK;
	fnip->fi_knconf.knc_semantics = NC_TPI_COTS_ORD;
	VN_RELE(kvp);

	/*
	 * Make the root vnode
	 */
	if ((rootfnp =
	    auto_makefnnode(VDIR, vfsp, fnip->fi_path, cr)) == NULL) {
		error = ENOMEM;
		goto errout;
	}
	rootvp = fntovn(rootfnp);
	VN_HOLD(rootvp);

	rootvp->v_flag |= VROOT;
	rootfnp->fn_mode = AUTOFS_MODE;
	rootfnp->fn_parent = rootfnp;
	/* account for ".." entry */
	rootfnp->fn_linkcnt = rootfnp->fn_size = 1;
	fnip->fi_rootvp = rootvp;

	/*
	 * Add to list of top level AUTOFS' if it is being mounted by
	 * a user level process.
	 */
	if ((uap->flags & MS_SYSSPACE) == 0) {
		mutex_enter(&fnnode_list_lock);
		if (fnnode_list == NULL)
			fnnode_list = rootfnp;
		else {
			rootfnp->fn_next = fnnode_list;
			fnnode_list = rootfnp;
		}
		mutex_exit(&fnnode_list_lock);
	}

	AUTOFS_DPRINT((5, "auto_mount: vfs %x root %x fnip %x return %d\n",
		vfsp, rootvp, fnip, error));

	return (0);

errout:
	ASSERT(fnip != NULL);
	ASSERT((uap->flags & MS_REMOUNT) == 0);

	if (fnip->fi_addr.buf != NULL)
		kmem_free(fnip->fi_addr.buf, fnip->fi_addr.len);
	if (fnip->fi_path != NULL)
		kmem_free(fnip->fi_path, fnip->fi_pathlen);
	if (fnip->fi_opts != NULL)
		kmem_free(fnip->fi_opts, fnip->fi_optslen);
	if (fnip->fi_map != NULL)
		kmem_free(fnip->fi_map, fnip->fi_maplen);
	if (fnip->fi_subdir != NULL)
		kmem_free(fnip->fi_subdir, fnip->fi_subdirlen);
	if (fnip->fi_key != NULL)
		kmem_free(fnip->fi_key, fnip->fi_keylen);
	kmem_free(fnip, sizeof (*fnip));

	AUTOFS_DPRINT((5, "auto_mount: vfs %x root %x fnip %x return %d\n",
		vfsp, rootvp, fnip, error));

	return (error);
}

/* ARGSUSED */
static int
auto_unmount(
	vfs_t *vfsp,
	cred_t *cr
)
{
	fninfo_t *fnip;
	vnode_t *rvp;
	fnnode_t *rfnp, *fnp, **fnpp;

	fnip = vfstofni(vfsp);
	AUTOFS_DPRINT((4, "auto_unmount vfsp %x fnip %x\n", vfsp, fnip));

	if (!suser(cr))
		return (EPERM);

	ASSERT(vfsp->vfs_vnodecovered->v_flag & VVFSLOCK);
	rvp = fnip->fi_rootvp;
	rfnp = vntofn(rvp);

	if (rvp->v_count > 1)
		return (EBUSY);

	if (rfnp->fn_dirents != NULL)
		return (EBUSY);

	/*
	 * The root vnode is on the linked list of root fnnodes
	 * only if this was not a trigger node. Since we have no way
	 * of knowing, if we don't find it, then we assume it was a
	 * trigger node.
	 */
	mutex_enter(&fnnode_list_lock);
	fnpp = &fnnode_list;
	for (;;) {
		fnp = *fnpp;
		if (fnp == NULL)
			/*
			 * Must be a trigger node.
			 */
			break;
		if (fnp == rfnp) {
			*fnpp = fnp->fn_next;
			fnp->fn_next = NULL;
			break;
		}
		fnpp = &fnp->fn_next;
	}
	mutex_exit(&fnnode_list_lock);

	ASSERT(rvp->v_count == 1);
	ASSERT(rfnp->fn_size == 1);
	ASSERT(rfnp->fn_linkcnt == 1);
	/*
	 * the following drops linkcnt to 0, therefore
	 * the disconnect is not attempted when auto_inactive() is
	 * called by vn_rele(). This is necessary
	 * because we have nothing to get disconnected from
	 * since we're the root of the filesystem. As a side effect
	 * the node is not freed, therefore I should free the node
	 * here. I really need to think of a better way of doing this.
	 */
	rfnp->fn_size--;
	rfnp->fn_linkcnt--;

	/*
	 * release of last reference causes node
	 * to be freed
	 */
	VN_RELE(rvp);
	rfnp->fn_parent = NULL;

	auto_freefnnode(rfnp);

	kmem_free(fnip->fi_addr.buf, fnip->fi_addr.len);
	kmem_free(fnip->fi_path, fnip->fi_pathlen);
	kmem_free(fnip->fi_map, fnip->fi_maplen);
	kmem_free(fnip->fi_subdir, fnip->fi_subdirlen);
	kmem_free(fnip->fi_key, fnip->fi_keylen);
	kmem_free(fnip->fi_opts, fnip->fi_optslen);
	kmem_free(fnip, sizeof (*fnip));
	AUTOFS_DPRINT((5, "auto_unmount: return=0\n"));

	return (0);
}


/*
 * find root of autofs
 */
static int
auto_root(
	vfs_t *vfsp,
	vnode_t **vpp
)
{
	*vpp = (vnode_t *) vfstofni(vfsp)->fi_rootvp;
	VN_HOLD(*vpp);

	AUTOFS_DPRINT((5, "auto_root: vfs %x, *vpp %x\n", vfsp, *vpp));
	return (0);
}

/*
 * Get file system statistics.
 */
static int
auto_statvfs(
	register vfs_t *vfsp,
	statvfs64_t *sbp
)
{
	AUTOFS_DPRINT((4, "auto_statvfs %x\n", vfsp));

	bzero((caddr_t)sbp, (int)sizeof (*sbp));
	sbp->f_bsize	= vfsp->vfs_bsize;
	sbp->f_frsize	= sbp->f_bsize;
	sbp->f_blocks	= (fsblkcnt64_t)0;
	sbp->f_bfree	= (fsblkcnt64_t)0;
	sbp->f_bavail	= (fsblkcnt64_t)0;
	sbp->f_files	= (fsfilcnt64_t)0;
	sbp->f_ffree	= (fsfilcnt64_t)0;
	sbp->f_favail	= (fsfilcnt64_t)0;
	sbp->f_fsid	= vfsp->vfs_dev;
	strcpy(sbp->f_basetype, vfssw[vfsp->vfs_fstype].vsw_name);
	sbp->f_flag = vf_to_stf(vfsp->vfs_flag);
	sbp->f_namemax = MAXNAMELEN;
	strcpy(sbp->f_fstr, MNTTYPE_AUTOFS);

	return (0);
}
