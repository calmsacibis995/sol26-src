/*
 * Copyright (c) 1996, Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident   "@(#)cachefs_ioctl.c 1.69     96/10/17 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/file.h>
#include <sys/filio.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/mman.h>
#include <sys/tiuser.h>
#include <sys/pathname.h>
#include <sys/dirent.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/vmsystm.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/swap.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/disp.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/vtrace.h>
#include <sys/mount.h>
#include <sys/dnlc.h>
#include <sys/stat.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <vm/seg.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/rm.h>
#include <sys/fs/cachefs_fs.h>
#include <sys/fs/cachefs_dlog.h>
#include <sys/fs/cachefs_ioctl.h>
#include <sys/fs/cachefs_dir.h>
#include <sys/fs/cachefs_dlog.h>
#include "fs/fs_subr.h"

void cachefs_addhash(struct cnode *);


/*
 * Local functions
 */
static void sync_metadata(cnode_t *);
static void drop_backvp(cnode_t *);
static void allow_pendrm(cnode_t *cp);
static int cachefs_unpack_common(vnode_t *vp);
static int cachefs_unpackall_list(cachefscache_t *cachep,
    enum cachefs_rl_type type);
static void cachefs_modified_fix(fscache_t *fscp);
static void cachefs_iosetneedattrs(fscache_t *fscp, cfs_cid_t *cidp);

/*
 * Pack a file in the cache
 *	dvp is the directory the file resides in.
 *	name is the name of the file.
 *	Returns 0 or an error if could not perform the operation.
 */
int
cachefs_pack(struct vnode *dvp, char *name, cred_t *cr)
{
	fscache_t *fscp = C_TO_FSCACHE(VTOC(dvp));
	int error = 0;
	int connected = 0;
	vnode_t *vp;

	for (;;) {
		/* get access to the file system */
		error = cachefs_cd_access(fscp, connected, 0);
		if (error)
			break;

		/* lookup the file name */
		error = cachefs_lookup_common(dvp, name, &vp, NULL, 0, NULL,
		    cr);
		if (error == 0) {
			error = cachefs_pack_common(vp, cr);
			VN_RELE(vp);
		}
		if (CFS_TIMEOUT(fscp, error)) {
			if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
				cachefs_cd_release(fscp);
				cachefs_cd_timedout(fscp);
				connected = 0;
				continue;
			} else {
				cachefs_cd_release(fscp);
				connected = 1;
				continue;
			}
		}
		cachefs_cd_release(fscp);
		break;
	}
	return (error);
}
/*
 * Packs the file belonging to the passed in vnode.
 */
int
cachefs_pack_common(vnode_t *vp, cred_t *cr)
{
	cnode_t *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int error = 0;
	offset_t off;
	caddr_t buf;
	int buflen;
	rl_entry_t rl_ent;
	u_offset_t cnode_size;

	rw_enter(&cp->c_rwlock, RW_WRITER);
	cachefs_mutex_enter(&cp->c_statelock);

	/* done if cannot write to cache */
	if ((cp->c_filegrp->fg_flags & CFS_FG_WRITE) == 0) {
		error = EROFS;
		goto out;
	}

	/* done if not usable */
	if (cp->c_flags & (CN_STALE | CN_DESTROY)) {
		error = ESTALE;
		goto out;
	}

	/* make sure up to date */
	error = CFSOP_CHECK_COBJECT(fscp, cp, C_BACK_CHECK, cr);
	if (error)
		goto out;

	/* make it cachable */
	cp->c_flags &= ~CN_NOCACHE;

	/* get a metadata slot if we do not have one yet */
	if (cp->c_flags & CN_ALLOC_PENDING) {
		if (cp->c_filegrp->fg_flags & CFS_FG_ALLOC_ATTR) {
			(void) filegrp_allocattr(cp->c_filegrp);
		}
		error = filegrp_create_metadata(cp->c_filegrp,
		    &cp->c_metadata, &cp->c_id);
		if (error)
			goto out;
		cp->c_flags &= ~CN_ALLOC_PENDING;
		cp->c_flags |= CN_UPDATED;
	}

	/* cache the ACL if necessary */
	if (((fscp->fs_info.fi_mntflags & CFS_NOACL) == 0) &&
	    (cachefs_vtype_aclok(vp)) &&
	    ((cp->c_metadata.md_flags & MD_ACL) == 0)) {
		error = cachefs_cacheacl(cp, NULL);
		if (error != 0)
			goto out;
	}

	/* directory */
	if (vp->v_type == VDIR) {
		if (cp->c_metadata.md_flags & MD_POPULATED)
			goto out;

		if (error = cachefs_dir_fill(cp, cr))
			goto out;
	}

	/* regular file */
	else if (vp->v_type == VREG) {
		if (cp->c_metadata.md_flags & MD_POPULATED)
			goto out;

		if (cp->c_backvp == NULL) {
			error = cachefs_getbackvp(fscp, cp);
			if (error)
				goto out;
		}
		if (cp->c_frontvp == NULL) {
			error = cachefs_getfrontfile(cp);
			if (error)
				goto out;
		}
		/* populate the file */
		off = (offset_t)0;
		cnode_size = cp->c_attr.va_size;
		while (off < cnode_size) {
			if (!cachefs_check_allocmap(cp, off)) {
				u_offset_t popoff;
				u_int popsize;

				cachefs_cluster_allocmap(off, &popoff,
				    &popsize, DEF_POP_SIZE, cp);
				if (popsize != 0) {
					error = cachefs_populate(cp, popoff,
					    popsize, cp->c_frontvp,
					    cp->c_backvp, cp->c_size, cr);
					if (error)
						goto out;
					else
						cp->c_flags |= (CN_UPDATED |
						    CN_NEED_FRONT_SYNC |
						    CN_POPULATION_PENDING);
					popsize = popsize - (off - popoff);
				}
			}
			off += MAXBSIZE;
		}
	}

	/* symbolic link */
	else if (vp->v_type == VLNK) {
		if (cp->c_metadata.md_flags & (MD_POPULATED | MD_FASTSYMLNK))
			goto out;

		/* get the sym link contents from the back fs */
		error = cachefs_readlink_back(cp, cr, &buf, &buflen);
		if (error)
			goto out;

		/* try to cache the sym link */
		error = cachefs_stuffsymlink(cp, buf, buflen);
		cachefs_kmem_free(buf, MAXPATHLEN);
	}

	/* assume that all other types fit in the attributes */

out:
	/* get the rl slot if needed */
	if ((error == 0) && (cp->c_metadata.md_rlno == 0)) {
		rl_ent.rl_fileno = cp->c_id.cid_fileno;
		rl_ent.rl_local = (cp->c_id.cid_flags & CFS_CID_LOCAL) ? 1 : 0;
		rl_ent.rl_fsid = fscp->fs_cfsid;
		rl_ent.rl_attrc = 0;
		cp->c_metadata.md_rltype = CACHEFS_RL_NONE;
		error = cachefs_rl_alloc(fscp->fs_cache, &rl_ent,
		    &cp->c_metadata.md_rlno);
		if (error == 0)
			error = filegrp_ffhold(cp->c_filegrp);
	}

	/* mark the file as packed */
	if (error == 0) {
		/* modified takes precedence over packed */
		if (cp->c_metadata.md_rltype != CACHEFS_RL_MODIFIED) {
			cachefs_rlent_moveto(fscp->fs_cache,
			    CACHEFS_RL_PACKED, cp->c_metadata.md_rlno,
			    cp->c_metadata.md_frontblks);
			cp->c_metadata.md_rltype = CACHEFS_RL_PACKED;
		}
		cp->c_metadata.md_flags |= MD_PACKED;
		cp->c_flags |= CN_UPDATED;
	}

	cachefs_mutex_exit(&cp->c_statelock);
	rw_exit(&cp->c_rwlock);

	return (error);
}

/*
 * Unpack a file from the cache
 *	dvp is the directory the file resides in.
 *	name is the name of the file.
 *	Returns 0 or an error if could not perform the operation.
 */
int
cachefs_unpack(struct vnode *dvp, char *name, cred_t *cr)
{
	fscache_t *fscp = C_TO_FSCACHE(VTOC(dvp));
	int error = 0;
	int connected = 0;
	vnode_t *vp;

	for (;;) {
		/* get access to the file system */
		error = cachefs_cd_access(fscp, connected, 0);
		if (error)
			break;

		/* lookup the file name */
		error = cachefs_lookup_common(dvp, name, &vp, NULL, 0, NULL,
		    cr);
		if (error == 0) {
			error = cachefs_unpack_common(vp);
			VN_RELE(vp);
		}
		if (CFS_TIMEOUT(fscp, error)) {
			if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
				cachefs_cd_release(fscp);
				cachefs_cd_timedout(fscp);
				connected = 0;
				continue;
			} else {
				cachefs_cd_release(fscp);
				connected = 1;
				continue;
			}
		}
		cachefs_cd_release(fscp);
		break;
	}
	return (error);
}

/*
 * Unpacks the file belonging to the passed in vnode.
 */
static int
cachefs_unpack_common(vnode_t *vp)
{
	cnode_t *cp = VTOC(vp);
	fscache_t *fscp = C_TO_FSCACHE(cp);
	int error = 0;

	cachefs_mutex_enter(&cp->c_statelock);

	/* nothing to do if not packed */
	if ((cp->c_metadata.md_flags & MD_PACKED) == 0)
		goto out;

	/* nothing to do if cannot modify cache */
	if ((cp->c_filegrp->fg_flags & CFS_FG_WRITE) == 0) {
		error = EROFS;
		goto out;
	}

	/* mark file as no longer packed */
	ASSERT(cp->c_metadata.md_rlno);
	cp->c_metadata.md_flags &= ~MD_PACKED;
	cp->c_flags |= CN_UPDATED;

	/* done if file has been modified */
	if (cp->c_metadata.md_rltype == CACHEFS_RL_MODIFIED)
		goto out;

	/* if there is no front file */
	if ((cp->c_metadata.md_flags & MD_FILE) == 0) {
		/* nuke front file resources */
		filegrp_ffrele(cp->c_filegrp);
		cachefs_rlent_moveto(fscp->fs_cache,
		    CACHEFS_RL_FREE, cp->c_metadata.md_rlno, 0);
		cp->c_metadata.md_rlno = 0;
		cp->c_metadata.md_rltype = CACHEFS_RL_NONE;
	}

	/* else move the front file to the active list */
	else {
		cachefs_rlent_moveto(fscp->fs_cache,
		    CACHEFS_RL_ACTIVE, cp->c_metadata.md_rlno,
		    cp->c_metadata.md_frontblks);
		cp->c_metadata.md_rltype = CACHEFS_RL_ACTIVE;
	}

out:
	cachefs_mutex_exit(&cp->c_statelock);
	return (error);
}

/*
 * Returns packing information on a file.
 *	dvp is the directory the file resides in.
 *	name is the name of the file.
 *	*statusp is set to the status of the file
 *	Returns 0 or an error if could not perform the operation.
 */
int
cachefs_packinfo(struct vnode *dvp, char *name, int *statusp, cred_t *cr)
{
	fscache_t *fscp = C_TO_FSCACHE(VTOC(dvp));
	struct vnode *vp;
	struct cnode *cp;
	int error;
	int connected = 0;

	*statusp = 0;

	for (;;) {
		/* get access to the file system */
		error = cachefs_cd_access(fscp, connected, 0);
		if (error)
			break;

		/* lookup the file name */
		error = cachefs_lookup_common(dvp, name, &vp, NULL, 0, NULL,
		    cr);
		if (CFS_TIMEOUT(fscp, error)) {
			if (fscp->fs_cdconnected == CFS_CD_CONNECTED) {
				cachefs_cd_release(fscp);
				cachefs_cd_timedout(fscp);
				connected = 0;
				continue;
			} else {
				cachefs_cd_release(fscp);
				connected = 1;
				continue;
			}
		}
		if (error)
			break;
		cp = VTOC(vp);

		cachefs_mutex_enter(&cp->c_statelock);
		if (cp->c_metadata.md_flags & MD_PACKED)
			*statusp |= CACHEFS_PACKED_FILE;
		if (cp->c_metadata.md_flags & (MD_POPULATED | MD_FASTSYMLNK))
			*statusp |= CACHEFS_PACKED_DATA;
		else if ((vp->v_type != VREG) &&
		    (vp->v_type != VDIR) &&
		    (vp->v_type != VLNK))
			*statusp |= CACHEFS_PACKED_DATA;
		else if (cp->c_size == 0)
			*statusp |= CACHEFS_PACKED_DATA;
		if (cp->c_flags & CN_NOCACHE)
			*statusp |= CACHEFS_PACKED_NOCACHE;
		cachefs_mutex_exit(&cp->c_statelock);

		VN_RELE(vp);
		cachefs_cd_release(fscp);
		break;
	}
	return (error);
}

/*
 * Finds all packed files in the cache and unpacks them.
 */
int
cachefs_unpackall(vnode_t *vp)
{
	fscache_t *fscp = C_TO_FSCACHE(VTOC(vp));
	cachefscache_t *cachep = fscp->fs_cache;
	int error;

	error = cachefs_unpackall_list(cachep, CACHEFS_RL_PACKED);
	if (error)
		goto out;
	error = cachefs_unpackall_list(cachep, CACHEFS_RL_PACKED_PENDING);
out:
	return (error);
}

/*
 * Finds all packed files on the specified list and unpacks them.
 */
static int
cachefs_unpackall_list(cachefscache_t *cachep, enum cachefs_rl_type type)
{
	fscache_t *fscp = NULL;
	cnode_t *cp;
	int error = 0;
	rl_entry_t rl_ent;
	cfs_cid_t cid;

	rl_ent.rl_current = type;
	for (;;) {
		/* get the next entry on the specified resource list */
		error = cachefs_rlent_data(cachep, &rl_ent, NULL);
		if (error) {
			error = 0;
			break;
		}

		/* if the fscp we have does not match */
		if ((fscp == NULL) || (fscp->fs_cfsid != rl_ent.rl_fsid)) {
			if (fscp) {
				cachefs_cd_release(fscp);
				fscache_rele(fscp);
				fscp = NULL;
			}

			/* get the file system cache object for this fsid */
			cachefs_mutex_enter(&cachep->c_fslistlock);
			fscp = fscache_list_find(cachep, rl_ent.rl_fsid);
			if (fscp == NULL) {
				fscp = fscache_create(cachep);
				error = fscache_activate(fscp, rl_ent.rl_fsid,
				    NULL, NULL, 0);
				if (error) {
					cmn_err(CE_WARN,
					    "cachefs: cache error, run fsck\n");
					fscache_destroy(fscp);
					fscp = NULL;
					cachefs_mutex_exit(
					    &cachep->c_fslistlock);
					break;
				}
				fscache_list_add(cachep, fscp);
			}
			fscache_hold(fscp);
			cachefs_mutex_exit(&cachep->c_fslistlock);

			/* get access to the file system */
			error = cachefs_cd_access(fscp, 0, 0);
			if (error) {
				fscache_rele(fscp);
				fscp = NULL;
				break;
			}
		}

		/* get the cnode for the file */
		cid.cid_fileno = rl_ent.rl_fileno;
		cid.cid_flags = rl_ent.rl_local ? CFS_CID_LOCAL : 0;
		error = cachefs_cnode_make(&cid, fscp,
		    NULL, NULL, NULL, kcred, 0, &cp);
		if (error) {
#ifdef CFSDEBUG
			CFS_DEBUG(CFSDEBUG_IOCTL)
				printf("cachefs: cul: could not find %lu\n",
				    cid.cid_fileno);
			delay(5*hz);
#endif
			continue;
		}

		/* unpack the file */
		(void) cachefs_unpack_common(CTOV(cp));
		VN_RELE(CTOV(cp));
	}

	/* free up allocated resources */
	if (fscp) {
		cachefs_cd_release(fscp);
		fscache_rele(fscp);
	}
	return (error);
}

/*
 * Identifies this process as the cachefsd.
 * Stays this way until close is done.
 */
int
/*ARGSUSED*/
cachefs_io_daemonid(vnode_t *vp, void *dinp, void *doutp)
{
	int error = 0;

	fscache_t *fscp = C_TO_FSCACHE(VTOC(vp));
	cachefscache_t *cachep = fscp->fs_cache;

	cachefs_mutex_enter(&fscp->fs_cdlock);

	/* can only do this on the root of the file system */
	if (vp != fscp->fs_rootvp)
		error = ENOENT;

	/* else if there already is a daemon running */
	else if (fscp->fs_cddaemonid)
		error = EBUSY;

	/* else use the pid to identify the daemon */
	else {
		fscp->fs_cddaemonid = ttoproc(curthread)->p_pid;
		cv_broadcast(&fscp->fs_cdwaitcv);
	}

	cachefs_mutex_exit(&fscp->fs_cdlock);

	if (error == 0) {
		/* the daemon that takes care of root is special */
		if (fscp->fs_flags & CFS_FS_ROOTFS) {
			cachefs_mutex_enter(&cachep->c_contentslock);
			ASSERT(cachep->c_rootdaemonid == 0);
			cachep->c_rootdaemonid = fscp->fs_cddaemonid;
			cachefs_mutex_exit(&cachep->c_contentslock);
		}
	}
	return (error);
}

/*
 * Returns the current state in doutp
 */
int
/*ARGSUSED*/
cachefs_io_stateget(vnode_t *vp, void *dinp, void *doutp)
{
	fscache_t  *fscp = C_TO_FSCACHE(VTOC(vp));
	int *statep = (int *)doutp;
	int state;

	cachefs_mutex_enter(&fscp->fs_cdlock);
	switch (fscp->fs_cdconnected) {
	case CFS_CD_CONNECTED:
		state = CFS_FS_CONNECTED;
		break;
	case CFS_CD_DISCONNECTED:
		state = CFS_FS_DISCONNECTED;
		break;
	case CFS_CD_RECONNECTING:
		state = CFS_FS_RECONNECTING;
		break;
	default:
		ASSERT(0);
		break;
	}
	cachefs_mutex_exit(&fscp->fs_cdlock);

	*statep = state;
	return (0);
}

/*
 * Sets the state of the file system.
 */
int
/*ARGSUSED*/
cachefs_io_stateset(vnode_t *vp, void *dinp, void *doutp)
{
	fscache_t *fscp = C_TO_FSCACHE(VTOC(vp));
	int error = 0;
	int nosig = 1;
	int state = *(int *)dinp;

	/* wait until the file system is quiet */
	cachefs_mutex_enter(&fscp->fs_cdlock);
	if (fscp->fs_cdtransition == 1) {
		/* if someone is already changing the state */
		cachefs_mutex_exit(&fscp->fs_cdlock);
		return (0);
	}
	fscp->fs_cdtransition = 1;
	while (nosig && (fscp->fs_cdrefcnt != 0)) {
		nosig = cachefs_cv_wait_sig(&fscp->fs_cdwaitcv,
		    &fscp->fs_cdlock);
	}
	if (!nosig) {
		fscp->fs_cdtransition = 0;
		cv_broadcast(&fscp->fs_cdwaitcv);
		cachefs_mutex_exit(&fscp->fs_cdlock);
		return (EINTR);
	}
	cachefs_mutex_exit(&fscp->fs_cdlock);

	switch (state) {
	case CFS_FS_CONNECTED:
		/* done if already in this state */
		if (fscp->fs_cdconnected == CFS_CD_CONNECTED)
			break;

		cachefs_mutex_enter(&fscp->fs_cdlock);
		fscp->fs_cdconnected = CFS_CD_CONNECTED;
		cachefs_mutex_exit(&fscp->fs_cdlock);

		/* fix up modified files */
		cachefs_modified_fix(fscp);

#if 0
		if (fscp->fs_hostname != NULL)
			printf("\ncachefs:server          - %s",
			    fscp->fs_hostname);
		if (fscp->fs_mntpt != NULL)
			printf("\ncachefs:mount point     - %s",
			    fscp->fs_mntpt);
		if (fscp->fs_backfsname != NULL)
			printf("\ncachefs:back filesystem - %s",
			    fscp->fs_backfsname);
		printf("\nok\n");
#else
		if (fscp->fs_hostname && fscp->fs_backfsname)
			printf("cachefs: %s:%s ok\n",
			    fscp->fs_hostname, fscp->fs_backfsname);
		else
			printf("cachefs: server ok\n");
#endif

		/* allow deletion of renamed open files to proceed */
		cachefs_cnode_traverse(fscp, allow_pendrm);
		break;

	case CFS_FS_DISCONNECTED:
		/* done if already in this state */
		if (fscp->fs_cdconnected == CFS_CD_DISCONNECTED)
			break;

		/* drop all back vps */
		cachefs_cnode_traverse(fscp, drop_backvp);


		cachefs_mutex_enter(&fscp->fs_cdlock);
		fscp->fs_cdconnected = CFS_CD_DISCONNECTED;
		cachefs_mutex_exit(&fscp->fs_cdlock);

#if 0
		if (fscp->fs_hostname != NULL)
			printf("\ncachefs:server          - %s",
			    fscp->fs_hostname);
		if (fscp->fs_mntpt != NULL)
			printf("\ncachefs:mount point     - %s",
			    fscp->fs_mntpt);
		if (fscp->fs_backfsname != NULL)
			printf("\ncachefs:back filesystem - %s",
			    fscp->fs_backfsname);
		printf("\nnot responding still trying\n");
#else
		if (fscp->fs_hostname && fscp->fs_backfsname)
			printf("cachefs: %s:%s not responding still trying\n",
			    fscp->fs_hostname, fscp->fs_backfsname);
		else
			printf("cachefs: server not responding still trying\n");
#endif
		break;

	case CFS_FS_RECONNECTING:
		/* done if already in this state */
		if (fscp->fs_cdconnected == CFS_CD_RECONNECTING)
			break;

		/*
		 * Before we enter disconnected state we sync all metadata,
		 * this allows us to read metadata directly in subsequent
		 * calls so we don't need to allocate cnodes when
		 * we just need metadata information.
		 */
		/* XXX bob: need to eliminate this */
		cachefs_cnode_traverse(fscp, sync_metadata);

		cachefs_mutex_enter(&fscp->fs_cdlock);
		fscp->fs_cdconnected = CFS_CD_RECONNECTING;
		cachefs_mutex_exit(&fscp->fs_cdlock);

		/* no longer need dlog active */
		cachefs_dlog_teardown(fscp);
		break;

	default:
		error = ENOTTY;
		break;
	}

	cachefs_mutex_enter(&fscp->fs_cdlock);
	fscp->fs_cdtransition = 0;
	cv_broadcast(&fscp->fs_cdwaitcv);
	cachefs_mutex_exit(&fscp->fs_cdlock);
	return (error);
}

/*
 * Blocks until the file system switches
 * out of the connected state.
 */
int
/*ARGSUSED*/
cachefs_io_xwait(vnode_t *vp, void *dinp, void *doutp)
{
	fscache_t *fscp = C_TO_FSCACHE(VTOC(vp));
	int nosig = 1;

	cachefs_mutex_enter(&fscp->fs_cdlock);
	while (nosig &&
	    (fscp->fs_cdconnected == CFS_CD_CONNECTED)) {
		nosig = cachefs_cv_wait_sig(&fscp->fs_cdwaitcv,
		    &fscp->fs_cdlock);
	}
	cachefs_mutex_exit(&fscp->fs_cdlock);
	if (!nosig)
		return (EINTR);

	return (0);
}

#define	RL_HEAD(cachep, type) \
	(&(cachep->c_rlinfo.rl_items[CACHEFS_RL_INDEX(type)]))

/*
 * Returns some statistics about the cache.
 */
int
/*ARGSUSED*/
cachefs_io_getstats(vnode_t *vp, void *dinp, void *doutp)
{
	fscache_t *fscp = C_TO_FSCACHE(VTOC(vp));
	cachefscache_t *cachep = fscp->fs_cache;
	struct statvfs64 sb;
	fsblkcnt64_t avail = 0;
	fsblkcnt64_t blocks;
	int error;
	long factor;
	cachefsio_getstats_t *gsp = (cachefsio_getstats_t *)doutp;

	/* determine number of blocks available to the cache */
	error = VFS_STATVFS(cachep->c_dirvp->v_vfsp, &sb);
	if (error == 0) {
		blocks = (fsblkcnt64_t)(cachep->c_label.cl_maxblks -
		    cachep->c_usage.cu_blksused);
		if ((longlong_t)blocks < (longlong_t)0)
			blocks = (fsblkcnt64_t)0;
		avail = (sb.f_bfree * sb.f_frsize) / MAXBSIZE;
		if (blocks < avail)
			avail = blocks;
	}

	factor = MAXBSIZE / 1024;
	gsp->gs_total = cachep->c_usage.cu_blksused * factor;
	gsp->gs_gc = RL_HEAD(cachep, CACHEFS_RL_GC)->rli_blkcnt * factor;
	gsp->gs_active = RL_HEAD(cachep, CACHEFS_RL_ACTIVE)->rli_blkcnt *
	    factor;
	gsp->gs_packed = RL_HEAD(cachep, CACHEFS_RL_PACKED)->rli_blkcnt *
	    factor;
	gsp->gs_free = (long)(avail * factor);
	gsp->gs_gctime = cachep->c_rlinfo.rl_gctime;
	return (0);
}

/*
 * This looks to see if the specified file exists in the cache.
 * 	0 is returned if it exists
 *	ENOENT is returned if it doesn't exist.
 */
int
/*ARGSUSED*/
cachefs_io_exists(vnode_t *vp, void *dinp, void *doutp)
{
	fscache_t  *fscp = C_TO_FSCACHE(VTOC(vp));
	cnode_t *cp = NULL;
	int error;
	cfs_cid_t *cidp = (cfs_cid_t *)dinp;

	/* find the cnode of the file */
	error = cachefs_cnode_make(cidp, fscp,
	    NULL, NULL, NULL, kcred, 0, &cp);
	if (error)
		return (ENOENT);

	if ((cp->c_flags & (CN_DESTROY | CN_NOCACHE)) ||
	    !(cp->c_metadata.md_flags & (MD_POPULATED | MD_FASTSYMLNK)))
		error = ENOENT;

	VN_RELE(CTOV(cp));
	return	(error);

}

/*
 * Moves the specified file to the lost+found directory for the
 * cached file system.
 * Invalidates cached data and attributes.
 * Returns 0 or an error if could not perform operation.
 */
int
cachefs_io_lostfound(vnode_t *vp, void *dinp, void *doutp)
{
	int error;
	cnode_t *cp = NULL;
	fscache_t *fscp;
	cachefscache_t *cachep;
	cachefsio_lostfound_arg_t *lfp;
	cachefsio_lostfound_return_t *rp;

	lfp = (cachefsio_lostfound_arg_t *)dinp;
	rp = (cachefsio_lostfound_return_t *)doutp;

	fscp = C_TO_FSCACHE(VTOC(vp));
	cachep = fscp->fs_cache;

	ASSERT((cachep->c_flags & (CACHE_NOCACHE|CACHE_NOFILL)) == 0);

	/* find the cnode of the file */
	error = cachefs_cnode_make(&lfp->lf_cid, fscp,
	    NULL, NULL, NULL, kcred, 0, &cp);
	if (error) {
		error = ENOENT;
		goto out;
	}

	cachefs_mutex_enter(&cp->c_statelock);

	/* must be regular file and modified */
	if ((cp->c_attr.va_type != VREG) ||
	    (cp->c_metadata.md_rltype != CACHEFS_RL_MODIFIED)) {
		cachefs_mutex_exit(&cp->c_statelock);
		error = EINVAL;
		goto out;
	}
	ASSERT(cp->c_metadata.md_flags & MD_POPULATED);

	/* move to lost+found */
	error = cachefs_cnode_lostfound(cp, lfp->lf_name);
	cachefs_mutex_exit(&cp->c_statelock);

	if (error == 0)
		strcpy(rp->lf_name, lfp->lf_name);
out:
	if (cp)
		VN_RELE(CTOV(cp));

	return (error);
}

/*
 * Given a cid, returns info about the file in the cache.
 */
int
cachefs_io_getinfo(vnode_t *vp, void *dinp, void *doutp)
{
	fscache_t *fscp = C_TO_FSCACHE(VTOC(vp));
	struct cnode *dcp = NULL;
	struct cnode *cp = NULL;
	struct vattr va;
	caddr_t	addr;
	u_offset_t blockoff = 0;
	int offset = 0;
	int error = 0;
	cfs_cid_t *fcidp;
	cachefsio_getinfo_t *infop;

	fcidp = (cfs_cid_t *)dinp;
	infop = (cachefsio_getinfo_t *)doutp;

	/* find the cnode of the file */
	error = cachefs_cnode_make(fcidp, fscp, NULL, NULL, NULL,
	    kcred, 0, &cp);
	if (error) {
		error = ENOENT;
		goto out;
	}

	infop->gi_cid = *fcidp;
	infop->gi_modified = (cp->c_metadata.md_rltype == CACHEFS_RL_MODIFIED);
	infop->gi_attr = cp->c_attr;
	infop->gi_pcid = cp->c_metadata.md_parent;
	infop->gi_name[0] = '\0';
	infop->gi_seq = cp->c_metadata.md_seq;
	if (cp->c_metadata.md_parent.cid_fileno == 0)
		goto out;

	/* try to get the cnode of the parent dir */
	error = cachefs_cnode_make(&cp->c_metadata.md_parent, fscp,
	    NULL, NULL, NULL, kcred, 0, &dcp);
	if (error) {
		error = 0;
		goto out;
	}

	/* make sure a directory and populated */
	if ((((dcp->c_flags & CN_ASYNC_POPULATE) == 0) ||
	    ((dcp->c_metadata.md_flags & MD_POPULATED) == 0)) &&
	    (CTOV(dcp)->v_type == VDIR)) {
		error = 0;
		goto out;
	}

	/* get the front file */
	if (dcp->c_frontvp == NULL) {
		cachefs_mutex_enter(&dcp->c_statelock);
		error = cachefs_getfrontfile(dcp);
		cachefs_mutex_exit(&dcp->c_statelock);
		if (error) {
			error = 0;
			goto out;
		}

		/* make sure frontvp is still populated */
		if ((dcp->c_metadata.md_flags & MD_POPULATED) == 0) {
			error = 0;
			goto out;
		}
	}

	/* Get the length of the directory */
	va.va_mask = AT_SIZE;
	error = VOP_GETATTR(dcp->c_frontvp, &va, 0, kcred);
	if (error) {
		error = 0;
		goto out;
	}

	/* XXX bob: change this to use cachfs_dir_read */
	/* We have found the parent, now we open the dir and look for file */
	while (blockoff < va.va_size) {
		offset = 0;
		addr = segmap_getmap(segkmap, dcp->c_frontvp, blockoff);
		while (offset < MAXBSIZE && (blockoff + offset) < va.va_size) {
			struct c_dirent	*dep;
			dep = (struct c_dirent *)(addr + offset);
			if ((dep->d_flag & CDE_VALID) &&
			    (bcmp((caddr_t)&dep->d_id, (caddr_t)&infop->gi_cid,
			    sizeof (cfs_cid_t)) == 0)) {
				/* found the name */
				strcpy(infop->gi_name, dep->d_name);
				(void) segmap_release(segkmap, addr, 0);
				goto out;
			}
			offset += dep->d_length;
		}
		(void) segmap_release(segkmap, addr, 0);
		addr = NULL;
		blockoff += MAXBSIZE;

	}
out:
	if (cp)
		VN_RELE(CTOV(cp));
	if (dcp)
		VN_RELE(CTOV(dcp));
	return (error);
}

/*
 * Given a file number, this functions returns the fid
 * for the back file system.
 * Returns ENOENT if file does not exist.
 * Returns ENOMSG if fid is not valid, ie: local file.
 */
int
cachefs_io_cidtofid(vnode_t *vp, void *dinp, void *doutp)
{
	fscache_t *fscp = C_TO_FSCACHE(VTOC(vp));
	cnode_t *cp = NULL;
	int error;
	cfs_cid_t *cidp = (cfs_cid_t *)dinp;
	fid_t *fidp = (fid_t *)doutp;

	/* get the cnode for the file */
	error = cachefs_cnode_make(cidp, fscp, NULL, NULL, NULL, kcred, 0, &cp);
	if (error)
		goto out;

	/* if local file, fid is a local fid and is not valid */
	if (cp->c_id.cid_flags & CFS_CID_LOCAL) {
		error = ENOMSG;
		goto out;
	}

	/* copy out the fid */
	*fidp = cp->c_cookie;

out:
	if (cp)
		VN_RELE(CTOV(cp));
	return	(error);
}

/*
 * This performs a getattr on the back file system given
 * a fid that is passed in.
 *
 * The backfid is in gafid->cg_backfid, the creds to use for
 * this operation are in gafid->cg_cred.  The attributes are
 * returned in gafid->cg_attr
 *
 * the error returned is 0 if successful, nozero if not
 */
int
cachefs_io_getattrfid(vnode_t *vp, void *dinp, void *doutp)
{
	vnode_t	*backvp = NULL;
	fscache_t  *fscp = C_TO_FSCACHE(VTOC(vp));
	int error = 0;
	cred_t	*cr;
	cachefsio_getattrfid_t *gafid;
	vattr_t *attrp;

	gafid = (cachefsio_getattrfid_t *)dinp;
	attrp = (vattr_t *)doutp;

	/* Get a vnode for the back file */
	error = VFS_VGET(fscp->fs_backvfsp, &backvp, &gafid->cg_backfid);
	if (error)
		return (error);

	attrp->va_mask = AT_ALL;
	cr = crdup(&gafid->cg_cred);
	error = VOP_GETATTR(backvp, attrp, 0, cr);
	crfree(cr);

	/* VFS_VGET performs a VN_HOLD on the vp */
	VN_RELE(backvp);

	return (error);
}


/*
 * This performs a getattr on the back file system.  Instead of
 * passing the fid to perform the gettr on we are given the
 * parent directory fid and a name.
 */
int
cachefs_io_getattrname(vnode_t *vp, void *dinp, void *doutp)
{
	vnode_t	*pbackvp = NULL;
	vnode_t	*cbackvp = NULL;
	fscache_t  *fscp = C_TO_FSCACHE(VTOC(vp));
	int error = 0;
	cred_t	*cr;
	cachefsio_getattrname_arg_t *gap;
	cachefsio_getattrname_return_t *retp;

	gap = (cachefsio_getattrname_arg_t *)dinp;
	retp = (cachefsio_getattrname_return_t *)doutp;

	/* Get a vnode for the parent directory */
	error = VFS_VGET(fscp->fs_backvfsp, &pbackvp, &gap->cg_dir);
	if (error)
		return (error);

	/* lookup the file name */
	cr = crdup(&gap->cg_cred);
	error = VOP_LOOKUP(pbackvp, gap->cg_name, &cbackvp,
	    (struct pathname *)NULL, 0, (vnode_t *)NULL, cr);
	if (error) {
		crfree(cr);
		VN_RELE(pbackvp);
		return (error);
	}

	retp->cg_attr.va_mask = AT_ALL;
	error = VOP_GETATTR(cbackvp, &retp->cg_attr, 0, cr);
	if (!error) {
		retp->cg_fid.fid_len = MAXFIDSZ;
		error = VOP_FID(cbackvp, &retp->cg_fid);
	}

	crfree(cr);
	VN_RELE(cbackvp);
	VN_RELE(pbackvp);
	return (error);
}

/*
 * This will return the fid of the root of this mount point.
 */
int
/*ARGSUSED*/
cachefs_io_rootfid(vnode_t *vp, void *dinp, void *doutp)
{
	fscache_t  *fscp = C_TO_FSCACHE(VTOC(vp));
	fid_t *rootfid = (fid_t *)doutp;

	*rootfid = VTOC(fscp->fs_rootvp)->c_metadata.md_cookie;
	return (0);
}

/*
 * Pushes the data associated with a file back to the file server.
 */
int
cachefs_io_pushback(vnode_t *vp, void *dinp, void *doutp)
{
	vnode_t *backvp = NULL;
	fscache_t *fscp = C_TO_FSCACHE(VTOC(vp));
	caddr_t	buffer = NULL;
	int error = 0;
	cnode_t	*cp;
	u_int amt;
	u_offset_t size;
	vattr_t	va;
	offset_t off;
	cred_t *cr = NULL;
	cachefsio_pushback_arg_t *pbp;
	cachefsio_pushback_return_t *retp;

	pbp = (cachefsio_pushback_arg_t *)dinp;
	retp = (cachefsio_pushback_return_t *)doutp;

	cr = crdup(&pbp->pb_cred);

	/* get the backvp to push to */
	error = VFS_VGET(fscp->fs_backvfsp, &backvp, &pbp->pb_fid);
	if (error) {
		backvp = NULL;
		goto out;
	}

	/* Get the cnode for the file we are to push back */
	error = cachefs_cnode_make(&pbp->pb_cid, fscp,
	    NULL, NULL, NULL, cr, 0, &cp);
	if (error) {
		goto out;
	}

	/* must be a regular file */
	if (cp->c_attr.va_type != VREG) {
		error = EINVAL;
		goto out;
	}

	cachefs_mutex_enter(&cp->c_statelock);

	/* get the front file */
	if (cp->c_frontvp == NULL) {
		error = cachefs_getfrontfile(cp);
		if (error) {
			cachefs_mutex_exit(&cp->c_statelock);
			goto out;
		}
	}

	/* better be populated */
	if ((cp->c_metadata.md_flags & MD_POPULATED) == 0) {
		cachefs_mutex_exit(&cp->c_statelock);
		error = EINVAL;
		goto out;
	}

	/* do open so NFS gets correct creds on writes */
	error = VOP_OPEN(&backvp, FWRITE, cr);
	if (error) {
		cachefs_mutex_exit(&cp->c_statelock);
		goto out;
	}

	buffer = cachefs_kmem_alloc(MAXBSIZE, KM_SLEEP);

	/* Read the data from the cache and write it to the server */
	/* XXX why not use segmapio? */
	off = 0;
	for (size = cp->c_size; size != 0; size -= amt) {
		if (size > MAXBSIZE)
			amt = MAXBSIZE;
		else
			amt = size;

		/* read a block of data from the front file */
		error = vn_rdwr(UIO_READ, cp->c_frontvp, buffer,
			amt, off, UIO_SYSSPACE, 0, RLIM_INFINITY, cr, 0);
		if (error) {
			cachefs_mutex_exit(&cp->c_statelock);
			goto out;
		}

		/* write the block of data to the back file */
		error = vn_rdwr(UIO_WRITE, backvp, buffer, amt, off,
			UIO_SYSSPACE, 0, RLIM_INFINITY, cr, 0);
		if (error) {
			cachefs_mutex_exit(&cp->c_statelock);
			goto out;
		}
		off += amt;
	}

	error = VOP_FSYNC(backvp, FSYNC, cr);
	if (error == 0)
		error = VOP_CLOSE(backvp, FWRITE, 0, (offset_t)0, cr);
	if (error) {
		cachefs_mutex_exit(&cp->c_statelock);
		goto out;
	}

	cp->c_metadata.md_flags |= MD_PUSHDONE;
	cp->c_metadata.md_flags &= ~MD_PUTPAGE;
	cp->c_metadata.md_flags |= MD_NEEDATTRS;
	cp->c_flags |= CN_UPDATED;
	cachefs_mutex_exit(&cp->c_statelock);

	/*
	 * if we have successfully stored the data, we need the
	 * new ctime and mtimes.
	 */
	va.va_mask = AT_ALL;
	error = VOP_GETATTR(backvp, &va, 0, cr);
	if (error)
		goto out;
	retp->pb_ctime = va.va_ctime;
	retp->pb_mtime = va.va_mtime;

out:
	if (buffer)
		cachefs_kmem_free(buffer, MAXBSIZE);
	if (cp)
		VN_RELE(CTOV(cp));
	if (backvp)
		VN_RELE(backvp);
	if (cr)
		crfree(cr);
	return (error);
}

/*
 * Create a file on the back file system.
 */
int
cachefs_io_create(vnode_t *vp, void *dinp, void *doutp)
{
	vnode_t	*dvp = NULL;
	vnode_t	*cvp = NULL;
	cnode_t *cp = NULL;
	fscache_t *fscp = C_TO_FSCACHE(VTOC(vp));
	vattr_t	va;
	int error = 0;
	cred_t *cr = NULL;
	cachefsio_create_arg_t *crp;
	cachefsio_create_return_t *retp;

	crp = (cachefsio_create_arg_t *)dinp;
	retp = (cachefsio_create_return_t *)doutp;

	/* get a vnode for the parent directory  */
	error = VFS_VGET(fscp->fs_backvfsp, &dvp, &crp->cr_backfid);
	if (error)
		goto out;

	cr = crdup(&crp->cr_cred);

	/* do the create */
	error = VOP_CREATE(dvp, crp->cr_name, &crp->cr_va,
	    crp->cr_exclusive, crp->cr_mode, &cvp, cr, 0);
	if (error)
		goto out;

	/* get the fid of the file */
	retp->cr_newfid.fid_len = MAXFIDSZ;
	error = VOP_FID(cvp, &retp->cr_newfid);
	if (error)
		goto out;

	/* get attributes for the file */
	va.va_mask = AT_ALL;
	error = VOP_GETATTR(cvp, &va, 0, cr);
	if (error)
		goto out;
	retp->cr_ctime = va.va_ctime;
	retp->cr_mtime = va.va_mtime;

	/* update the cnode for this file with the new info */
	error = cachefs_cnode_make(&crp->cr_cid, fscp,
	    NULL, NULL, NULL, cr, 0, &cp);
	if (error) {
		error = 0;
		goto out;
	}

	cachefs_mutex_enter(&cp->c_statelock);
	ASSERT(cp->c_id.cid_flags & CFS_CID_LOCAL);
	cp->c_attr.va_nodeid = va.va_nodeid;
	cp->c_metadata.md_flags |= MD_CREATEDONE;
	cp->c_metadata.md_flags |= MD_NEEDATTRS;
	cp->c_metadata.md_cookie = retp->cr_newfid;
	cp->c_flags |= CN_UPDATED;
	cachefs_mutex_exit(&cp->c_statelock);

out:
	if (cr)
		crfree(cr);
	if (dvp)
		VN_RELE(dvp);
	if (cvp)
		VN_RELE(cvp);
	if (cp)
		VN_RELE(CTOV(cp));
	return (error);
}

/*
 * Remove a file on the back file system.
 * Returns 0 or an error if could not perform operation.
 */
int
cachefs_io_remove(vnode_t *vp, void *dinp, void *doutp)
{
	vnode_t	*dvp = NULL;
	vnode_t	*cvp;
	cred_t *cr = NULL;
	vattr_t	va;
	fscache_t  *fscp = C_TO_FSCACHE(VTOC(vp));
	int error;
	fid_t child_fid;
	cachefsio_remove_t *rmp = (cachefsio_remove_t *)dinp;
	timestruc_t *ctimep = (timestruc_t *)doutp;

	/* Get a vnode for the directory */
	error = VFS_VGET(fscp->fs_backvfsp, &dvp, &rmp->rm_fid);
	if (error) {
		dvp = NULL;
		goto out;
	}

	cr = crdup(&rmp->rm_cred);

	/* if the caller wants the ctime after the remove */
	if (ctimep) {
		error = VOP_LOOKUP(dvp, rmp->rm_name, &cvp, NULL, 0, NULL, cr);
		if (error == 0) {
			child_fid.fid_len = MAXFIDSZ;
			error = VOP_FID(cvp, &child_fid);
			VN_RELE(cvp);
		}
		if (error)
			goto out;
	}

	/* do the remove */
	error = VOP_REMOVE(dvp, rmp->rm_name, cr);
	if (error)
		goto out;

	/* get the new ctime if requested */
	if (ctimep) {
		error = VFS_VGET(fscp->fs_backvfsp, &cvp, &child_fid);
		if (error == 0) {
			va.va_mask = AT_ALL;
			error = VOP_GETATTR(cvp, &va, 0, cr);
			if (error == 0) {
				*ctimep = va.va_ctime;
			}
			VN_RELE(cvp);
		}
		cachefs_iosetneedattrs(fscp, &rmp->rm_cid);
	}

out:
	if (cr)
		crfree(cr);
	if (dvp)
		VN_RELE(dvp);
	return (error);
}

/*
 * Perform a link on the back file system.
 * Returns 0 or an error if could not perform operation.
 */
int
cachefs_io_link(vnode_t *vp, void *dinp, void *doutp)
{
	vnode_t	*dvp = NULL;
	vnode_t	*lvp = NULL;
	vattr_t	va;
	fscache_t *fscp = C_TO_FSCACHE(VTOC(vp));
	int error = 0;
	cred_t *cr = NULL;
	cachefsio_link_t *linkp = (cachefsio_link_t *)dinp;
	timestruc_t *ctimep = (timestruc_t *)doutp;

	/* Get a vnode parent directory */
	error = VFS_VGET(fscp->fs_backvfsp, &dvp, &linkp->ln_dirfid);
	if (error) {
		dvp = NULL;
		goto out;
	}

	/* Get a vnode file to link to */
	error = VFS_VGET(fscp->fs_backvfsp, &lvp, &linkp->ln_filefid);
	if (error) {
		lvp = NULL;
		goto out;
	}

	cr = crdup(&linkp->ln_cred);

	/* do the link */
	error = VOP_LINK(dvp, lvp, linkp->ln_name, cr);
	if (error)
		goto out;

	/* get the ctime */
	va.va_mask = AT_ALL;
	error = VOP_GETATTR(lvp, &va, 0, cr);
	if (error)
		goto out;
	*ctimep = va.va_ctime;

	cachefs_iosetneedattrs(fscp, &linkp->ln_cid);
out:
	if (cr)
		crfree(cr);
	if (dvp)
		VN_RELE(dvp);
	if (lvp)
		VN_RELE(lvp);
	return (error);
}

/*
 * Rename the file on the back file system.
 * Returns 0 or an error if could not perform operation.
 */
int
cachefs_io_rename(vnode_t *vp, void *dinp, void *doutp)
{
	vnode_t	*odvp = NULL;
	vnode_t	*ndvp = NULL;
	cred_t *cr = NULL;
	vnode_t	*cvp = NULL;
	vattr_t va;
	fscache_t  *fscp = C_TO_FSCACHE(VTOC(vp));
	int error = 0;
	fid_t child_fid;
	cachefsio_rename_arg_t *rnp;
	cachefsio_rename_return_t *retp;

	rnp = (cachefsio_rename_arg_t *)dinp;
	retp = (cachefsio_rename_return_t *)doutp;

	/* Get vnode of old parent directory */
	error = VFS_VGET(fscp->fs_backvfsp, &odvp, &rnp->rn_olddir);
	if (error) {
		odvp = NULL;
		goto out;
	}

	/* Get vnode of new parent directory */
	error = VFS_VGET(fscp->fs_backvfsp, &ndvp, &rnp->rn_newdir);
	if (error) {
		ndvp = NULL;
		goto out;
	}

	cr = crdup(&rnp->rn_cred);

	/* if the caller wants the ctime of the target after deletion */
	if (rnp->rn_del_getctime) {
		error = VOP_LOOKUP(ndvp, rnp->rn_newname, &cvp, NULL, 0,
		    NULL, cr);
		if (error) {
			cvp = NULL; /* paranoia */
			goto out;
		}

		child_fid.fid_len = MAXFIDSZ;
		error = VOP_FID(cvp, &child_fid);
		if (error)
			goto out;
		VN_RELE(cvp);
		cvp = NULL;
	}

	/* do the rename */
	error = VOP_RENAME(odvp, rnp->rn_oldname, ndvp, rnp->rn_newname, cr);
	if (error)
		goto out;

	/* get the new ctime on the renamed file */
	error = VOP_LOOKUP(ndvp, rnp->rn_newname, &cvp, NULL, 0, NULL, cr);
	if (error)
		goto out;

	va.va_mask = AT_ALL;
	error = VOP_GETATTR(cvp, &va, 0, cr);
	if (error)
		goto out;
	retp->rn_ctime = va.va_ctime;
	VN_RELE(cvp);
	cvp = NULL;

	cachefs_iosetneedattrs(fscp, &rnp->rn_cid);

	/* get the new ctime if requested of the deleted target */
	if (rnp->rn_del_getctime) {
		error = VFS_VGET(fscp->fs_backvfsp, &cvp, &child_fid);
		if (error) {
			cvp = NULL;
			goto out;
		}
		va.va_mask = AT_ALL;
		error = VOP_GETATTR(cvp, &va, 0, cr);
		if (error)
			goto out;
		retp->rn_del_ctime = va.va_ctime;
		VN_RELE(cvp);
		cvp = NULL;
		cachefs_iosetneedattrs(fscp, &rnp->rn_del_cid);
	}

out:
	if (cr)
		crfree(cr);
	if (cvp)
		VN_RELE(cvp);
	if (odvp)
		VN_RELE(odvp);
	if (ndvp)
		VN_RELE(ndvp);
	return (error);
}

/*
 * Make a directory on the backfs.
 * Returns 0 or an error if could not perform operation.
 */
int
cachefs_io_mkdir(vnode_t *vp, void *dinp, void *doutp)
{
	vnode_t	*dvp = NULL;
	vnode_t	*cvp = NULL;
	cnode_t *cp = NULL;
	fscache_t *fscp = C_TO_FSCACHE(VTOC(vp));
	int error = 0;
	cred_t *cr = NULL;
	vattr_t va;
	cachefsio_mkdir_t *mdirp = (cachefsio_mkdir_t *)dinp;
	fid_t *fidp = (fid_t *)doutp;

	/* Get vnode of parent directory */
	error = VFS_VGET(fscp->fs_backvfsp, &dvp, &mdirp->md_dirfid);
	if (error) {
		dvp = NULL;
		goto out;
	}

	cr = crdup(&mdirp->md_cred);

	/* make the directory */
	error = VOP_MKDIR(dvp, mdirp->md_name, &mdirp->md_vattr, &cvp, cr);
	if (error) {
		if (error != EEXIST)
			goto out;

		/* if the directory already exists, then use it */
		error = VOP_LOOKUP(dvp, mdirp->md_name, &cvp,
		    NULL, 0, NULL, cr);
		if (error) {
			cvp = NULL;
			goto out;
		}
		if (cvp->v_type != VDIR) {
			error = EINVAL;
			goto out;
		}
	}

	/* get the fid of the directory */
	fidp->fid_len = MAXFIDSZ;
	error = VOP_FID(cvp, fidp);
	if (error)
		goto out;

	/* get attributes of the directory */
	va.va_mask = AT_ALL;
	error = VOP_GETATTR(cvp, &va, 0, cr);
	if (error)
		goto out;

	/* update the cnode for this dir with the new fid */
	error = cachefs_cnode_make(&mdirp->md_cid, fscp,
	    NULL, NULL, NULL, cr, 0, &cp);
	if (error) {
		error = 0;
		goto out;
	}
	cachefs_mutex_enter(&cp->c_statelock);
	ASSERT(cp->c_id.cid_flags & CFS_CID_LOCAL);
	cp->c_metadata.md_cookie = *fidp;
	cp->c_metadata.md_flags |= MD_CREATEDONE;
	cp->c_metadata.md_flags |= MD_NEEDATTRS;
	cp->c_attr.va_nodeid = va.va_nodeid;
	cp->c_flags |= CN_UPDATED;
	cachefs_mutex_exit(&cp->c_statelock);
out:
	if (cr)
		crfree(cr);
	if (dvp)
		VN_RELE(dvp);
	if (cvp)
		VN_RELE(cvp);
	if (cp)
		VN_RELE(CTOV(cp));
	return (error);
}

/*
 * Perform a rmdir on the back file system.
 * Returns 0 or an error if could not perform operation.
 */
int
/*ARGSUSED*/
cachefs_io_rmdir(vnode_t *vp, void *dinp, void *doutp)
{
	vnode_t	*dvp = NULL;
	fscache_t *fscp = C_TO_FSCACHE(VTOC(vp));
	int error;
	cred_t *cr;
	cachefsio_rmdir_t *rdp = (cachefsio_rmdir_t *)dinp;

	/* Get a vnode for the back file */
	error = VFS_VGET(fscp->fs_backvfsp, &dvp, &rdp->rd_dirfid);
	if (error) {
		dvp = NULL;
		return (error);
	}

	cr = crdup(&rdp->rd_cred);
	error = VOP_RMDIR(dvp, rdp->rd_name, dvp, cr);
	crfree(cr);

	VN_RELE(dvp);
	return (error);
}

/*
 * create a symlink on the back file system
 * Returns 0 or an error if could not perform operation.
 */
int
cachefs_io_symlink(vnode_t *vp, void *dinp, void *doutp)
{
	vnode_t	*dvp = NULL;
	vnode_t	*svp = NULL;
	cnode_t *cp = NULL;
	fscache_t *fscp = C_TO_FSCACHE(VTOC(vp));
	vattr_t	va;
	int error = 0;
	cred_t *cr = NULL;
	cachefsio_symlink_arg_t *symp;
	cachefsio_symlink_return_t *retp;

	symp = (cachefsio_symlink_arg_t *)dinp;
	retp = (cachefsio_symlink_return_t *)doutp;

	/* get a vnode for the back directory */
	error = VFS_VGET(fscp->fs_backvfsp, &dvp, &symp->sy_dirfid);
	if (error) {
		dvp = NULL;
		goto out;
	}

	cr = crdup(&symp->sy_cred);

	/* create the symlink */
	error = VOP_SYMLINK(dvp, symp->sy_name, &symp->sy_vattr,
	    symp->sy_link, cr);
	if (error)
		goto out;

	/* get the vnode for the symlink */
	error = VOP_LOOKUP(dvp, symp->sy_name, &svp, NULL, 0, NULL, cr);
	if (error)
		goto out;

	/* get the attributes of the symlink */
	va.va_mask = AT_ALL;
	error = VOP_GETATTR(svp, &va, 0, cr);
	if (error)
		goto out;
	retp->sy_ctime = va.va_ctime;
	retp->sy_mtime = va.va_mtime;

	/* get the fid */
	retp->sy_newfid.fid_len = MAXFIDSZ;
	error = VOP_FID(svp, &retp->sy_newfid);
	if (error)
		goto out;

	/* update the cnode for this file with the new info */
	error = cachefs_cnode_make(&symp->sy_cid, fscp,
	    NULL, NULL, NULL, cr, 0, &cp);
	if (error) {
		error = 0;
		goto out;
	}
	cachefs_mutex_enter(&cp->c_statelock);
	ASSERT(cp->c_id.cid_flags & CFS_CID_LOCAL);
	cp->c_metadata.md_cookie = retp->sy_newfid;
	cp->c_metadata.md_flags |= MD_CREATEDONE;
	cp->c_metadata.md_flags |= MD_NEEDATTRS;
	cp->c_attr.va_nodeid = va.va_nodeid;
	cp->c_flags |= CN_UPDATED;
	cachefs_mutex_exit(&cp->c_statelock);

out:
	if (cr)
		crfree(cr);
	if (dvp)
		VN_RELE(dvp);
	if (svp)
		VN_RELE(svp);
	if (cp)
		VN_RELE(CTOV(cp));
	return (error);
}

/*
 * Perform setattr on the back file system.
 * Returns 0 or an error if could not perform operation.
 */
int
cachefs_io_setattr(vnode_t *vp, void *dinp, void *doutp)
{
	vnode_t	*cvp = NULL;
	fscache_t *fscp = C_TO_FSCACHE(VTOC(vp));
	vattr_t	va;
	int error = 0;
	cred_t *cr = NULL;
	cachefsio_setattr_arg_t *sap;
	cachefsio_setattr_return_t *retp;

	sap = (cachefsio_setattr_arg_t *)dinp;
	retp = (cachefsio_setattr_return_t *)doutp;

	/* get a vnode for the back dir */
	error = VFS_VGET(fscp->fs_backvfsp, &cvp, &sap->sa_backfid);
	if (error) {
		cvp = NULL;
		goto out;
	}

	cr = crdup(&sap->sa_cred);

	/* perform the setattr */
	error = VOP_SETATTR(cvp, &sap->sa_vattr, 0, cr);
	if (error)
		goto out;

	/* get the new ctime and mtime */
	va.va_mask = AT_ALL;
	error = VOP_GETATTR(cvp, &va, 0, cr);
	if (error)
		goto out;
	retp->sa_ctime = va.va_ctime;
	retp->sa_mtime = va.va_mtime;

	cachefs_iosetneedattrs(fscp, &sap->sa_cid);
out:
	if (cr)
		crfree(cr);
	if (cvp)
		VN_RELE(cvp);
	return (error);
}

int
cachefs_io_setsecattr(vnode_t *vp, void *dinp, void *doutp)
{
	int error = 0;
	fscache_t *fscp = C_TO_FSCACHE(VTOC(vp));
	vnode_t *tvp = NULL;
	vsecattr_t vsec;
	vattr_t va;
	cred_t *cr = NULL;
	cachefsio_setsecattr_arg_t *ssap;
	cachefsio_setsecattr_return_t *retp;

	ssap = (cachefsio_setsecattr_arg_t *)dinp;
	retp = (cachefsio_setsecattr_return_t *)doutp;

	/* get vnode of back file to do VOP_SETSECATTR to */
	error = VFS_VGET(fscp->fs_backvfsp, &tvp, &ssap->sc_backfid);
	if (error != 0) {
		tvp = NULL;
		goto out;
	}

	/* get the creds */
	cr = crdup(&ssap->sc_cred);

	/* form the vsecattr_t */
	vsec.vsa_mask = ssap->sc_mask;
	vsec.vsa_aclcnt = ssap->sc_aclcnt;
	vsec.vsa_dfaclcnt = ssap->sc_dfaclcnt;
	vsec.vsa_aclentp = ssap->sc_acl;
	vsec.vsa_dfaclentp = ssap->sc_acl + ssap->sc_aclcnt;

	/* set the ACL */
	VOP_RWLOCK(tvp, 1);
	error = VOP_SETSECATTR(tvp, &vsec, 0, cr);
	VOP_RWUNLOCK(tvp, 1);
	if (error != 0)
		goto out;

	/* get the new ctime and mtime */
	va.va_mask = AT_ALL;
	error = VOP_GETATTR(tvp, &va, 0, cr);
	if (error)
		goto out;
	retp->sc_ctime = va.va_ctime;
	retp->sc_mtime = va.va_mtime;

	cachefs_iosetneedattrs(fscp, &ssap->sc_cid);
out:

	if (cr != NULL)
		crfree(cr);
	if (tvp != NULL)
		VN_RELE(tvp);

	return (error);
}

static void
sync_metadata(cnode_t *cp)
{
	if (cp->c_flags & (CN_STALE | CN_DESTROY))
		return;
	(void) cachefs_sync_metadata(cp);
}

static void
drop_backvp(cnode_t *cp)
{
	if (cp->c_backvp) {
		cachefs_mutex_enter(&cp->c_statelock);
		if (cp->c_backvp) {
			/* dump any pages, may be a dirty one */
			(void) VOP_PUTPAGE(cp->c_backvp, (offset_t)0, 0,
			    B_INVAL | B_TRUNC, kcred);
			VN_RELE(cp->c_backvp);
			cp->c_backvp = NULL;
		}
		cachefs_mutex_exit(&cp->c_statelock);
	}
}

static void
allow_pendrm(cnode_t *cp)
{
	if (cp->c_flags & CN_PENDRM) {
		cachefs_mutex_enter(&cp->c_statelock);
		if (cp->c_flags & CN_PENDRM) {
			cp->c_flags &= ~CN_PENDRM;
			VN_RELE(CTOV(cp));
		}
		cachefs_mutex_exit(&cp->c_statelock);
	}
}

static void
cachefs_modified_fix(fscache_t *fscp)
{
	cnode_t *cp;
	int error = 0;
	rl_entry_t rl_ent;
	cfs_cid_t cid;
	cachefscache_t *cachep = fscp->fs_cache;
	enum cachefs_rl_type type;
	cachefs_metadata_t *mdp;
	int timedout = 0;
	struct vattr va;

	/* XXX just return if fs is in error ro mode */

	/* lock out other users of the MF list */
	cachefs_mutex_enter(&cachep->c_mflock);

	/* move the modified entries for this file system to the MF list */
	cachefs_move_modified_to_mf(cachep, fscp);

	rl_ent.rl_current = CACHEFS_RL_MF;
	for (;;) {
		/* get the next entry on the MF list */
		error = cachefs_rlent_data(cachep, &rl_ent, NULL);
		if (error) {
			error = 0;
			break;
		}
		ASSERT(fscp->fs_cfsid == rl_ent.rl_fsid);

		/* get the cnode for the file */
		cid.cid_fileno = rl_ent.rl_fileno;
		cid.cid_flags = rl_ent.rl_local ? CFS_CID_LOCAL : 0;
		error = cachefs_cnode_make(&cid, fscp,
		    NULL, NULL, NULL, kcred, 0, &cp);
		if (error) {
#ifdef CFSDEBUG
			CFS_DEBUG(CFSDEBUG_IOCTL)
				printf("cachefs: mf: could not find %lu\n",
				    cid.cid_fileno);
			delay(5*hz);
#endif
			/* XXX this will loop forever, maybe put fs in */
			/*   ro mode */
			continue;
		}

		cachefs_mutex_enter(&cp->c_statelock);

		mdp = &cp->c_metadata;

		/* if a regular file that has not been pushed */
		if ((cp->c_attr.va_type == VREG) &&
		    ((mdp->md_flags & (MD_PUSHDONE | MD_PUTPAGE) ==
		    MD_PUTPAGE))) {
			/* move the file to lost+found */
			error = cachefs_cnode_lostfound(cp, NULL);
			if (error) {
				/* XXX put fs in ro mode */
				/* XXX need to drain MF list */
				panic("lostfound failed %d", error);
			}
			cachefs_mutex_exit(&cp->c_statelock);
			VN_RELE(CTOV(cp));
			continue;
		}

		/* if a local file */
		if (cp->c_id.cid_flags & CFS_CID_LOCAL) {
			/* if the file was not created */
			if ((cp->c_metadata.md_flags & MD_CREATEDONE) == 0) {
				/* do not allow cnode to be used */
				cachefs_cnode_stale(cp);
				cachefs_mutex_exit(&cp->c_statelock);
				VN_RELE(CTOV(cp));
				continue;
			}

			/* save the local fileno for later getattrs */
			mdp->md_localfileno = cp->c_id.cid_fileno;
			cachefs_mutex_exit(&cp->c_statelock);

			/* register the mapping from old to new fileno */
			cachefs_mutex_enter(&fscp->fs_fslock);
			cachefs_inum_register(fscp, cp->c_attr.va_nodeid,
			    mdp->md_localfileno);
			cachefs_inum_register(fscp, mdp->md_localfileno, 0);
			cachefs_mutex_exit(&fscp->fs_fslock);

			/* move to new location in the cache */
			cachefs_cnode_move(cp);
			cachefs_mutex_enter(&cp->c_statelock);
		}

		/* else if a modified file that needs to have its mode fixed */
		else if ((cp->c_metadata.md_flags & MD_FILE) &&
		    (cp->c_attr.va_type == VREG)) {

			if (cp->c_frontvp == NULL)
				(void) cachefs_getfrontfile(cp);
			if (cp->c_frontvp) {
				/* mark file as no longer modified */
				va.va_mode = 0666;
				va.va_mask = AT_MODE;
				error = VOP_SETATTR(cp->c_frontvp, &va,
				    0, kcred);
				if (error) {
					cmn_err(CE_WARN,
					    "Cannot change ff mode.\n");
				}
			}
		}


		/* if there is a rl entry, put it on the correct list */
		if (mdp->md_rlno) {
			if (mdp->md_flags & MD_PACKED) {
				if ((mdp->md_flags & MD_POPULATED) ||
				    ((mdp->md_flags & MD_FILE) == 0))
					type = CACHEFS_RL_PACKED;
				else
					type = CACHEFS_RL_PACKED_PENDING;
				cachefs_rlent_moveto(fscp->fs_cache, type,
				    mdp->md_rlno, mdp->md_frontblks);
				mdp->md_rltype = type;
			} else if (mdp->md_flags & MD_FILE) {
				type = CACHEFS_RL_ACTIVE;
				cachefs_rlent_moveto(fscp->fs_cache, type,
				    mdp->md_rlno, mdp->md_frontblks);
				mdp->md_rltype = type;
			} else {
				type = CACHEFS_RL_FREE;
				cachefs_rlent_moveto(fscp->fs_cache, type,
				    mdp->md_rlno, 0);
				filegrp_ffrele(cp->c_filegrp);
				mdp->md_rlno = 0;
				mdp->md_rltype = CACHEFS_RL_NONE;
			}
		}
		mdp->md_flags &= ~(MD_CREATEDONE | MD_PUTPAGE |
		    MD_PUSHDONE | MD_MAPPING);

		/* if a directory, populate it */
		if (CTOV(cp)->v_type == VDIR) {
			/* XXX hack for now */
			mdp->md_flags |= MD_INVALREADDIR;
			dnlc_purge_vp(CTOV(cp));

			mdp->md_flags |= MD_NEEDATTRS;
		}

		if (!timedout) {
			error = CFSOP_CHECK_COBJECT(fscp, cp, 0, kcred);
			if (CFS_TIMEOUT(fscp, error))
				timedout = 1;
			else if ((error == 0) &&
			    ((fscp->fs_info.fi_mntflags & CFS_NOACL) == 0)) {
				if (cachefs_vtype_aclok(CTOV(cp)) &&
				    ((cp->c_flags & CN_NOCACHE) == 0))
					(void) cachefs_cacheacl(cp, NULL);
			}
		}

		cp->c_flags |= CN_UPDATED;
		cachefs_mutex_exit(&cp->c_statelock);
		VN_RELE(CTOV(cp));
	}
	cachefs_mutex_exit(&cachep->c_mflock);
}

void
cachefs_inum_register(fscache_t *fscp, ino64_t real, ino64_t fake)
{
	cachefs_inum_trans_t *tbl;
	int toff, thop;
	int i;

	ASSERT(CACHEFS_MUTEX_HELD(&fscp->fs_fslock));

	/*
	 * first, see if an empty slot exists.
	 */

	for (i = 0; i < fscp->fs_inum_size; i++)
		if (fscp->fs_inum_trans[i].cit_real == 0)
			break;

	/*
	 * if there are no empty slots, try to grow the table.
	 */

	if (i >= fscp->fs_inum_size) {
		cachefs_inum_trans_t *oldtbl;
		int oldsize, newsize = 0;

		/*
		 * try to fetch a new table size that's bigger than
		 * our current size
		 */

		for (i = 0; cachefs_hash_sizes[i] != 0; i++)
			if (cachefs_hash_sizes[i] > fscp->fs_inum_size) {
				newsize = cachefs_hash_sizes[i];
				break;
			}

		/*
		 * if we're out of larger twin-primes, give up.  thus,
		 * the inode numbers in some directory entries might
		 * change at reconnect, and disagree with what stat()
		 * says.  this isn't worth panicing over, but it does
		 * merit a warning message.
		 */
		if (newsize == 0) {
			/* only print hash table warning once */
			if ((fscp->fs_flags & CFS_FS_HASHPRINT) == 0) {
				cmn_err(CE_WARN,
				    "cachefs: inode hash table full\n");
				fscp->fs_flags |= CFS_FS_HASHPRINT;
			}
			return;
		}

		/* set up this fscp with a new hash table */

		oldtbl = fscp->fs_inum_trans;
		oldsize = fscp->fs_inum_size;
		fscp->fs_inum_size = newsize;
		fscp->fs_inum_trans = (cachefs_inum_trans_t *)
		    cachefs_kmem_zalloc(sizeof (cachefs_inum_trans_t) * newsize,
			KM_SLEEP);

		/*
		 * re-insert all of the old values.  this will never
		 * go more than one level into recursion-land.
		 */

		for (i = 0; i < oldsize; i++) {
			tbl = oldtbl + i;
			if (tbl->cit_real != 0) {
				cachefs_inum_register(fscp, tbl->cit_real,
				    tbl->cit_fake);
			} else {
				ASSERT(0);
			}
		}

		if (oldsize > 0)
			cachefs_kmem_free((caddr_t)oldtbl, oldsize *
			    sizeof (cachefs_inum_trans_t));
	}

	/*
	 * compute values for the hash table.  see ken rosen's
	 * `elementary number theory and its applications' for one
	 * description of double hashing.
	 */

	toff = real % fscp->fs_inum_size;
	thop = (real % (fscp->fs_inum_size - 2)) + 1;

	/*
	 * since we know the hash table isn't full when we get here,
	 * this loop shouldn't terminate except via the `break'.
	 */

	for (i = 0; i < fscp->fs_inum_size; i++) {
		tbl = fscp->fs_inum_trans + toff;
		if ((tbl->cit_real == 0) || (tbl->cit_real == real)) {
			tbl->cit_real = real;
			tbl->cit_fake = fake;
			break;
		}

		toff += thop;
		toff %= fscp->fs_inum_size;
	}
	ASSERT(i < fscp->fs_inum_size);
}

/*
 * given an inode number, map it to the inode number that should be
 * put in a directory entry before its copied out.
 *
 * don't call this function unless there is a fscp->fs_inum_trans
 * table that has real entries in it!
 */

ino64_t
cachefs_inum_real2fake(fscache_t *fscp, ino64_t real)
{
	cachefs_inum_trans_t *tbl;
	ino64_t rc = real;
	int toff, thop;
	int i;

	ASSERT(fscp->fs_inum_size > 0);
	ASSERT(CACHEFS_MUTEX_HELD(&fscp->fs_fslock));

	toff = real % fscp->fs_inum_size;
	thop = (real % (fscp->fs_inum_size - 2)) + 1;

	for (i = 0; i < fscp->fs_inum_size; i++) {
		tbl = fscp->fs_inum_trans + toff;

		if (tbl->cit_real == 0) {
			break;
		} else if (tbl->cit_real == real) {
			rc = tbl->cit_fake;
			break;
		}

		toff += thop;
		toff %= fscp->fs_inum_size;
	}

	return (rc);
}

/*
 * Passed a cid, finds the cnode and sets the MD_NEEDATTRS bit
 * in the metadata.
 */
static void
cachefs_iosetneedattrs(fscache_t *fscp, cfs_cid_t *cidp)
{
	int error;
	cnode_t *cp;

	error = cachefs_cnode_make(cidp, fscp,
	    NULL, NULL, NULL, kcred, 0, &cp);
	if (error)
		return;

	cachefs_mutex_enter(&cp->c_statelock);
	cp->c_metadata.md_flags |= MD_NEEDATTRS;
	cp->c_flags |= CN_UPDATED;
	cachefs_mutex_exit(&cp->c_statelock);

	VN_RELE(CTOV(cp));
}
