/*
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's Unix(r) System V.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	Copyright (c) 1986-1991,1994-1996 by Sun Microsystems, Inc.
 *	Copyright (c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	All rights reserved.
 */

#pragma ident	"@(#)nfs_vfsops.c	1.79	96/10/17 SMI"
/* SVr4.0 1.16 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/pathname.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/kstat.h>
#include <sys/mkdev.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/utsname.h>
#include <sys/bootconf.h>
#include <sys/modctl.h>
#include <sys/acl.h>
#include <sys/flock.h>

#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>

#include <nfs/nfs.h>
#include <nfs/nfs_clnt.h>
#include <nfs/rnode.h>
#include <nfs/mount.h>
#include <nfs/nfs_acl.h>

#include <fs/fs_subr.h>

/*
 * from rpcsec module (common/rpcsec).
 */
extern long sec_clnt_loadinfo(struct sec_data *, struct sec_data **);
extern void sec_clnt_freeinfo(struct sec_data *);

static int	pathconf_get(struct mntinfo *, struct nfs_args *);
static void	pathconf_rele(struct mntinfo *);

kstat_t *nfs_client_kstat;
kstat_t *nfs_server_kstat;

static kstat_named_t rfsreqcnt_v2[] = {
	{ "null",	KSTAT_DATA_ULONG },
	{ "getattr",	KSTAT_DATA_ULONG },
	{ "setattr",	KSTAT_DATA_ULONG },
	{ "root",	KSTAT_DATA_ULONG },
	{ "lookup",	KSTAT_DATA_ULONG },
	{ "readlink",	KSTAT_DATA_ULONG },
	{ "read",	KSTAT_DATA_ULONG },
	{ "wrcache",	KSTAT_DATA_ULONG },
	{ "write",	KSTAT_DATA_ULONG },
	{ "create",	KSTAT_DATA_ULONG },
	{ "remove",	KSTAT_DATA_ULONG },
	{ "rename",	KSTAT_DATA_ULONG },
	{ "link",	KSTAT_DATA_ULONG },
	{ "symlink",	KSTAT_DATA_ULONG },
	{ "mkdir",	KSTAT_DATA_ULONG },
	{ "rmdir",	KSTAT_DATA_ULONG },
	{ "readdir",	KSTAT_DATA_ULONG },
	{ "statfs",	KSTAT_DATA_ULONG }
};
static kstat_named_t *rfsreqcnt_v2_ptr = rfsreqcnt_v2;
static ulong_t rfsreqcnt_v2_ndata = sizeof (rfsreqcnt_v2) /
					sizeof (kstat_named_t);

static char *rfsnames_v2[] = {
	"null", "getattr", "setattr", "unused", "lookup", "readlink", "read",
	"unused", "write", "create", "remove", "rename", "link", "symlink",
	"mkdir", "rmdir", "readdir", "fsstat"
};

/*
 * This table maps from NFS protocol number into call type.
 * Zero means a "Lookup" type call
 * One  means a "Read" type call
 * Two  means a "Write" type call
 * This is used to select a default time-out.
 */
static char call_type_v2[] = {
	0, 0, 1, 0, 0, 0, 1,
	0, 2, 2, 2, 2, 2, 2,
	2, 2, 1, 0 };

/*
 * Similar table, but to determine which timer to use
 * (only real reads and writes!)
 */
static char timer_type_v2[] = {
	0, 0, 0, 0, 0, 0, 1,
	0, 2, 0, 0, 0, 0, 0,
	0, 0, 1, 0 };

/*
 * This table maps from NFS protocol number into a call type
 * for the semisoft mount option.
 * Zero means do not repeat operation.
 * One  means repeat.
 */
static char ss_call_type_v2[] = {
	0, 0, 1, 0, 0, 0, 0,
	0, 1, 1, 1, 1, 1, 1,
	1, 1, 0, 0 };

/*
 * nfs vfs operations.
 */
static	int	nfs_mount(vfs_t *, vnode_t *, struct mounta *, cred_t *);
static	int	nfs_unmount(vfs_t *, cred_t *);
static	int	nfs_root(vfs_t *, vnode_t **);
static	int	nfs_statvfs(vfs_t *, struct statvfs64 *);
static	int	nfs_sync(vfs_t *, short, cred_t *);
static	int	nfs_vget(vfs_t *, vnode_t **, fid_t *);
static	int	nfs_mountroot(vfs_t *, whymountroot_t);

struct vfsops nfs_vfsops = {
	nfs_mount,
	nfs_unmount,
	nfs_root,
	nfs_statvfs,
	nfs_sync,
	nfs_vget,
	nfs_mountroot,
	fs_nosys
};

vnode_t nfs_notfound;

/*
 * Initialize the vfs structure
 */

int nfsfstyp;

int
nfsinit(struct vfssw *vswp, int fstyp)
{
	vnode_t *vp;
	kstat_t *rfsproccnt_v2_kstat;
	kstat_t *rfsreqcnt_v2_kstat;
	kstat_t *aclproccnt_v2_kstat;
	kstat_t *aclreqcnt_v2_kstat;

	vswp->vsw_vfsops = &nfs_vfsops;
	nfsfstyp = fstyp;

	mutex_enter(&nfs_kstat_lock);
	if (nfs_client_kstat == NULL) {
		if ((nfs_client_kstat = kstat_create("nfs", 0, "nfs_client",
		    "misc", KSTAT_TYPE_NAMED, clstat_ndata,
		    KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE)) != NULL) {
			nfs_client_kstat->ks_data = (void *)clstat_ptr;
			kstat_install(nfs_client_kstat);
		}
	}

	if (nfs_server_kstat == NULL) {
		if ((nfs_server_kstat = kstat_create("nfs", 0, "nfs_server",
		    "misc", KSTAT_TYPE_NAMED, svstat_ndata,
		    KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE)) != NULL) {
			nfs_server_kstat->ks_data = (void *)svstat_ptr;
			kstat_install(nfs_server_kstat);
		}
	}
	mutex_exit(&nfs_kstat_lock);

	if ((rfsproccnt_v2_kstat = kstat_create("nfs", 0, "rfsproccnt_v2",
	    "misc", KSTAT_TYPE_NAMED, rfsproccnt_v2_ndata,
	    KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE)) != NULL) {
		rfsproccnt_v2_kstat->ks_data = (void *)rfsproccnt_v2_ptr;
		kstat_install(rfsproccnt_v2_kstat);
	}
	if ((rfsreqcnt_v2_kstat = kstat_create("nfs", 0, "rfsreqcnt_v2",
	    "misc", KSTAT_TYPE_NAMED, rfsreqcnt_v2_ndata,
	    KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE)) != NULL) {
		rfsreqcnt_v2_kstat->ks_data = (void *)rfsreqcnt_v2_ptr;
		kstat_install(rfsreqcnt_v2_kstat);
	}
	if ((aclproccnt_v2_kstat = kstat_create("nfs_acl", 0, "aclproccnt_v2",
	    "misc", KSTAT_TYPE_NAMED, aclproccnt_v2_ndata,
	    KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE)) != NULL) {
		aclproccnt_v2_kstat->ks_data = (void *)aclproccnt_v2_ptr;
		kstat_install(aclproccnt_v2_kstat);
	}
	if ((aclreqcnt_v2_kstat = kstat_create("nfs_acl", 0, "aclreqcnt_v2",
	    "misc", KSTAT_TYPE_NAMED, aclreqcnt_v2_ndata,
	    KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_WRITABLE)) != NULL) {
		aclreqcnt_v2_kstat->ks_data = (void *)aclreqcnt_v2_ptr;
		kstat_install(aclreqcnt_v2_kstat);
	}

	vp = &nfs_notfound;
	bzero((caddr_t)vp, sizeof (*vp));
	mutex_init(&vp->v_lock, "rnode v_lock", MUTEX_DEFAULT, DEFAULT_WT);
	cv_init(&vp->v_cv, "rnode v_cv", CV_DEFAULT, NULL);
	vp->v_count = 1;
	vp->v_op = &nfs_vnodeops;

	return (0);
}

/*
 * nfs mount vfsop
 * Set up mount info record and attach it to vfs struct.
 */
static int
nfs_mount(vfs_t *vfsp, vnode_t *mvp, struct mounta *uap, cred_t *cr)
{
	char *data = uap->dataptr;
	int error;
	vnode_t *rtvp;			/* the server's root */
	mntinfo_t *mi;			/* mount info, pointed at by vfs */
	struct nfs_args args;		/* nfs mount arguments */
	int hlen;			/* length of hostname */
	char shostname[HOSTNAMESZ];	/* server's hostname */
	int nlen;			/* length of netname */
	char netname[MAXNETNAMELEN+1];	/* server's netname */
	struct netbuf addr;		/* server's address */
	struct netbuf syncaddr;		/* AUTH_DES time sync addr */
	struct knetconfig *knconf;	/* transport knetconfig structure */
	rnode_t *rp;
	struct servinfo *svp;		/* nfs server info */
	struct servinfo *svp_prev = NULL; /* previous nfs server info */
	struct servinfo *svp_head;	/* first nfs server info */
	struct sec_data *secdata;	/* security data */

	if (!suser(cr))
		return (EPERM);

	if (mvp->v_type != VDIR)
		return (ENOTDIR);

	/*
	 * get arguments
	 *
	 * nfs_args is now versioned and is extensible, so
	 * uap->datalen might be different from sizeof (args)
	 * in a compatible situation.
	 */
more:
	bzero((caddr_t)&args, sizeof (args));
	if (copyin(data, (caddr_t)&args, MIN(uap->datalen, sizeof (args))))
		return (EFAULT);

	/*
	 * For now, only check the llock flag on remount.
	 * Remounts need to save the pathconf information.
	 * Part of the infamous static kludge.
	 */
	if (uap->flags & MS_REMOUNT) {
		if ((mi = VFTOMI(vfsp)) != NULL) {
			u_int new_mi_llock;
			u_int old_mi_llock;

			new_mi_llock = (args.flags & NFSMNT_LLOCK) ? 1 : 0;
			mutex_enter(&mi->mi_lock);
			old_mi_llock = (mi->mi_flags & MI_LLOCK) ? 1 : 0;
			if (old_mi_llock != new_mi_llock) {
				if (flk_vfs_has_locks(vfsp)) {
					mutex_exit(&mi->mi_lock);
					return (EBUSY);
				}
				if (new_mi_llock)
					mi->mi_flags |= MI_LLOCK;
				else
					mi->mi_flags &= ~MI_LLOCK;
			}
			mutex_exit(&mi->mi_lock);
		}
		return (pathconf_get((struct mntinfo *)vfsp->vfs_data, &args));
	}

	mutex_enter(&mvp->v_lock);
	if (!(uap->flags & MS_OVERLAY) &&
	    (mvp->v_count != 1 || (mvp->v_flag & VROOT))) {
		mutex_exit(&mvp->v_lock);
		return (EBUSY);
	}
	mutex_exit(&mvp->v_lock);

	/* make sure things are zeroed for errout: */
	rtvp = NULL;
	mi = NULL;
	addr.buf = NULL;
	syncaddr.buf = NULL;
	secdata = NULL;

	/*
	 * A valid knetconfig structure is required.
	 */
	if (!(args.flags & NFSMNT_KNCONF))
		return (EINVAL);

	/*
	 * Allocate a servinfo struct.
	 */
	svp = (struct servinfo *) kmem_zalloc(sizeof (*svp), KM_SLEEP);
	if (svp_prev)
		svp_prev->sv_next = svp;
	else
		svp_head = svp;
	svp_prev = svp;

	/*
	 * Allocate space for a knetconfig structure and
	 * its strings and copy in from user-land.
	 */
	knconf = (struct knetconfig *) kmem_alloc(sizeof (*knconf), KM_SLEEP);
	svp->sv_knconf = knconf;
	if (copyin((caddr_t)args.knconf, (caddr_t)knconf, sizeof (*knconf))) {
		kmem_free((caddr_t)knconf, sizeof (*knconf));
		return (EFAULT);
	} else {
		size_t nmoved_tmp;
		char *p, *pf;

		pf = (char *) kmem_alloc(KNC_STRSIZE, KM_SLEEP);
		p = (char *) kmem_alloc(KNC_STRSIZE, KM_SLEEP);
		error = copyinstr((caddr_t)knconf->knc_protofmly, pf,
				KNC_STRSIZE, &nmoved_tmp);
		if (!error) {
			error = copyinstr((caddr_t)knconf->knc_proto,
					p, KNC_STRSIZE, &nmoved_tmp);
			if (!error) {
				knconf->knc_protofmly = pf;
				knconf->knc_proto = p;
			} else {
				kmem_free((caddr_t)pf, KNC_STRSIZE);
				kmem_free((caddr_t)p, KNC_STRSIZE);
				kmem_free((caddr_t)knconf, sizeof (*knconf));
				return (error);
			}
		} else {
			kmem_free((caddr_t)pf, KNC_STRSIZE);
			kmem_free((caddr_t)p, KNC_STRSIZE);
			kmem_free((caddr_t)knconf, sizeof (*knconf));
			return (error);
		}
	}

	/*
	 * Get server address
	 */
	if (copyin((caddr_t)args.addr, (caddr_t)&addr, sizeof (addr))) {
		addr.buf = NULL;
		error = EFAULT;
	} else {
		char *userbufptr = addr.buf;

		addr.buf = (char *) kmem_alloc(addr.len, KM_SLEEP);
		addr.maxlen = addr.len;
		if (copyin(userbufptr, addr.buf, addr.len))
			error = EFAULT;
	}
	if (error)
		goto errout;

	svp->sv_addr = addr;

	/*
	 * Get the root fhandle
	 */
	if (copyin((caddr_t)args.fh, (caddr_t)&(svp->sv_fhandle.fh_buf),
		NFS_FHSIZE)) {
		error = EFAULT;
		goto errout;
	}
	svp->sv_fhandle.fh_len = NFS_FHSIZE;

	/*
	 * Get server's hostname
	 */
	if (args.flags & NFSMNT_HOSTNAME) {
		error = copyinstr(args.hostname, shostname,
			sizeof (shostname), (u_int *)&hlen);
		if (error)
			goto errout;
		shostname[hlen] = '\0';
	} else {
		char *p = "unknown-host";
		hlen = strlen(p);
		(void) strncpy(shostname, p, hlen+1);
	}
	svp->sv_hostnamelen = hlen+1;
	svp->sv_hostname = (char *) kmem_alloc(svp->sv_hostnamelen, KM_SLEEP);
	strcpy(svp->sv_hostname, shostname);

	/*
	 *  Get the extention data which has the new security data structure.
	 */
	if (args.flags & NFSMNT_NEWARGS) {

		switch (args.nfs_args_ext) {
		case NFS_ARGS_EXT1:
		case NFS_ARGS_EXT2:
			/*
			 *  Indicating the application is using the new sec_data
			 *  structure to pass in the security data.
			 */
			if (args.nfs_ext_u.nfs_ext1.secdata ==
					(struct sec_data *) NULL)
				error = EINVAL;
			else {
				error = sec_clnt_loadinfo(
				    args.nfs_ext_u.nfs_ext1.secdata, &secdata);
			}
			break;

		default:
			error = EINVAL;
			break;
		}

	} else if (args.flags & (NFSMNT_SECURE | NFSMNT_KERBEROS)) {
		/*
		 * Keep this for backward compatibility to support
		 * NFSMNT_SECURE/NFSMNT_KERBEROS/NFSMNT_RPCTIMESYNC flags.
		 */
		if (args.syncaddr == NULL)
			error = EINVAL;
		else {
			/*
			 * get time sync address.
			 */
			if (copyin((caddr_t)args.syncaddr, (caddr_t)&syncaddr,
			    sizeof (syncaddr))) {
				syncaddr.buf = NULL;
				error = EFAULT;
			} else {
				char *userbufptr = syncaddr.buf;

				syncaddr.buf = (char *) kmem_alloc(syncaddr.len,
								KM_SLEEP);
				syncaddr.maxlen = syncaddr.len;
				if (copyin(userbufptr, syncaddr.buf,
				    syncaddr.len))
					error = EFAULT;
			}

			/*
			 * get server's netname
			 */
			if (!error) {
				error = copyinstr(args.netname, netname,
					sizeof (netname), (u_int *) &nlen);
				netname[nlen] = '\0';
			}

			if (error && syncaddr.buf != NULL) {
				kmem_free((caddr_t)syncaddr.buf, syncaddr.len);
				syncaddr.buf = NULL;
			}
		}

		/*
		 * Move compatible security related data to the
		 * new extened sec_data structure.
		 */
		if (!error) {
			dh_k4_clntdata_t *data;
			char *pf, *p;

			secdata = (struct sec_data *) kmem_alloc(
						sizeof (*secdata), KM_SLEEP);
			if (args.flags & NFSMNT_RPCTIMESYNC)
				secdata->flags |= AUTH_F_RPCTIMESYNC;
			data = (dh_k4_clntdata_t *) kmem_alloc(
						sizeof (*data), KM_SLEEP);
			data->syncaddr = syncaddr;

			/*
			 * duplicate the knconf information for the
			 * new opaque data.
			 */
			data->knconf = (struct knetconfig *) kmem_alloc(
						sizeof (*knconf), KM_SLEEP);
			*data->knconf = *knconf;
			pf = (char *) kmem_alloc(KNC_STRSIZE, KM_SLEEP);
			p = (char *) kmem_alloc(KNC_STRSIZE, KM_SLEEP);
			bcopy(knconf->knc_protofmly, pf, KNC_STRSIZE);
			bcopy(knconf->knc_proto, pf, KNC_STRSIZE);
			data->knconf->knc_protofmly = pf;
			data->knconf->knc_proto = p;

			/* move server netname to the sec_data structure */
			if (nlen > 0) {
				data->netname = (char *) kmem_alloc((u_int)nlen,
								KM_SLEEP);
				bcopy(netname, data->netname, (u_int)nlen);
				data->netnamelen = nlen;
			}
			secdata->secmod = secdata->rpcflavor =
				(args.flags & NFSMNT_SECURE) ? AUTH_DES :
				((args.flags & NFSMNT_KERBEROS) ? AUTH_KERB :
							AUTH_UNIX);
			secdata->data = (caddr_t) data;
		}
	} else {
		secdata = (struct sec_data *) kmem_alloc(sizeof (*secdata),
					KM_SLEEP);
		secdata->secmod = secdata->rpcflavor = AUTH_UNIX;
		secdata->data = NULL;
	}
	if (error)
		goto errout;
	/*
	 *  See bug 1180236.
	 *  If mount secure failed, we will allow to fall back
	 *  to AUTH_NONE and try again.
	 *
	 *  V2 mount uses GETATTR and STATFS procedures.
	 *  Server does not care if these procedures have the proper
	 *  authentication flavor, so if mount retries using AUTH_NONE
	 *  that does not require a credential setup for root then
	 *  the automounter would work without requiring root to be
	 *  keylogged into AUTH_DES.
	 */
	if (secdata->rpcflavor != AUTH_UNIX)
		secdata->flags |= AUTH_F_TRYNONE;
	svp->sv_secdata = secdata;

	/*
	 * Failover support:
	 *
	 * We may have a linked list of nfs_args structures,
	 * which means the user is looking for failover.  If
	 * the mount is either not "read-only" or "soft",
	 * we want to bail out with EINVAL.
	 */
	if (args.nfs_args_ext == NFS_ARGS_EXT2 &&
	    args.nfs_ext_u.nfs_ext2.next) {
		if (uap->flags & MS_RDONLY && !(args.flags & NFSMNT_SOFT)) {
			data = (caddr_t) args.nfs_ext_u.nfs_ext2.next;
			goto more;
		} else {
			error = EINVAL;
			goto errout;
		}
	}

	/*
	 * Get root vnode.
	 */
	error = nfsrootvp(&rtvp, vfsp, svp_head, args.flags, cr);

	if (error)
		goto errout;

	/*
	 * Set option fields in mount info record
	 */
	mi = VTOMI(rtvp);

	if (svp_head->sv_next)
		mi->mi_flags |= MI_LLOCK;
	if (args.flags & NFSMNT_NOAC) {
		mi->mi_flags |= MI_NOAC;
		PURGE_ATTRCACHE(rtvp);
	}
	if (args.flags & NFSMNT_NOCTO)
		mi->mi_flags |= MI_NOCTO;
	if (args.flags & NFSMNT_LLOCK)
		mi->mi_flags |= MI_LLOCK;
	if (args.flags & NFSMNT_GRPID)
		mi->mi_flags |= MI_GRPID;
	if (args.flags & NFSMNT_RETRANS) {
		if (args.retrans < 0) {
			error = EINVAL;
			goto errout;
		}
		mi->mi_retrans = args.retrans;
	}
	if (args.flags & NFSMNT_TIMEO) {
		if (args.timeo <= 0) {
			error = EINVAL;
			goto errout;
		}
		mi->mi_timeo = args.timeo;
		/*
		 * The following scales the standard deviation and
		 * and current retransmission timer to match the
		 * initial value for the timeout specified.
		 */
		mi->mi_timers[3].rt_deviate = (args.timeo * hz * 2) / 5;
		mi->mi_timers[3].rt_rtxcur = args.timeo * hz / 10;
	}
	if (args.flags & NFSMNT_RSIZE) {
		if (args.rsize <= 0) {
			error = EINVAL;
			goto errout;
		}
		mi->mi_tsize = MIN(mi->mi_tsize, args.rsize);
		mi->mi_curread = mi->mi_tsize;
	}
	if (args.flags & NFSMNT_WSIZE) {
		if (args.wsize <= 0) {
			error = EINVAL;
			goto errout;
		}
		mi->mi_stsize = MIN(mi->mi_stsize, args.wsize);
		mi->mi_curwrite = mi->mi_stsize;
	}
	if (args.flags & NFSMNT_ACREGMIN) {
		if (args.acregmin < 0)
			mi->mi_acregmin = ACMINMAX;
		else
			mi->mi_acregmin = MIN(args.acregmin, ACMINMAX);
	}
	if (args.flags & NFSMNT_ACREGMAX) {
		if (args.acregmax < 0)
			mi->mi_acregmax = ACMAXMAX;
		else
			mi->mi_acregmax = MIN(args.acregmax, ACMAXMAX);
	}
	if (args.flags & NFSMNT_ACDIRMIN) {
		if (args.acdirmin < 0)
			mi->mi_acdirmin = ACMINMAX;
		else
			mi->mi_acdirmin = MIN(args.acdirmin, ACMINMAX);
	}
	if (args.flags & NFSMNT_ACDIRMAX) {
		if (args.acdirmax < 0)
			mi->mi_acdirmax = ACMAXMAX;
		else
			mi->mi_acdirmax = MIN(args.acdirmax, ACMAXMAX);
	}

	if (args.flags & NFSMNT_LOOPBACK)
		mi->mi_flags |= MI_LOOPBACK;

	error = pathconf_get(mi, &args);	/* static pathconf kludge */

errout:
	if (error) {
		if (rtvp != NULL) {
			rp = VTOR(rtvp);
			if (rp->r_flags & RHASHED) {
				mutex_enter(&nfs_rtable_lock);
				rp_rmhash(rp);
				mutex_exit(&nfs_rtable_lock);
			}
			VN_RELE(rtvp);
		}
		if (svp_head)
			sv_free(svp_head);
		if (mi != NULL) {
			nfs_async_stop(vfsp);
			nfs_free_mi(mi);
		}
	}
	return (error);
}

/*
 * The pathconf information is kept on a linked list of kmemalloc'ed
 * structs. We search the list & add a new struct iff there is no other
 * struct with the same information.
 * See sys/pathconf.h for ``the rest of the story.''
 */
static struct pathcnf *allpc = NULL;

static int
pathconf_get(struct mntinfo *mi, struct nfs_args *ap)
{
	register struct pathcnf *p;
	struct pathcnf pc;

	if (mi->mi_pathconf != NULL) {
		pathconf_rele(mi);
		mi->mi_pathconf = NULL;
	}
	if ((ap->flags & NFSMNT_POSIX) && ap->pathconf != NULL) {
		if (copyin((caddr_t)ap->pathconf, (caddr_t)&pc, sizeof (pc)))
			return (EFAULT);
		if (_PC_ISSET(_PC_ERROR, pc.pc_mask))
			return (EINVAL);
		for (p = allpc; p != NULL; p = p->pc_next)
			if (PCCMP(p, &pc) == 0)
				break;
		if (p != NULL) {
			mi->mi_pathconf = p;
			p->pc_refcnt++;
		} else {
			p = (struct pathcnf *) kmem_alloc(sizeof (*p),
							KM_SLEEP);
			*p = pc;
			p->pc_next = allpc;
			p->pc_refcnt = 1;
			allpc = mi->mi_pathconf = p;
		}
	}
	return (0);
}

/*
 * release the static pathconf information
 */
static void
pathconf_rele(struct mntinfo *mi)
{

	if (mi->mi_pathconf != NULL) {
		if (--mi->mi_pathconf->pc_refcnt == 0) {
			register struct pathcnf *p;
			register struct pathcnf *p2;

			p2 = p = allpc;
			while (p != NULL && p != mi->mi_pathconf) {
				p2 = p;
				p = p->pc_next;
			}
			if (p == NULL)
				cmn_err(CE_PANIC, "mi->pathconf");
			if (p == allpc)
				allpc = p->pc_next;
			else
				p2->pc_next = p->pc_next;
			kmem_free((caddr_t)p, (u_int)sizeof (*p));
			mi->mi_pathconf = NULL;
		}
	}
}

static int nfs_dynamic = 1;	/* global variable to enable dynamic retrans. */
static u_short nfs_max_threads = 8;	/* max number of active async threads */

int
nfsrootvp(vnode_t **rtvpp, vfs_t *vfsp, struct servinfo *svp,
	int flags, cred_t *cr)
{
	vnode_t *rtvp;
	mntinfo_t *mi;
	dev_t nfs_dev;
	struct vattr va;
	int error;
	rnode_t *rp;

	ASSERT(cr->cr_ref != 0);

	/*
	 * Create a mount record and link it to the vfs struct.
	 */
	mi = (mntinfo_t *) kmem_zalloc(sizeof (*mi), KM_SLEEP);
	mutex_init(&mi->mi_lock, "mi_lock", MUTEX_DEFAULT, NULL);
	mi->mi_flags = MI_ACL;
	if (!(flags & NFSMNT_SOFT))
		mi->mi_flags |= MI_HARD;
	if ((flags & NFSMNT_SEMISOFT))
		mi->mi_flags |= MI_SEMISOFT;
	if ((flags & NFSMNT_NOPRINT))
		mi->mi_flags |= MI_NOPRINT;
	if (flags & NFSMNT_INT)
		mi->mi_flags |= MI_INT;
	mi->mi_retrans = NFS_RETRIES;
	if (svp->sv_knconf->knc_semantics == NC_TPI_COTS_ORD ||
	    svp->sv_knconf->knc_semantics == NC_TPI_COTS)
		mi->mi_timeo = NFS_COTS_TIMEO;
	else
		mi->mi_timeo = NFS_TIMEO;
	mi->mi_prog = NFS_PROGRAM;
	mi->mi_vers = NFS_VERSION;
	mi->mi_rfsnames = rfsnames_v2;
	mi->mi_reqs = rfsreqcnt_v2;
	mi->mi_call_type = call_type_v2;
	mi->mi_ss_call_type = ss_call_type_v2;
	mi->mi_timer_type = timer_type_v2;
	mi->mi_aclnames = aclnames_v2;
	mi->mi_aclreqs = aclreqcnt_v2_ptr;
	mi->mi_acl_call_type = acl_call_type_v2;
	mi->mi_acl_ss_call_type = acl_ss_call_type_v2;
	mi->mi_acl_timer_type = acl_timer_type_v2;
	cv_init(&mi->mi_failover_cv, "nfs failover_cv", CV_DEFAULT, NULL);
	mi->mi_servers = svp;
	mi->mi_curr_serv = svp;
	mi->mi_acregmin = ACREGMIN;
	mi->mi_acregmax = ACREGMAX;
	mi->mi_acdirmin = ACDIRMIN;
	mi->mi_acdirmax = ACDIRMAX;

	if (nfs_dynamic)
		mi->mi_flags |= MI_DYNAMIC;

	/*
	 * Make a vfs struct for nfs.  We do this here instead of below
	 * because rtvp needs a vfs before we can do a getattr on it.
	 *
	 * Assign a unique device id to the mount
	 */
	mutex_enter(&nfs_minor_lock);
	do {
		nfs_minor = (nfs_minor + 1) & MAXMIN;
		nfs_dev = makedevice(nfs_major, nfs_minor);
	} while (vfs_devsearch(nfs_dev));
	mutex_exit(&nfs_minor_lock);

	vfsp->vfs_dev = nfs_dev;
	vfsp->vfs_fsid.val[0] = nfs_dev;
	vfsp->vfs_fsid.val[1] = nfsfstyp;
	vfsp->vfs_data = (caddr_t)mi;
	vfsp->vfs_fstype = nfsfstyp;
	vfsp->vfs_bsize = NFS_MAXDATA;

	/*
	 * Initialize fields used to support async putpage operations.
	 */
	mi->mi_async_reqs = mi->mi_async_tail = NULL;
	mi->mi_threads = 0;
	mi->mi_max_threads = nfs_max_threads;
	mutex_init(&mi->mi_async_lock, "nfs async_lock", MUTEX_DEFAULT, NULL);
	cv_init(&mi->mi_async_reqs_cv, "nfs async_reqs_cv", CV_DEFAULT, NULL);
	cv_init(&mi->mi_async_cv, "nfs async_cv", CV_DEFAULT, NULL);

	/*
	 * Make the root vnode, use it to get attributes,
	 * then remake it with the attributes.
	 */
	rtvp = makenfsnode((fhandle_t *)&(svp->sv_fhandle.fh_buf),
		NULL, vfsp, cr, NULL, NULL);
	mutex_enter(&rtvp->v_lock);
	ASSERT(!(rtvp->v_flag & VROOT));
	rtvp->v_flag |= VROOT;
	mutex_exit(&rtvp->v_lock);

	va.va_mask = AT_ALL;
	error = nfsgetattr(rtvp, &va, cr);
	if (error)
		goto bad;
	rtvp->v_type = va.va_type;

	mi->mi_rootvp = rtvp;

	/*
	 * Poll every server to get the filesystem stats; we're
	 * only interested in the server's transfer size, and we
	 * want the minimum.
	 *
	 * While we're looping, we'll turn off AUTH_F_TRYNONE,
	 * which is only for the mount operation.
	 */
	for (svp = mi->mi_servers; svp; svp = svp->sv_next) {
		struct nfsstatfs fs;
		int douprintf;

		douprintf = 1;
		mi->mi_curr_serv = svp;

		error = rfs2call(mi, RFS_STATFS,
			xdr_fhandle, (caddr_t)VTOFH(mi->mi_rootvp),
			xdr_statfs, (caddr_t)&fs, CRED(), &douprintf,
			&fs.fs_status, 0, NULL);
		if (error)
			goto bad;
		if (mi->mi_stsize)
			mi->mi_stsize = MIN(mi->mi_stsize, fs.fs_tsize);
		else
			mi->mi_stsize = fs.fs_tsize;
		mi->mi_curwrite = mi->mi_stsize;
		svp->sv_secdata->flags &= ~AUTH_F_TRYNONE;
	}
	mi->mi_tsize = min(NFS_MAXDATA, (u_int)nfstsize());
	mi->mi_curread = mi->mi_tsize;
	mi->mi_curr_serv = mi->mi_servers;

	*rtvpp = rtvp;
	return (0);
bad:
	/*
	 * An error occurred somewhere, need to clean up...
	 * We need to release our reference to the root vnode and
	 * destroy the mntinfo struct that we just created.
	 */
	rp = VTOR(rtvp);
	if (rp->r_flags & RHASHED) {
		mutex_enter(&nfs_rtable_lock);
		rp_rmhash(rp);
		mutex_exit(&nfs_rtable_lock);
	}
	VN_RELE(rtvp);
	nfs_async_stop(vfsp);
	nfs_free_mi(mi);
	*rtvpp = NULL;
	return (error);
}

/*
 * vfs operations
 */
static int
nfs_unmount(vfs_t *vfsp, cred_t *cr)
{
	mntinfo_t *mi;
	vnode_t *vp;
	u_short omax;

	if (!suser(cr))
		return (EPERM);

	/*
	 * Wait until all asynchronous putpage operations on
	 * this file system are complete before flushing rnodes
	 * from the cache.
	 */
	mi = VFTOMI(vfsp);
	omax = mi->mi_max_threads;
	nfs_async_stop(vfsp);

	rflush(vfsp, cr);

	vp = mi->mi_rootvp;
	mutex_enter(&nfs_rtable_lock);
	ASSERT(vp->v_count > 0);

	/*
	 * If the reference count on the root vnode is higher
	 * than 1 or if there are any other active vnodes on
	 * this file system, then the file system is busy and
	 * it can't be umounted.
	 */
	if (vp->v_count != 1 || check_rtable(vfsp, vp)) {
		mutex_exit(&nfs_rtable_lock);
		mutex_enter(&mi->mi_async_lock);
		mi->mi_max_threads = omax;
		mutex_exit(&mi->mi_async_lock);
		return (EBUSY);
	}

	/*
	 * Purge all rnodes belonging to this file system from the
	 * rnode hash queues and purge any resources allocated to
	 * them.
	 */
	purge_rtable(vfsp, cr);
	mutex_exit(&nfs_rtable_lock);

	VN_RELE(vp);

	pathconf_rele(mi);
	sv_free(mi->mi_servers);
	nfs_free_mi(mi);

	return (0);
}

/*
 * find root of nfs
 */
static int
nfs_root(vfs_t *vfsp, vnode_t **vpp)
{

	*vpp = VFTOMI(vfsp)->mi_rootvp;
	VN_HOLD(*vpp);
	return (0);
}

/*
 * Get file system statistics.
 */
static int
nfs_statvfs(vfs_t *vfsp, struct statvfs64 *sbp)
{
	int error;
	mntinfo_t *mi;
	struct nfsstatfs fs;
	int douprintf;
	failinfo_t fi;

	mi = VFTOMI(vfsp);
	douprintf = 1;
	fi.vp = mi->mi_rootvp;
	fi.fhp = NULL;		/* no need to update, filehandle not copied */
	fi.copyproc = nfscopyfh;
	fi.lookupproc = nfslookup;

	error = rfs2call(mi, RFS_STATFS,
			xdr_fhandle, (caddr_t)VTOFH(mi->mi_rootvp),
			xdr_statfs, (caddr_t)&fs, CRED(), &douprintf,
			&fs.fs_status, 0, &fi);
	if (!error) {
		error = geterrno(fs.fs_status);
		if (!error) {
			mutex_enter(&mi->mi_lock);
			if (mi->mi_stsize) {
				mi->mi_stsize = MIN(mi->mi_stsize, fs.fs_tsize);
			} else {
				mi->mi_stsize = fs.fs_tsize;
				mi->mi_curwrite = mi->mi_stsize;
			}
			mutex_exit(&mi->mi_lock);
			sbp->f_bsize = fs.fs_bsize;
			sbp->f_frsize = fs.fs_bsize;
			sbp->f_blocks = (fsblkcnt64_t)fs.fs_blocks;
			sbp->f_bfree = (fsblkcnt64_t)fs.fs_bfree;
			sbp->f_bavail = (fsblkcnt64_t)fs.fs_bavail;
			sbp->f_files = (fsfilcnt64_t)-1;
			sbp->f_ffree = (fsfilcnt64_t)-1;
			sbp->f_favail = (fsfilcnt64_t)-1;
			bcopy((caddr_t)&vfsp->vfs_fsid, (caddr_t)&sbp->f_fsid,
			    sizeof (fsid_t));
			strncpy(sbp->f_basetype,
				vfssw[vfsp->vfs_fstype].vsw_name, FSTYPSZ);
			sbp->f_flag = vf_to_stf(vfsp->vfs_flag);
			sbp->f_namemax = (u_long)-1;
		} else {
			PURGE_STALE_FH(error, mi->mi_rootvp, CRED());
		}
	}
	return (error);
}

static kmutex_t nfs_syncbusy;

/*
 * Flush dirty nfs files for file system vfsp.
 * If vfsp == NULL, all nfs files are flushed.
 */
/* ARGSUSED */
static int
nfs_sync(vfs_t *vfsp, short flag, cred_t *cr)
{

	if (!(flag & SYNC_ATTR) && mutex_tryenter(&nfs_syncbusy) != 0) {
		rflush(vfsp, cr);
		mutex_exit(&nfs_syncbusy);
	}
	return (0);
}

/* ARGSUSED */
static int
nfs_vget(vfs_t *vfsp, vnode_t **vpp, fid_t *fidp)
{
	int error;
	vnode_t *vp;
	struct vattr va;
	struct nfs_fid *nfsfidp = (struct nfs_fid *)fidp;

	if (fidp->fid_len != (sizeof (*nfsfidp) - sizeof (short))) {
#ifdef DEBUG
		cmn_err(CE_WARN, "nfs_vget: bad fid len, %d/%d",
			fidp->fid_len, (sizeof (*nfsfidp) - sizeof (short)));
#endif
		*vpp = NULL;
		return (ESTALE);
	}

	vp = makenfsnode((fhandle_t *)(nfsfidp->nf_data), NULL, vfsp, CRED(),
			NULL, NULL);

	if (vp->v_type == VNON) {
		va.va_mask = AT_ALL;
		error = nfsgetattr(vp, &va, CRED());
		if (error) {
			VN_RELE(vp);
			*vpp = NULL;
			return (error);
		}
		vp->v_type = va.va_type;
	}

	*vpp = vp;

	return (0);
}

/* ARGSUSED */
static int
nfs_mountroot(vfs_t *vfsp, whymountroot_t why)
{
	vnode_t *rtvp;
	char root_hostname[SYS_NMLN+1];
	struct servinfo *svp;
	int error;
	int size;
	char *root_path;
	struct pathname pn;
	char *name;
	cred_t *cr;
	mntinfo_t *mi;
	static char token[10];

	/* do this BEFORE getfile which causes xid stamps to be initialized */
	clkset(-1L);		/* hack for now - until we get time svc? */

	if (why == ROOT_REMOUNT) {
		/*
		 * Shouldn't happen.
		 */
		panic("nfs_mountroot: why == ROOT_REMOUNT\n");
	}

	if (why == ROOT_UNMOUNT) {
		/*
		 * Nothing to do for NFS.
		 */
		return (0);
	}

	/*
	 * why == ROOT_INIT
	 */

	name = token;
	*name = 0;
	(void) getfsname("root", name);

	pn_alloc(&pn);
	root_path = pn.pn_path;

	svp = (struct servinfo *) kmem_zalloc(sizeof (*svp), KM_SLEEP);
	svp->sv_knconf = (struct knetconfig *)
		kmem_zalloc(sizeof (*svp->sv_knconf), KM_SLEEP);
	svp->sv_knconf->knc_protofmly = (char *)
		kmem_alloc(KNC_STRSIZE, KM_SLEEP);
	svp->sv_knconf->knc_proto = (char *) kmem_alloc(KNC_STRSIZE, KM_SLEEP);

	if (error = mount_root(svp->sv_knconf, *name ? name : "root",
		&svp->sv_addr, (fhandle_t *)&svp->sv_fhandle.fh_buf,
		root_hostname, root_path)) {
		nfs_cmn_err(error, CE_WARN,
			    "nfs_mountroot: mount_root failed: %m");
		sv_free(svp);
		pn_free(&pn);
		return (error);
	}
	svp->sv_hostnamelen = strlen(root_hostname) + 1;
	svp->sv_hostname = (char *) kmem_alloc(svp->sv_hostnamelen, KM_SLEEP);
	strcpy(svp->sv_hostname, root_hostname);

	svp->sv_secdata = (struct sec_data *)
		kmem_alloc(sizeof (*svp->sv_secdata), KM_SLEEP);
	svp->sv_secdata->secmod = AUTH_UNIX;
	svp->sv_secdata->rpcflavor = AUTH_UNIX;
	svp->sv_secdata->data = NULL;

	cr = crgetcred();

	error = nfsrootvp(&rtvp, vfsp, svp, 0, cr);

	crfree(cr);

	if (error) {
		sv_free(svp);
		pn_free(&pn);
		nfs_async_stop(vfsp);
		return (error);
	}

	(void) vfs_lock_wait(vfsp);

	if (why != ROOT_BACKMOUNT)
		vfs_add(NULL, vfsp, 0);

	/*
	 * Set maximum attribute timeouts and turn off close-to-open
	 * consistency checking and set local locking.
	 */
	mi = VFTOMI(vfsp);
	if (why == ROOT_BACKMOUNT) {
		/* cache-only client */
		mi->mi_acregmin = ACREGMIN;
		mi->mi_acregmax = ACREGMAX;
		mi->mi_acdirmin = ACDIRMIN;
		mi->mi_acdirmax = ACDIRMAX;
	} else {
		/* diskless */
		mi->mi_acregmin = ACMINMAX;
		mi->mi_acregmax = ACMAXMAX;
		mi->mi_acdirmin = ACMINMAX;
		mi->mi_acdirmax = ACMAXMAX;
	}
	mutex_enter(&mi->mi_lock);
	mi->mi_flags |= (MI_NOCTO | MI_LLOCK);
	mutex_exit(&mi->mi_lock);

	vfs_unlock(vfsp);

	size = strlen(svp->sv_hostname);
	strcpy(rootfs.bo_name, svp->sv_hostname);
	rootfs.bo_name[size] = ':';
	strcpy(&rootfs.bo_name[size+1], root_path);

	pn_free(&pn);

	return (0);
}

/*
 * Initialization routine for VFS routines.  Should only be called once
 */
int
nfs_vfsinit(void)
{

	mutex_init(&nfs_syncbusy, "nfs_syncbusy", MUTEX_DEFAULT, NULL);
	return (0);
}
