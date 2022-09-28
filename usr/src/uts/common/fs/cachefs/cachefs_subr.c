/*
 * Copyright (c) 1996, Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)cachefs_subr.c	1.168	96/10/22 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/pathname.h>
#include <sys/uio.h>
#include <sys/tiuser.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <sys/statvfs.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/utsname.h>
#include <sys/modctl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/fbuf.h>
#include <sys/dnlc.h>
#include <sys/callb.h>
#include <sys/kobj.h>

#include <sys/vmsystm.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <vm/seg.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/rm.h>
#include <sys/fs/cachefs_fs.h>
#include <sys/fs/cachefs_log.h>
#include <sys/fs/cachefs_dir.h>

extern struct seg *segkmap;
caddr_t segmap_getmap();
int segmap_release();

extern struct cnode *cachefs_freeback;
extern struct cnode *cachefs_freefront;
extern cachefscache_t *cachefs_cachelist;

#ifdef CFSDEBUG
int cachefsdebug = 0;
#endif

int cachefs_max_threads = CFS_MAX_THREADS;
ino64_t cachefs_check_fileno = 0;
struct kmem_cache *cachefs_cache_kmcache = NULL;
struct kmem_cache *cachefs_req_cache = NULL;

static int
cachefs_async_populate_reg(struct cachefs_populate_req *, cred_t *,
    vnode_t *, vnode_t *);

/*
 * Cache routines
 */

/*
 * ------------------------------------------------------------------
 *
 *		cachefs_cache_create
 *
 * Description:
 *	Creates a cachefscache_t object and initializes it to
 *	be NOCACHE and NOFILL mode.
 * Arguments:
 * Returns:
 *	Returns a pointer to the created object or NULL if
 *	threads could not be created.
 * Preconditions:
 */

cachefscache_t *
cachefs_cache_create(void)
{
	cachefscache_t *cachep;
	int error;
	struct cachefs_req *rp;

	/* allocate zeroed memory for the object */
	cachep = (cachefscache_t *)
	    kmem_cache_alloc(cachefs_cache_kmcache, KM_SLEEP);

	bzero((caddr_t)cachep, sizeof (*cachep));

	cv_init(&cachep->c_cwcv, "cache worker thread cv", CV_DEFAULT, NULL);
	cv_init(&cachep->c_cwhaltcv, "cw thread halt cv", CV_DEFAULT, NULL);
	cachefs_mutex_init(&cachep->c_contentslock, "cache contents",
		MUTEX_DEFAULT, NULL);
	cachefs_mutex_init(&cachep->c_fslistlock, "cache fslist",
		MUTEX_DEFAULT, NULL);
	cachefs_mutex_init(&cachep->c_log_mutex, "cachefs logging mutex",
	    MUTEX_DEFAULT, NULL);

	/* set up the work queue and get the sync thread created */
	cachefs_workq_init(&cachep->c_workq);
	cachep->c_workq.wq_keepone = 1;
	cachep->c_workq.wq_cachep = cachep;
	rp = (struct cachefs_req *)
	    kmem_cache_alloc(cachefs_req_cache, KM_SLEEP);
	rp->cfs_cmd = CFS_NOOP;
	rp->cfs_cr = kcred;
	rp->cfs_req_u.cu_fs_sync.cf_cachep = cachep;
	crhold(rp->cfs_cr);
	error = cachefs_addqueue(rp, &cachep->c_workq);
	if (error) {
		goto out;
	}

	cachep->c_flags |= CACHE_NOCACHE | CACHE_NOFILL;
out:
	if (error) {
		kmem_cache_free(cachefs_cache_kmcache, cachep);
		cachep = NULL;
	}
	return (cachep);
}

/*
 * ------------------------------------------------------------------
 *
 *		cachefs_cache_destroy
 *
 * Description:
 *	Destroys the cachefscache_t object.
 * Arguments:
 *	cachep	the cachefscache_t object to destroy
 * Returns:
 * Preconditions:
 *	precond(cachep)
 */

void
cachefs_cache_destroy(cachefscache_t *cachep)
{
	int tend;
#ifdef CFSRLDEBUG
	u_int index;
#endif /* CFSRLDEBUG */

	/* stop async threads */
	while (cachep->c_workq.wq_thread_count > 0)
		(void) cachefs_async_halt(&cachep->c_workq, 1);

	/* kill off the cachep worker thread */
	cachefs_mutex_enter(&cachep->c_contentslock);
	while (cachep->c_flags & CACHE_CACHEW_THREADRUN) {
		cachep->c_flags |= CACHE_CACHEW_THREADEXIT;
		cv_signal(&cachep->c_cwcv);
		tend = lbolt + (60 * hz);
		(void) cachefs_cv_timedwait(&cachep->c_cwhaltcv,
			&cachep->c_contentslock, tend);
	}
	cachefs_mutex_exit(&cachep->c_contentslock);

	/* if there is a cache */
	if ((cachep->c_flags & CACHE_NOCACHE) == 0) {
		if ((cachep->c_flags & CACHE_NOFILL) == 0) {
#ifdef CFSRLDEBUG
			/* blow away dangling rl debugging info */
			cachefs_mutex_enter(&cachep->c_contentslock);
			for (index = 0;
			    index <= cachep->c_rlinfo.rl_entries;
			    index++) {
				rl_entry_t *rlent;

				rlent = cachefs_rl_entry_get(cachep, index);
				cachefs_rl_debug_destroy(rlent);
			}
			cachefs_mutex_exit(&cachep->c_contentslock);
#endif /* CFSRLDEBUG */

			/* sync the cache */
			cachefs_cache_sync(cachep);
		} else {
			/* get rid of any unused fscache objects */
			cachefs_mutex_enter(&cachep->c_fslistlock);
			fscache_list_gc(cachep);
			cachefs_mutex_exit(&cachep->c_fslistlock);
		}
		ASSERT(cachep->c_fslist == NULL);

		VN_RELE(cachep->c_resfilevp);
		VN_RELE(cachep->c_dirvp);
		VN_RELE(cachep->c_lockvp);
		VN_RELE(cachep->c_lostfoundvp);
	}

	if (cachep->c_log_ctl != NULL)
		cachefs_kmem_free((caddr_t)cachep->c_log_ctl,
		    sizeof (cachefs_log_control_t));
	if (cachep->c_log != NULL)
		cachefs_log_destroy_cookie(cachep->c_log);

	cv_destroy(&cachep->c_cwcv);
	cv_destroy(&cachep->c_cwhaltcv);
	cachefs_mutex_destroy(&cachep->c_contentslock);
	cachefs_mutex_destroy(&cachep->c_fslistlock);
	cachefs_mutex_destroy(&cachep->c_log_mutex);

	kmem_cache_free(cachefs_cache_kmcache, cachep);
}

/*
 * ------------------------------------------------------------------
 *
 *		cachefs_cache_active_ro
 *
 * Description:
 *	Activates the cachefscache_t object for a read-only file system.
 * Arguments:
 *	cachep	the cachefscache_t object to activate
 *	cdvp	the vnode of the cache directory
 * Returns:
 *	Returns 0 for success, !0 if there is a problem with the cache.
 * Preconditions:
 *	precond(cachep)
 *	precond(cdvp)
 *	precond(cachep->c_flags & CACHE_NOCACHE)
 */

int
cachefs_cache_activate_ro(cachefscache_t *cachep, vnode_t *cdvp)
{
	cachefs_log_control_t *lc;
	vnode_t *labelvp = NULL;
	vnode_t *rifvp = NULL;
	vnode_t *lockvp = NULL;
	vnode_t *statevp = NULL;
	vnode_t *lostfoundvp = NULL;
	struct vattr *attrp = NULL;
	int error;

	ASSERT(cachep->c_flags & CACHE_NOCACHE);
	cachefs_mutex_enter(&cachep->c_contentslock);

	attrp = (struct vattr *)cachefs_kmem_alloc(sizeof (struct vattr),
			KM_SLEEP);

	/* get the mode bits of the cache directory */
	attrp->va_mask = AT_ALL;
	error = VOP_GETATTR(cdvp, attrp, 0, kcred);
	if (error)
		goto out;

	/* ensure the mode bits are 000 to keep out casual users */
	if (attrp->va_mode & S_IAMB) {
		cmn_err(CE_WARN, "cachefs: Cache Directory Mode must be 000\n");
		error = EPERM;
		goto out;
	}

	/* Get the lock file */
	error = VOP_LOOKUP(cdvp, CACHEFS_LOCK_FILE, &lockvp, NULL, 0, NULL,
		kcred);
	if (error) {
		cmn_err(CE_WARN, "cachefs: activate_a: cache corruption"
			" run fsck.\n");
		goto out;
	}

	/* Get the label file */
	error = VOP_LOOKUP(cdvp, CACHELABEL_NAME, &labelvp, NULL, 0, NULL,
		kcred);
	if (error) {
		cmn_err(CE_WARN, "cachefs: activate_b: cache corruption"
			" run fsck.\n");
		goto out;
	}

	/* read in the label */
	error = vn_rdwr(UIO_READ, labelvp, (caddr_t)&cachep->c_label,
			sizeof (struct cache_label), 0LL, UIO_SYSSPACE,
			0, (long long)0, kcred, NULL);
	if (error) {
		cmn_err(CE_WARN, "cachefs: activate_c: cache corruption"
			" run fsck.\n");
		goto out;
	}

	/* Verify that we can handle the version this cache was created under */
	if (cachep->c_label.cl_cfsversion != CFSVERSION) {
		cmn_err(CE_WARN, "cachefs: Invalid Cache Version, run fsck\n");
		error = EINVAL;
		goto out;
	}

	/* Open the resource file */
	error = VOP_LOOKUP(cdvp, RESOURCE_NAME, &rifvp, NULL, 0, NULL, kcred);
	if (error) {
		cmn_err(CE_WARN, "cachefs: activate_d: cache corruption"
			" run fsck.\n");
		goto out;
	}

	/*  Read the usage struct for this cache */
	error = vn_rdwr(UIO_READ, rifvp, (caddr_t)&cachep->c_usage,
			sizeof (struct cache_usage), 0LL, UIO_SYSSPACE, 0,
			(long long)0, kcred, NULL);
	if (error) {
		cmn_err(CE_WARN, "cachefs: activate_e: cache corruption"
			" run fsck.\n");
		goto out;
	}

	if (cachep->c_usage.cu_flags & CUSAGE_ACTIVE) {
		cmn_err(CE_WARN, "cachefs: cache not clean.  Run fsck\n");
		/* ENOSPC is what UFS uses for clean flag check */
		error = ENOSPC;
		goto out;
	}

	/*  Read the rlinfo for this cache */
	error = vn_rdwr(UIO_READ, rifvp, (caddr_t)&cachep->c_rlinfo,
	sizeof (cachefs_rl_info_t), (offset_t)sizeof (struct cache_usage),
			UIO_SYSSPACE, 0, 0, kcred, NULL);
	if (error) {
		cmn_err(CE_WARN, "cachefs: activate_f: cache corruption"
			" run fsck.\n");
		goto out;
	}

	/* Open the lost+found directory */
	error = VOP_LOOKUP(cdvp, CACHEFS_LOSTFOUND_NAME, &lostfoundvp,
	    NULL, 0, NULL, kcred);
	if (error) {
		cmn_err(CE_WARN, "cachefs: activate_g: cache corruption"
			" run fsck.\n");
		goto out;
	}

	VN_HOLD(rifvp);
	VN_HOLD(cdvp);
	VN_HOLD(lockvp);
	VN_HOLD(lostfoundvp);
	cachep->c_resfilevp = rifvp;
	cachep->c_dirvp = cdvp;
	cachep->c_lockvp = lockvp;
	cachep->c_lostfoundvp = lostfoundvp;

	/* get the cachep worker thread created */
	cachep->c_flags |= CACHE_CACHEW_THREADRUN;
	if (thread_create(NULL, NULL, cachefs_cachep_worker_thread,
	    (caddr_t)cachep, 0, &p0, TS_RUN, 60) == NULL) {
		cmn_err(CE_WARN,
			"cachefs: Can't start garbage collection thread.\n");
	}

	/* allocate the `logging control' field */
	cachefs_mutex_enter(&cachep->c_log_mutex);
	cachep->c_log_ctl = (struct cachefs_log_control *)
	    cachefs_kmem_zalloc(sizeof (cachefs_log_control_t), KM_SLEEP);
	lc = (cachefs_log_control_t *)cachep->c_log_ctl;

	/* if the LOG_STATUS_NAME file exists, read it in and set up logging */
	error = VOP_LOOKUP(cachep->c_dirvp, LOG_STATUS_NAME, &statevp,
	    NULL, 0, NULL, kcred);
	if (error == 0) {
		int vnrw_error;

		vnrw_error = vn_rdwr(UIO_READ, statevp, (caddr_t)lc,
		    sizeof (*lc), 0LL, UIO_SYSSPACE, 0, (rlim64_t)RLIM_INFINITY,
		    kcred, NULL);
		VN_RELE(statevp);

		if (vnrw_error == 0) {
			if ((cachep->c_log = cachefs_log_create_cookie(lc))
			    == NULL)
				cachefs_log_error(cachep, ENOMEM, 0);
			else if ((lc->lc_magic != CACHEFS_LOG_MAGIC) ||
			    (lc->lc_path[0] != '/') ||
			    (cachefs_log_logfile_open(cachep,
			    lc->lc_path) != 0))
				cachefs_log_error(cachep, EINVAL, 0);
		}
	} else {
		error = 0;
	}
	lc->lc_magic = CACHEFS_LOG_MAGIC;
	lc->lc_cachep = cachep;
	cachefs_mutex_exit(&cachep->c_log_mutex);

out:
	if (error == 0) {
		cachep->c_flags &= ~CACHE_NOCACHE;
	}
	if (attrp)
		cachefs_kmem_free((caddr_t)attrp, sizeof (struct vattr));
	if (labelvp != NULL)
		VN_RELE(labelvp);
	if (rifvp != NULL)
		VN_RELE(rifvp);
	if (lockvp)
		VN_RELE(lockvp);
	if (lostfoundvp)
		VN_RELE(lostfoundvp);

	cachefs_mutex_exit(&cachep->c_contentslock);
	return (error);
}

int
cachefs_stop_cache(cnode_t *cp)
{
	fscache_t *fscp = C_TO_FSCACHE(cp);
	cachefscache_t *cachep = fscp->fs_cache;
	filegrp_t *fgp;
	int i, tend;
	int error = 0;

	/* XXX verify lock-ordering for this function */

	cachefs_mutex_enter(&cachep->c_contentslock);

	/*
	 * no work if we're already in nocache mode.  hopefully this
	 * will be the usual case.
	 */

	if (cachep->c_flags & CACHE_NOCACHE) {
		cachefs_mutex_exit(&cachep->c_contentslock);
		return (0);
	}

	if ((cachep->c_flags & CACHE_NOFILL) == 0) {
		cachefs_mutex_exit(&cachep->c_contentslock);
		return (EINVAL);
	}

	cachefs_mutex_exit(&cachep->c_contentslock);

#ifdef CFSDEBUG
	cachefs_mutex_enter(&cachep->c_fslistlock);
	ASSERT(fscp == cachep->c_fslist);
	ASSERT(fscp->fs_next == NULL);
	cachefs_mutex_exit(&cachep->c_fslistlock);

	printf("cachefs_stop_cache: resetting CACHE_NOCACHE\n");
#endif

	/* XXX should i worry about disconnected during boot? */
	error = cachefs_cd_access(fscp, 1, 1);
	if (error)
		goto out;

	error = cachefs_async_halt(&fscp->fs_workq, 1);
	ASSERT(error == 0);
	error = cachefs_async_halt(&cachep->c_workq, 1);
	ASSERT(error == 0);
	/* sigh -- best to keep going if async_halt failed. */
	error = 0;

	/* XXX current order: cnode, fgp, fscp, cache. okay? */

	cachefs_cnode_traverse(fscp, cachefs_cnode_disable_caching);

	for (i = 0; i < CFS_FS_FGP_BUCKET_SIZE; i++) {
		for (fgp = fscp->fs_filegrp[i]; fgp != NULL;
		fgp = fgp->fg_next) {
			cachefs_mutex_enter(&fgp->fg_mutex);

			ASSERT((fgp->fg_flags &
			    (CFS_FG_WRITE | CFS_FG_UPDATED)) == 0);
			fgp->fg_flags |=
			    CFS_FG_ALLOC_FILE |
			    CFS_FG_ALLOC_ATTR;
			fgp->fg_flags &= ~CFS_FG_READ;

			if (fgp->fg_dirvp) {
				fgp->fg_flags |= CFS_FG_ALLOC_FILE;
				VN_RELE(fgp->fg_dirvp);
				fgp->fg_dirvp = NULL;
			}
			if (fgp->fg_attrvp) {
				fgp->fg_flags |= CFS_FG_ALLOC_ATTR;
				VN_RELE(fgp->fg_attrvp);
				fgp->fg_attrvp = NULL;
			}

			cachefs_mutex_exit(&fgp->fg_mutex);
		}
	}

	cachefs_mutex_enter(&fscp->fs_fslock);
	ASSERT((fscp->fs_flags & (CFS_FS_WRITE)) == 0);
	fscp->fs_flags &= ~(CFS_FS_READ | CFS_FS_DIRTYINFO);

	if (fscp->fs_fscdirvp) {
		VN_RELE(fscp->fs_fscdirvp);
		fscp->fs_fscdirvp = NULL;
	}
	if (fscp->fs_fsattrdir) {
		VN_RELE(fscp->fs_fsattrdir);
		fscp->fs_fsattrdir = NULL;
	}
	if (fscp->fs_infovp) {
		VN_RELE(fscp->fs_infovp);
		fscp->fs_infovp = NULL;
	}
	/* XXX dlog stuff? */

	cachefs_mutex_exit(&fscp->fs_fslock);

	/*
	 * release resources grabbed in cachefs_cache_activate_ro
	 */

	cachefs_mutex_enter(&cachep->c_contentslock);

	/* kill off the cachep worker thread */
	while (cachep->c_flags & CACHE_CACHEW_THREADRUN) {
		cachep->c_flags |= CACHE_CACHEW_THREADEXIT;
		cv_signal(&cachep->c_cwcv);
		tend = lbolt + (60 * hz);
		(void) cachefs_cv_timedwait(&cachep->c_cwhaltcv,
			&cachep->c_contentslock, tend);
	}

	if (cachep->c_resfilevp) {
		VN_RELE(cachep->c_resfilevp);
		cachep->c_resfilevp = NULL;
	}
	if (cachep->c_dirvp) {
		VN_RELE(cachep->c_dirvp);
		cachep->c_dirvp = NULL;
	}
	if (cachep->c_lockvp) {
		VN_RELE(cachep->c_lockvp);
		cachep->c_lockvp = NULL;
	}
	if (cachep->c_lostfoundvp) {
		VN_RELE(cachep->c_lostfoundvp);
		cachep->c_lostfoundvp = NULL;
	}

	cachefs_mutex_enter(&cachep->c_log_mutex);
	if (cachep->c_log_ctl) {
		cachefs_kmem_free((caddr_t)cachep->c_log_ctl,
		    sizeof (cachefs_log_control_t));
		cachep->c_log_ctl = NULL;
	}
	if (cachep->c_log) {
		cachefs_log_destroy_cookie(cachep->c_log);
		cachep->c_log = NULL;
	}
	cachefs_mutex_exit(&cachep->c_log_mutex);

	/* XXX do what mountroot_init does when ! foundcache */

	cachep->c_flags |= CACHE_NOCACHE;
	cachefs_mutex_exit(&cachep->c_contentslock);

	/* XXX should i release this here? */
	cachefs_cd_release(fscp);

out:

	return (error);
}

/*
 * ------------------------------------------------------------------
 *
 *		cachefs_cache_active_rw
 *
 * Description:
 *	Activates the cachefscache_t object for a read-write file system.
 * Arguments:
 *	cachep	the cachefscache_t object to activate
 * Returns:
 * Preconditions:
 *	precond(cachep)
 *	precond((cachep->c_flags & CACHE_NOCACHE) == 0)
 *	precond(cachep->c_flags & CACHE_NOFILL)
 */

void
cachefs_cache_activate_rw(cachefscache_t *cachep)
{
	cachefs_rl_listhead_t *lhp;

	ASSERT((cachep->c_flags & CACHE_NOCACHE) == 0);
	ASSERT(cachep->c_flags & CACHE_NOFILL);

	cachefs_mutex_enter(&cachep->c_contentslock);
	cachep->c_flags &= ~CACHE_NOFILL;

	/* move the active list to the rl list */
	cachefs_rl_cleanup(cachep);

	lhp = &cachep->c_rlinfo.rl_items[
	    CACHEFS_RL_INDEX(CACHEFS_RL_PACKED_PENDING)];
	if (lhp->rli_itemcnt != 0)
		cachep->c_flags |= CACHE_PACKED_PENDING;
	cachefs_cache_dirty(cachep, 0);
	cachefs_mutex_exit(&cachep->c_contentslock);
}

/*
 * ------------------------------------------------------------------
 *
 *		cachefs_cache_dirty
 *
 * Description:
 *	Marks the cache as dirty (active).
 * Arguments:
 *	cachep	the cachefscache_t to mark as dirty
 *	lockit	1 means grab contents lock, 0 means caller grabbed it
 * Returns:
 * Preconditions:
 *	precond(cachep)
 *	precond(cache is in rw mode)
 */

void
cachefs_cache_dirty(struct cachefscache *cachep, int lockit)
{
	int error;

	ASSERT((cachep->c_flags & (CACHE_NOCACHE | CACHE_NOFILL)) == 0);

	if (lockit) {
		cachefs_mutex_enter(&cachep->c_contentslock);
	} else {
		ASSERT(CACHEFS_MUTEX_HELD(&cachep->c_contentslock));
	}
	if (cachep->c_flags & CACHE_DIRTY) {
		ASSERT(cachep->c_usage.cu_flags & CUSAGE_ACTIVE);
	} else {
		/*
		 * turn on the "cache active" (dirty) flag and write it
		 * synchronously to disk
		 */
		cachep->c_flags |= CACHE_DIRTY;
		cachep->c_usage.cu_flags |= CUSAGE_ACTIVE;
		if (error = vn_rdwr(UIO_WRITE, cachep->c_resfilevp,
		    (caddr_t)&cachep->c_usage, sizeof (struct cache_usage),
		    0LL, UIO_SYSSPACE, FSYNC, (rlim64_t)RLIM_INFINITY,
				kcred, NULL)) {
			cmn_err(CE_WARN,
			    "cachefs: clean flag write error: %d\n", error);
		}
	}

	if (lockit)
		cachefs_mutex_exit(&cachep->c_contentslock);
}

/*
 * ------------------------------------------------------------------
 *
 *		cachefs_cache_rssync
 *
 * Description:
 *	Syncs out the resource file for the cachefscache_t object.
 * Arguments:
 *	cachep	the cachefscache_t object to operate on
 * Returns:
 *	Returns 0 for success, !0 on an error writing data.
 * Preconditions:
 *	precond(cachep)
 *	precond(cache is in rw mode)
 */

int
cachefs_cache_rssync(struct cachefscache *cachep)
{
	int error;

	ASSERT((cachep->c_flags & (CACHE_NOCACHE | CACHE_NOFILL)) == 0);

	if (cachep->c_rl_entries != NULL) {
		(void) segmap_release(segkmap,
			(caddr_t)cachep->c_rl_entries, 0);
		cachep->c_rl_entries = NULL;
	}

	/* write the usage struct for this cache */
	error = vn_rdwr(UIO_WRITE, cachep->c_resfilevp,
		(caddr_t)&cachep->c_usage, sizeof (struct cache_usage),
		0LL, UIO_SYSSPACE, 0, (rlim64_t)RLIM_INFINITY, kcred, NULL);
	if (error) {
		cmn_err(CE_WARN, "cachefs: Can't Write Cache Usage Info\n");
	}

	/* write the rlinfo for this cache */
	error = vn_rdwr(UIO_WRITE, cachep->c_resfilevp,
			(caddr_t)&cachep->c_rlinfo, sizeof (cachefs_rl_info_t),
			(offset_t)sizeof (struct cache_usage), UIO_SYSSPACE,
			0, (rlim64_t)RLIM_INFINITY, kcred, NULL);
	if (error) {
		cmn_err(CE_WARN, "cachefs: Can't Write Cache RL Info\n");
	}
	error = VOP_FSYNC(cachep->c_resfilevp, FSYNC, kcred);
	return (error);
}

/*
 * ------------------------------------------------------------------
 *
 *		cachefs_cache_sync
 *
 * Description:
 *	Sync a cache which includes all of its fscaches.
 * Arguments:
 *	cachep	the cachefscache_t object to sync
 * Returns:
 * Preconditions:
 *	precond(cachep)
 *	precond(cache is in rw mode)
 */

void
cachefs_cache_sync(struct cachefscache *cachep)
{
	struct fscache *fscp;
	struct fscache **syncfsc;
	int nfscs, fscidx;
	int try;
	int done;

	if (cachep->c_flags & (CACHE_NOCACHE | CACHE_NOFILL))
		return;

	done = 0;
	for (try = 0; (try < 2) && !done; try++) {

		nfscs = 0;

		/*
		 * here we turn off the cache-wide DIRTY flag.  If it's still
		 * off when the sync completes we can write the clean flag to
		 * disk telling fsck it has no work to do.
		 */
#ifdef CFSCLEANFLAG
		cachefs_mutex_enter(&cachep->c_contentslock);
		cachep->c_flags &= ~CACHE_DIRTY;
		cachefs_mutex_exit(&cachep->c_contentslock);
#endif /* CFSCLEANFLAG */

		cachefs_log_process_queue(cachep, 1);

		cachefs_mutex_enter(&cachep->c_fslistlock);
		syncfsc = (struct fscache **)cachefs_kmem_alloc(
				(cachep->c_refcnt*sizeof (struct fscache *)),
				KM_SLEEP);
		for (fscp = cachep->c_fslist; fscp; fscp = fscp->fs_next) {
			fscache_hold(fscp);
			ASSERT(nfscs < cachep->c_refcnt);
			syncfsc[nfscs++] = fscp;
		}
		ASSERT(nfscs == cachep->c_refcnt);
		cachefs_mutex_exit(&cachep->c_fslistlock);
		for (fscidx = 0; fscidx < nfscs; fscidx++) {
			fscp = syncfsc[fscidx];
			fscache_sync(fscp);
			fscache_rele(fscp);
		}

		/* get rid of any unused fscache objects */
		cachefs_mutex_enter(&cachep->c_fslistlock);
		fscache_list_gc(cachep);
		cachefs_mutex_exit(&cachep->c_fslistlock);

		/*
		 * here we check the cache-wide DIRTY flag.
		 * If it's off,
		 * we can write the clean flag to disk.
		 */
#ifdef CFSCLEANFLAG
		cachefs_mutex_enter(&cachep->c_contentslock);
		if ((cachep->c_flags & CACHE_DIRTY) == 0) {
			if (cachep->c_usage.cu_flags & CUSAGE_ACTIVE) {
				cachep->c_usage.cu_flags &= ~CUSAGE_ACTIVE;
				if (cachefs_cache_rssync(cachep) == 0) {
					done = 1;
				} else {
					cachep->c_usage.cu_flags |=
						CUSAGE_ACTIVE;
				}
			} else {
				done = 1;
			}
		}
		cachefs_mutex_exit(&cachep->c_contentslock);
#else /* CFSCLEANFLAG */
		cachefs_mutex_enter(&cachep->c_contentslock);
		(void) cachefs_cache_rssync(cachep);
		cachefs_mutex_exit(&cachep->c_contentslock);
		done = 1;
#endif /* CFSCLEANFLAG */
		cachefs_kmem_free((caddr_t)syncfsc,
			(nfscs*sizeof (struct fscache *)));
	}
}

/*
 * ------------------------------------------------------------------
 *
 *		cachefs_cache_unique
 *
 * Description:
 * Arguments:
 * Returns:
 *	Returns a unique number.
 * Preconditions:
 *	precond(cachep)
 */

u_int
cachefs_cache_unique(cachefscache_t *cachep)
{
	u_int unique = 0;
	int error = 0;

	cachefs_mutex_enter(&cachep->c_contentslock);
	if (cachep->c_usage.cu_flags & CUSAGE_NEED_ADJUST ||
		++(cachep->c_unique) == 0) {
		cachep->c_usage.cu_unique++;

		if (cachep->c_unique == 0)
			cachep->c_unique = 1;
		cachep->c_flags &= ~CUSAGE_NEED_ADJUST;
		error = cachefs_cache_rssync(cachep);
	}
	if (error == 0)
		unique = (cachep->c_usage.cu_unique << 16) + cachep->c_unique;
	cachefs_mutex_exit(&cachep->c_contentslock);
	return (unique);
}

/*
 * Called from c_getfrontfile. Shouldn't be called from anywhere else !
 */
static int
cachefs_createfrontfile(cnode_t *cp, struct filegrp *fgp)
{
	char name[CFS_FRONTFILE_NAME_SIZE];
	struct vattr *attrp = NULL;
	int error = 0;
	int mode;
	int alloc = 0;
	int freefile = 0;
	int ffrele = 0;
	int rlfree = 0;
	rl_entry_t rl_ent;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_FRONT)
		printf("c_createfrontfile: ENTER cp %x fgp %x\n",
			(int)cp, (int)fgp);
#endif

	ASSERT(cp->c_frontvp == NULL);

	/* quit if we cannot write to the filegrp */
	if ((fgp->fg_flags & CFS_FG_WRITE) == 0) {
		error = ENOENT;
		goto out;
	}

	/* find or create the filegrp attrcache file if necessary */
	if (fgp->fg_flags & CFS_FG_ALLOC_ATTR) {
		error = filegrp_allocattr(fgp);
		if (error)
			goto out;
	}

	make_ascii_name(&cp->c_id, name);

	/* set up attributes for the front file we want to create */
	attrp = (struct vattr *)cachefs_kmem_zalloc(
		sizeof (struct vattr), KM_SLEEP);
	alloc++;
	attrp->va_mode = S_IFREG | 0666;
	mode = 0666;
	attrp->va_uid = 0;
	attrp->va_gid = 0;
	attrp->va_type = VREG;
	attrp->va_size = 0;
	attrp->va_mask = AT_SIZE | AT_TYPE | AT_MODE | AT_UID | AT_GID;

	/* get a file from the resource counts */
	error = cachefs_allocfile(fgp->fg_fscp->fs_cache);
	if (error) {
		error = EINVAL;
		goto out;
	}
	freefile++;

	/* create the metadata slot if necessary */
	if (cp->c_flags & CN_ALLOC_PENDING) {
		error = filegrp_create_metadata(fgp, &cp->c_metadata,
		    &cp->c_id);
		if (error) {
			error = EINVAL;
			goto out;
		}
		cp->c_flags &= ~CN_ALLOC_PENDING;
		cp->c_flags |= CN_UPDATED;
	}

	/* get an rl entry if necessary */
	if (cp->c_metadata.md_rlno == 0) {
		rl_ent.rl_fileno = cp->c_id.cid_fileno;
		rl_ent.rl_local = (cp->c_id.cid_flags & CFS_CID_LOCAL) ? 1 : 0;
		rl_ent.rl_fsid = fgp->fg_fscp->fs_cfsid;
		rl_ent.rl_attrc = 0;
		error = cachefs_rl_alloc(fgp->fg_fscp->fs_cache, &rl_ent,
		    &cp->c_metadata.md_rlno);
		if (error)
			goto out;
		cachefs_rlent_moveto(fgp->fg_fscp->fs_cache,
		    CACHEFS_RL_ACTIVE, cp->c_metadata.md_rlno,
		    cp->c_metadata.md_frontblks);
		cp->c_metadata.md_rltype = CACHEFS_RL_ACTIVE;
		rlfree++;
		cp->c_flags |= CN_UPDATED; /* XXX sam: do we need this? */

		/* increment number of front files */
		error = filegrp_ffhold(fgp);
		if (error) {
			error = EINVAL;
			goto out;
		}
		ffrele++;
	}

	if (cp->c_flags & CN_ASYNC_POP_WORKING) {
		/* lookup the already created front file */
		error = VOP_LOOKUP(fgp->fg_dirvp, name, &cp->c_frontvp,
		    NULL, 0, NULL, kcred);
	} else {
		/* create the front file */
		error = VOP_CREATE(fgp->fg_dirvp, name, attrp, EXCL, mode,
		    &cp->c_frontvp, kcred, 0);
	}
	if (error) {
#ifdef CFSDEBUG
		CFS_DEBUG(CFSDEBUG_FRONT)
			printf("c_createfrontfile: Can't create cached object"
			    " error %u, fileno %llx\n", error,
			    cp->c_id.cid_fileno);
#endif
		goto out;
	}

	/* get a copy of the fid of the front file */
	cp->c_metadata.md_fid.fid_len = MAXFIDSZ;
	error = VOP_FID(cp->c_frontvp, &cp->c_metadata.md_fid);
	if (error) {
		/*
		 * If we get back ENOSPC then the fid we passed in was too
		 * small.  For now we don't do anything and map to EINVAL.
		 */
		if (error == ENOSPC) {
			error = EINVAL;
		}
		goto out;
	}

	dnlc_purge_vp(cp->c_frontvp);

	cp->c_metadata.md_flags |= MD_FILE;
	cp->c_flags |= CN_UPDATED | CN_NEED_FRONT_SYNC;

out:
	if (error) {
		if (cp->c_frontvp) {
			VN_RELE(cp->c_frontvp);
			VOP_REMOVE(fgp->fg_dirvp, name, kcred);
			cp->c_frontvp = NULL;
		}
		if (ffrele)
			filegrp_ffrele(fgp);
		if (freefile)
			cachefs_freefile(fgp->fg_fscp->fs_cache);
		if (rlfree) {
#ifdef CFSDEBUG
			cachefs_rlent_verify(fgp->fg_fscp->fs_cache,
			    CACHEFS_RL_ACTIVE, cp->c_metadata.md_rlno);
#endif /* CFSDEBUG */
			cachefs_rlent_moveto(fgp->fg_fscp->fs_cache,
			    CACHEFS_RL_FREE, cp->c_metadata.md_rlno, 0);
			cp->c_metadata.md_rlno = 0;
			cp->c_metadata.md_rltype = CACHEFS_RL_NONE;
		}
		cachefs_nocache(cp);
	}
	if (alloc)
		cachefs_kmem_free((caddr_t)attrp, sizeof (struct vattr));
#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_FRONT)
		printf("c_createfrontfile: EXIT error = %d name %s\n", error,
			name);
#endif
	return (error);
}

/*
 * Releases resources associated with the front file.
 * Only call this routine if a ffhold has been done.
 * Its okay to call this routine if the front file does not exist.
 * Note: this routine is used even if there is no front file.
 */
void
cachefs_removefrontfile(cachefs_metadata_t *mdp, cfs_cid_t *cidp,
    filegrp_t *fgp)
{
	int error;
	char name[CFS_FRONTFILE_NAME_SIZE + 2];

	if (mdp->md_flags & MD_FILE) {
		if (fgp->fg_dirvp == NULL) {
			cmn_err(CE_WARN, "cachefs: remove error, run fsck\n");
			return;
		}
		make_ascii_name(cidp, name);
		error = VOP_REMOVE(fgp->fg_dirvp, name, kcred);
		if ((error) && (error != ENOENT)) {
			cmn_err(CE_WARN, "UFS remove error %s %d, run fsck\n",
			    name, error);
		}
		if (mdp->md_flags & MD_ACLDIR) {
			(void) strcat(name, ".d");
			error = VOP_RMDIR(fgp->fg_dirvp, name, fgp->fg_dirvp,
			    kcred);
			if ((error) && (error != ENOENT)) {
				cmn_err(CE_WARN, "frontfs rmdir error %s %d"
				    "; run fsck\n", name, error);
			}
		}
		mdp->md_flags &= ~(MD_FILE | MD_POPULATED | MD_ACL | MD_ACLDIR);
		mdp->md_flags &= ~MD_PACKED;
		bzero((caddr_t)&mdp->md_allocinfo, mdp->md_allocents *
			sizeof (struct cachefs_allocmap));
		cachefs_freefile(fgp->fg_fscp->fs_cache);
	}

	/* XXX either rename routine or move this to caller */
	filegrp_ffrele(fgp);

	if (mdp->md_frontblks) {
		cachefs_freeblocks(fgp->fg_fscp->fs_cache, mdp->md_frontblks,
		    mdp->md_rltype);
		mdp->md_frontblks = 0;
	}
}

/*
 * This is the interface to the rest of CFS. This takes a cnode, and returns
 * the frontvp (stuffs it in the cnode). This creates an attrcache slot and
 * and frontfile if necessary.
 */

int
cachefs_getfrontfile(cnode_t *cp)
{
	struct filegrp *fgp = cp->c_filegrp;
	int error;
	struct vattr va;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_SUBR)
		printf("c_getfrontfile: ENTER cp %x\n", (int)cp);
#endif
	ASSERT(CACHEFS_MUTEX_HELD(&cp->c_statelock));

	/*
	 * Now we check to see if there is a front file for this entry.
	 * If there is, we get the vnode for it and stick it in the cnode.
	 * Otherwise, we create a front file, get the vnode for it and stick
	 * it in the cnode.
	 */
	if (cp->c_flags & CN_STALE) {
		cp->c_flags |= CN_NOCACHE;
		error = ESTALE;
		goto out;
	}
	if ((cp->c_metadata.md_flags & MD_FILE) == 0) {
#ifdef CFSDEBUG
		if (cp->c_frontvp != NULL)
			CFS_DEBUG(CFSDEBUG_FRONT)
				printf(
		"c_getfrontfile: !MD_FILE and frontvp not null cp %x\n",
				    (int) cp);
#endif
		if (CTOV(cp)->v_type == VDIR)
			ASSERT((cp->c_metadata.md_flags & MD_POPULATED) == 0);
		error = cachefs_createfrontfile(cp, fgp);
		if (error)
			goto out;
	} else {
		/*
		 * A front file exists, all we need to do is to grab the fid,
		 * do a VFS_VGET() on the fid, stuff the vnode in the cnode,
		 * and return.
		 */
		if (fgp->fg_dirvp == NULL) {
			cmn_err(CE_WARN, "cachefs: gff0: corrupted file system"
				" run fsck\n");
			cachefs_inval_object(cp);
			cp->c_flags |= CN_NOCACHE;
			error = ESTALE;
			goto out;
		}
		error = VFS_VGET(fgp->fg_dirvp->v_vfsp, &cp->c_frontvp,
				&cp->c_metadata.md_fid);
		if (error || (cp->c_frontvp == NULL)) {
#ifdef CFSDEBUG
			CFS_DEBUG(CFSDEBUG_FRONT)
				printf("cachefs: "
				    "gff1: front file system error %d\n",
				    error);
#endif /* CFSDEBUG */
			cachefs_inval_object(cp);
			cp->c_flags |= CN_NOCACHE;
			error = ESTALE;
			goto out;
		}

		/* don't need to check timestamps if need_front_sync is set */
		if (cp->c_flags & CN_NEED_FRONT_SYNC) {
			error = 0;
			goto out;
		}

		/* don't need to check empty directories */
		if (CTOV(cp)->v_type == VDIR &&
		    ((cp->c_metadata.md_flags & MD_POPULATED) == 0)) {
			error = 0;
			goto out;
		}

		/* get modify time of the front file */
		va.va_mask = AT_MTIME;
		error = VOP_GETATTR(cp->c_frontvp, &va, 0, kcred);
		if (error) {
			cmn_err(CE_WARN, "cachefs: gff2: front file"
				" system error %d", error);
			cachefs_inval_object(cp);
			error = (cp->c_flags & CN_NOCACHE) ? ESTALE : 0;
			goto out;
		}

		/* compare with modify time stored in metadata */
		if (bcmp((caddr_t)&va.va_mtime,
		    (caddr_t)(&cp->c_metadata.md_timestamp),
		    sizeof (timestruc_t)) != 0) {
#ifdef CFSDEBUG
			CFS_DEBUG(CFSDEBUG_GENERAL | CFSDEBUG_INVALIDATE) {
				int sec, nsec;
				sec = cp->c_metadata.md_timestamp.tv_sec;
				nsec = cp->c_metadata.md_timestamp.tv_nsec;
				printf("c_getfrontfile: timestamps don't"
					" match fileno %lld va %lx %x"
					" meta %x %x\n",
					cp->c_id.cid_fileno,
					va.va_mtime.tv_sec,
					(int)va.va_mtime.tv_nsec, sec, nsec);
			}
#endif
			cachefs_inval_object(cp);
			error = (cp->c_flags & CN_NOCACHE) ? ESTALE : 0;
		}
	}
out:

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_FRONT)
		printf("c_getfrontfile: EXIT error = %d\n", error);
#endif
	return (error);
}

void
cachefs_inval_object(cnode_t *cp)
{
	cachefscache_t *cachep = C_TO_FSCACHE(cp)->fs_cache;
	struct filegrp *fgp = cp->c_filegrp;
	int error;

#if 0
	CFS_DEBUG(CFSDEBUG_SUBR)
		printf("c_inval_object: ENTER cp %x\n", (int)cp);
	if (cp->c_flags & (CN_ASYNC_POPULATE | CN_ASYNC_POP_WORKING))
		debug_enter("inval object during async pop");
#endif
	cp->c_flags |= CN_NOCACHE;

	/* if we cannot modify the cache */
	if (C_TO_FSCACHE(cp)->fs_cache->c_flags &
	    (CACHE_NOFILL | CACHE_NOCACHE)) {
		goto out;
	}

	/* if there is a front file */
	if (cp->c_metadata.md_flags & MD_FILE) {
		if (fgp->fg_dirvp == NULL)
			goto out;

		/* get the front file vp if necessary */
		if (cp->c_frontvp == NULL) {

			error = VFS_VGET(fgp->fg_dirvp->v_vfsp, &cp->c_frontvp,
				&cp->c_metadata.md_fid);
			if (error || (cp->c_frontvp == NULL)) {
#ifdef CFSDEBUG
				CFS_DEBUG(CFSDEBUG_FRONT)
					printf("cachefs: "
					    "io: front file error %d\n", error);
#endif /* CFSDEBUG */
				goto out;
			}
		}

		/* truncate the file to zero size */
		error = cachefs_frontfile_size(cp, 0);
		if (error)
			goto out;
		cp->c_flags &= ~CN_NOCACHE;

		/* if a directory, v_type is zero if called from initcnode */
		if (cp->c_attr.va_type == VDIR) {
			if (cp->c_usage < CFS_DIRCACHE_COST) {
				cp->c_invals++;
				if (cp->c_invals > CFS_DIRCACHE_INVAL) {
					cp->c_invals = 0;
				}
			} else
				cp->c_invals = 0;
			cp->c_usage = 0;
		}
	} else {
		cp->c_flags &= ~CN_NOCACHE;
	}

out:
	if ((cp->c_metadata.md_flags & MD_PACKED) &&
	    (cp->c_metadata.md_rltype != CACHEFS_RL_MODIFIED) &&
	    ((cachep->c_flags & CACHE_NOFILL) == 0)) {
		ASSERT(cp->c_metadata.md_rlno != 0);
		if (cp->c_metadata.md_rltype != CACHEFS_RL_PACKED_PENDING) {
			cachefs_rlent_moveto(cachep,
			    CACHEFS_RL_PACKED_PENDING,
			    cp->c_metadata.md_rlno,
			    cp->c_metadata.md_frontblks);
			cp->c_metadata.md_rltype = CACHEFS_RL_PACKED_PENDING;
			/* unconditionally set CN_UPDATED below */
		}
	}

	cachefs_purgeacl(cp);

	if (cp->c_flags & CN_ASYNC_POP_WORKING)
		cp->c_flags |= CN_NOCACHE;
	cp->c_metadata.md_flags &= ~(MD_POPULATED | MD_INVALREADDIR |
	    MD_FASTSYMLNK);
	cp->c_flags &= ~CN_NEED_FRONT_SYNC;
	cp->c_flags |= CN_UPDATED;

	/*
	 * If the object invalidated is a directory, the dnlc should be purged
	 * to elide all references to this (directory) vnode.
	 */
	if (CTOV(cp)->v_type == VDIR)
		dnlc_purge_vp(CTOV(cp));

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_SUBR)
		printf("c_inval_object: EXIT\n");
#endif
}

void
make_ascii_name(cfs_cid_t *cidp, char *strp)
{
	int i = sizeof (u_int) * 4;
	u_longlong_t index;
	ino64_t name;

	if (cidp->cid_flags & CFS_CID_LOCAL)
		*strp++ = 'L';
	name = (ino64_t)cidp->cid_fileno;
	do {
		index = (((u_longlong_t)name) & 0xf000000000000000) >> 60;
		index &= (u_longlong_t)0xf;
		ASSERT(index < (u_longlong_t)16);
		*strp++ = "0123456789abcdef"[index];
		name <<= 4;
	} while (--i);
	*strp = '\0';
}

void
cachefs_nocache(cnode_t *cp)
{
	fscache_t *fscp = C_TO_FSCACHE(cp);
	cachefscache_t *cachep = fscp->fs_cache;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_SUBR)
		printf("c_nocache: ENTER cp %x\n", (int)cp);
#endif

	ASSERT(CACHEFS_MUTEX_HELD(&cp->c_statelock));
	if ((cp->c_flags & CN_NOCACHE) == 0) {
#ifdef CFSDEBUG
		CFS_DEBUG(CFSDEBUG_INVALIDATE)
			printf("cachefs_nocache: invalidating %lu\n",
			    cp->c_id.cid_fileno);
#endif
		/*
		 * Here we are waiting until inactive time to do
		 * the inval_object.  In case we don't get to inactive
		 * (because of a crash, say) we set up a timestamp mismatch
		 * such that getfrontfile will blow the front file away
		 * next time we try to use it.
		 */
		cp->c_metadata.md_timestamp.tv_sec = 0;
		cp->c_metadata.md_timestamp.tv_nsec = 0;
		cp->c_metadata.md_flags &= ~(MD_POPULATED | MD_INVALREADDIR |
		    MD_FASTSYMLNK);
		cp->c_flags &= ~CN_NEED_FRONT_SYNC;

		cachefs_purgeacl(cp);

		/*
		 * It is possible we can nocache while disconnected.
		 * A directory could be nocached by running out of space.
		 * A regular file should only be nocached if an I/O error
		 * occurs to the front fs.
		 * We count on the item staying on the modified list
		 * so we do not loose the cid to fid mapping for directories.
		 */

		if ((cp->c_metadata.md_flags & MD_PACKED) &&
		    (cp->c_metadata.md_rltype != CACHEFS_RL_MODIFIED) &&
		    ((cachep->c_flags & CACHE_NOFILL) == 0)) {
			ASSERT(cp->c_metadata.md_rlno != 0);
			if (cp->c_metadata.md_rltype !=
			    CACHEFS_RL_PACKED_PENDING) {
				cachefs_rlent_moveto(cachep,
				    CACHEFS_RL_PACKED_PENDING,
				    cp->c_metadata.md_rlno,
				    cp->c_metadata.md_frontblks);
				cp->c_metadata.md_rltype =
				    CACHEFS_RL_PACKED_PENDING;
				/* unconditionally set CN_UPDATED below */
			}
		}

		if (CTOV(cp)->v_type == VDIR)
			dnlc_purge_vp(CTOV(cp));
		cp->c_flags |= (CN_NOCACHE | CN_UPDATED);
	}

	if (CACHEFS_LOG_LOGGING(cachep, CACHEFS_LOG_NOCACHE))
		cachefs_log_nocache(cachep, 0, fscp->fs_cfsvfsp,
		    &cp->c_metadata.md_cookie, cp->c_id.cid_fileno);

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_SUBR)
		printf("c_nocache: EXIT cp %x\n", (int)cp);
#endif
}

/*
 * Checks to see if the page is in the disk cache, by checking the allocmap.
 */
int
cachefs_check_allocmap(cnode_t *cp, u_offset_t off)
{
	int i;
	int size_to_look =
		(int)MIN((u_offset_t)PAGESIZE, cp->c_attr.va_size - off);

	for (i = 0; i < cp->c_metadata.md_allocents; i++) {
		struct cachefs_allocmap *allocp =
				cp->c_metadata.md_allocinfo + i;

		if (off >= allocp->am_start_off) {
			if ((off + size_to_look) <=
			    (allocp->am_start_off + allocp->am_size)) {
				struct fscache *fscp = C_TO_FSCACHE(cp);
				cachefscache_t *cachep = fscp->fs_cache;

				if (CACHEFS_LOG_LOGGING(cachep,
				    CACHEFS_LOG_CALLOC))
					cachefs_log_calloc(cachep, 0,
					    fscp->fs_cfsvfsp,
					    &cp->c_metadata.md_cookie,
					    cp->c_id.cid_fileno,
					    off, size_to_look);
			/*
			 * Found the page in the CFS disk cache.
			 */
				return (1);
			}
		} else {
			return (0);
		}
	}
	return (0);
}

/*
 * Updates the allocmap to reflect a new chunk of data that has been
 * populated.
 */
void
cachefs_update_allocmap(cnode_t *cp, u_offset_t off, u_int size)
{
	int i;
	struct cachefs_allocmap *allocp;
	struct fscache *fscp =  C_TO_FSCACHE(cp);
	cachefscache_t *cachep = fscp->fs_cache;
	u_offset_t saveoff;
	u_offset_t savesize;
	u_offset_t logoff = off;
	u_int logsize = size;
	u_offset_t endoff;

	/*
	 * We try to see if we can coalesce the current block into an existing
	 * allocation and mark it as such.
	 * If we can't do that then we make a new entry in the allocmap.
	 * when we run out of allocmaps, put the cnode in NOCACHE mode.
	 */
again:
	allocp = cp->c_metadata.md_allocinfo;
	for (i = 0; i < cp->c_metadata.md_allocents; i++, allocp++) {

		if (off <= (allocp->am_start_off)) {
			if ((off + size) >= allocp->am_start_off) {
				endoff = MAX((allocp->am_start_off +
					allocp->am_size), (off + size));
				allocp->am_size = endoff - off;
				allocp->am_start_off = off;
				if (allocp->am_size >= cp->c_size)
					cp->c_metadata.md_flags |= MD_POPULATED;
				return;
			} else {
				saveoff = off;
				savesize = size;
				off = allocp->am_start_off;
				size = allocp->am_size;
				allocp->am_size = savesize;
				allocp->am_start_off = saveoff;
				goto again;
			}
		} else {
			if (off < (allocp->am_start_off + allocp->am_size)) {
				endoff = MAX((allocp->am_start_off +
					allocp->am_size), (off + size));
				allocp->am_size = endoff - allocp->am_start_off;
				if (allocp->am_size >= cp->c_size)
					cp->c_metadata.md_flags |= MD_POPULATED;
				return;
			}
			if (off == (allocp->am_start_off + allocp->am_size)) {
				allocp->am_size += size;
				if (allocp->am_size >= cp->c_size)
					cp->c_metadata.md_flags |= MD_POPULATED;
				return;
			}
		}
	}
	if (i == C_MAX_ALLOCINFO_SLOTS) {
#ifdef CFSDEBUG
		CFS_DEBUG(CFSDEBUG_ALLOCMAP)
			printf("c_update_alloc_map: "
			    "Too many allinfo entries cp %x fileno %lu %x\n",
			    (int) cp, cp->c_id.cid_fileno,
			    (int) cp->c_metadata.md_allocinfo);
#endif
		cachefs_nocache(cp);
		return;
	}
	allocp->am_start_off = off;
	allocp->am_size = (u_offset_t)size;
	if (allocp->am_size >= cp->c_size)
		cp->c_metadata.md_flags |= MD_POPULATED;
	cp->c_metadata.md_allocents++;

	if (CACHEFS_LOG_LOGGING(cachep, CACHEFS_LOG_UALLOC))
		cachefs_log_ualloc(cachep, 0, fscp->fs_cfsvfsp,
		    &cp->c_metadata.md_cookie, cp->c_id.cid_fileno,
		    logoff, logsize);
}

/*
 * CFS population function
 *
 * before async population, this function used to turn on the cnode
 * flags CN_UPDATED, CN_NEED_FRONT_SYNC, and CN_POPULATION_PENDING.
 * now, however, it's the responsibility of the caller to do this if
 * this function returns 0 (no error).
 */

int
cachefs_populate(cnode_t *cp, u_offset_t off, int popsize, vnode_t *frontvp,
    vnode_t *backvp, u_offset_t cpsize, cred_t *cr)
{
	int error = 0;
	caddr_t addr;
	u_offset_t upto;
	u_int size;
	u_offset_t from = off;
	cachefscache_t *cachep = C_TO_FSCACHE(cp)->fs_cache;
	int resid;
	struct fbuf *fbp;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_populate: ENTER cp %x off %d\n",
		    (int)cp, off);
#endif

	upto = MIN((off + popsize), cpsize);

	while (from < upto) {
		u_offset_t blkoff = (from & (u_offset_t)MAXBMASK);
		u_int n = from - blkoff;

		size = upto - from;
		if ((from + size) > (blkoff + MAXBSIZE))
			size = MAXBSIZE - n;
		error = fbread(backvp, (offset_t)blkoff, size,
			S_OTHER, &fbp);
		if (CFS_TIMEOUT(C_TO_FSCACHE(cp), error))
			goto out;
		else if (error) {
#ifdef CFSDEBUG
			CFS_DEBUG(CFSDEBUG_BACK)
				printf("cachefs_populate: fbread error %d\n",
				    error);
#endif
			goto out;
		}
		addr = fbp->fb_addr;
		ASSERT(addr != NULL);
		if (n == 0 || cachefs_check_allocmap(cp, blkoff) == 0) {
			error = cachefs_allocblocks(cachep, 1,
			    cp->c_metadata.md_rltype);
			if (error) {
				fbrelse(fbp, S_OTHER);
				goto out;
			}
			cp->c_metadata.md_frontblks++;
		}
		resid = 0;
		error = vn_rdwr(UIO_WRITE, frontvp, addr + n, size,
				(offset_t)from, UIO_SYSSPACE, 0,
				(rlim64_t)RLIM64_INFINITY, cr, &resid);
		fbrelse(fbp, S_OTHER);
		if (error) {
#ifdef CFSDEBUG
			CFS_DEBUG(CFSDEBUG_FRONT)
				printf("cachefs_populate: "
				    "Got error = %d from vn_rdwr\n", error);
#endif
			goto out;
		}
#ifdef CFSDEBUG
		if (resid)
			CFS_DEBUG(CFSDEBUG_FRONT)
				printf("cachefs_populate: non-zero resid %d\n",
				    resid);
#endif
		from += size;
	}
	(void) cachefs_update_allocmap(cp, off, upto - off);
out:
	if (CACHEFS_LOG_LOGGING(cachep, CACHEFS_LOG_POPULATE))
		cachefs_log_populate(cachep, error,
		    C_TO_FSCACHE(cp)->fs_cfsvfsp,
		    &cp->c_metadata.md_cookie, cp->c_id.cid_fileno, off,
		    popsize);

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_VOPS)
		printf("cachefs_populate: EXIT cp %x error %d\n",
		    (int)cp, error);
#endif
	return (error);
}

/*
 * due to compiler error we shifted cnode to the last argument slot.
 * occured during large files project - XXX.
 */
void
cachefs_cluster_allocmap(u_offset_t new_off, u_offset_t *popoffp,
    u_int *popsizep, int new_size, struct cnode *cp)
{
	int done = 0;
	int cur_entry;
	int prev_map = 0;
	int map_found = 0;
	int map_entries;
	u_offset_t new_end;
	u_offset_t prev_off;
	u_offset_t prev_end;
	u_offset_t cur_off;
	u_offset_t cur_size;
	u_offset_t cur_end;
	u_offset_t segsize;
	u_offset_t segoff;
	struct cachefs_allocmap *allocp;

	ASSERT(new_size <= C_TO_FSCACHE(cp)->fs_info.fi_popsize);

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_SUBR)
		printf("cachefs_cluster_allocmap: off %llx, size %x, "
			"c_size %x\n", new_off, new_size, cp->c_size);
#endif /* CFSDEBUG */

	map_entries = cp->c_metadata.md_allocents;
	cur_entry = 0;
	segsize = (u_offset_t)new_size;
	segoff = (u_offset_t)new_off;

	/* get the end of the new mapping */
	new_end = new_off + (u_offset_t)new_size - 1;

	/* try to find a mapping that overlaps the new mapping */
	while ((cur_entry < map_entries) && (done == 0)) {

		/* get map entry pointer */
		allocp = cp->c_metadata.md_allocinfo;
		allocp += cur_entry;

		/* get size, start and end offset of current mapping */
		cur_size = allocp->am_size;
		cur_off = allocp->am_start_off;
		cur_end = cur_off + cur_size - 1;

#ifdef CFSDEBUG
		CFS_DEBUG(CFSDEBUG_SUBR)
			printf("cachefs_cluster_allocmap: entry %d, "
				"cur_off %llx, cur_end %llx\n",
				cur_entry, cur_off, cur_end);
#endif /* CFSDEBUG */

		/* is this mapping past the new offset? */
		if (cur_off >= new_off) {
			done = 1;
			/*
			 *    then the mappings look like this:
			 *		-------------------
			 *   cur_off -> | current mapping |
			 *		-------------------
			 *	-----------------
			 *	|  new mapping  | <- new_end
			 *	-----------------
			 *			or this
			 *			-------------------
			 *	    cur_off ->	| current mapping |
			 *			-------------------
			 *	-----------------
			 *	|  new mapping  | <- new_end
			 *	-----------------
			 */
			/* does it overlap with the new mapping? */
			if ((new_end + 1) >= cur_off) {
				map_found = 1;
			}
		} else {
			prev_map = 1;
			prev_off = cur_off;
			prev_end = cur_end;
			cur_entry++;
		}
	}

	/* did we find a mapping? */
	if (map_found) {

		/* merge this mapping and the new one */
		/*
		 *    if the mapping look like this:
		 *		-------------------
		 *		| current mapping | <- cur_end
		 *		-------------------
		 *	-------------------
		 *	|   new mapping   | <- new_end
		 *	-------------------
		 */
		if (new_end < cur_end) {
			segsize = (cur_off - new_off) + cur_size;
		}
		segoff = new_off;

		/*
		 * now see if there is another mapping that straddles
		 * the end of the new mapping
		 */
		cur_entry++;
		map_found = 0;
		done = 0;

		while ((cur_entry < map_entries) && (done == 0)) {
			/* get map entry pointer */
			allocp = cp->c_metadata.md_allocinfo;
			allocp += cur_entry;

			/* get size, start and end offset of current mapping */
			cur_size = allocp->am_size;
			cur_off = allocp->am_start_off;
			cur_end = cur_off + cur_size - 1;

#ifdef CFSDEBUG
			CFS_DEBUG(CFSDEBUG_SUBR)
				printf("cachefs_cluster_allocmap1: entry %d, "
					"cur_off %llx, cur_end %llx\n",
					cur_entry, cur_off, cur_end);
#endif /* CFSDEBUG */

			/* is the current map past the end of the new map? */
			if (cur_end > new_end) {
				done = 1;
				/*
				 *    then the mappings look like this:
				 *		-------------------
				 *   cur_off ->	| current mapping |
				 *		-------------------
				 *	-----------------
				 *	|  new mapping  | <- new_end
				 *	-----------------
				 *		or this
				 *			-------------------
				 *	cur_off ->	| current mapping |
				 *			-------------------
				 *	-----------------
				 *	|  new mapping	| <- new_end
				 *	-----------------
				 */
				/* does it overlap with the new mapping? */
				if ((new_end + 1) >= cur_off) {
					map_found = 1;
				}
			} else {
				cur_entry++;
			}
		}

		if (map_found) {
		/*
		 *    the mapping look like this:
		 *		-------------------
		 *		| current mapping |
		 *		-------------------
		 *	-------------------
		 *	|   new mapping   |
		 *	-------------------
		 */
			segsize = (cur_off - new_off) + cur_size;
		}
	}

	/* is there a previous mapping? */
	if (prev_map) {
		/*
		 *    then the mappings look like this:
		 *    ----------------
		 *    | prev mapping |
		 *    ----------------
		 *		-----------------
		 *		|  new mapping  |
		 *		-----------------
		 *		or this
		 *    ----------------
		 *    | prev mapping |
		 *    ----------------
		 *			-----------------
		 *			|  new mapping  |
		 *			-----------------
		 */
		/* does it overlap with the new mapping? */
		if ((prev_end + 1) >= new_off) {
			segsize = segsize + (new_off - prev_off);
			segoff = prev_off;
		}
	}
	/* make sure the mapping begins on a page boundary */
	*popoffp = (segoff & (u_offset_t)PAGEMASK);

	/*
	 * make sure the popsize will not go past the end of file and
	 * is a multiple of a pagesize
	 */
	if ((*popoffp + segsize) > cp->c_size)
		*popsizep = (u_int)((cp->c_size - *popoffp + PAGEOFFSET) &
			(u_offset_t)PAGEMASK);
	else
		*popsizep = (u_int)((segsize + PAGEOFFSET) &
			(u_offset_t)PAGEMASK);

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_SUBR)
		printf("cachefs_cluster_allocmap: popoff %llx, popsize %x\n",
			*popoffp, *popsizep);
#endif /* CFSDEBUG */
}

/*
 * "populate" a symlink in the cache
 */
int
cachefs_stuffsymlink(cnode_t *cp, caddr_t buf, int buflen)
{
	int error = 0;
	struct fscache *fscp = C_TO_FSCACHE(cp);
	cachefscache_t *cachep = fscp->fs_cache;
	struct cachefs_metadata *mdp = &cp->c_metadata;

	ASSERT(CACHEFS_MUTEX_HELD(&cp->c_statelock));

	if (cp->c_flags & CN_NOCACHE)
		return (ENOENT);

	cp->c_size = buflen;

	/* if can create a fast sym link */
	if (buflen <= C_FSL_SIZE) {
		/* put sym link contents in allocinfo in metadata */
		bzero((caddr_t)mdp->md_allocinfo, C_FSL_SIZE);
		bcopy(buf, (caddr_t)mdp->md_allocinfo, buflen);

		/* give up the front file resources */
		if (mdp->md_rlno) {
			cachefs_removefrontfile(mdp, &cp->c_id, cp->c_filegrp);
			cachefs_rlent_moveto(cachep, CACHEFS_RL_FREE,
			    mdp->md_rlno, 0);
			mdp->md_rlno = 0;
			mdp->md_rltype = CACHEFS_RL_NONE;
		}
		mdp->md_flags |= MD_FASTSYMLNK;
		cp->c_flags &= ~CN_NEED_FRONT_SYNC;
		cp->c_flags |= CN_UPDATED;
		goto out;
	}

	/* else create a sym link in a front file */
	if (cp->c_frontvp == NULL)
		error = cachefs_getfrontfile(cp);
	if (error)
		goto out;

	/* truncate front file */
	error = cachefs_frontfile_size(cp, 0);
	mdp->md_flags &= ~(MD_FASTSYMLNK | MD_POPULATED);
	if (error)
		goto out;

	/* get space for the sym link */
	error = cachefs_allocblocks(cachep, 1, cp->c_metadata.md_rltype);
	if (error)
		goto out;

	/* write the sym link to the front file */
	error = vn_rdwr(UIO_WRITE, cp->c_frontvp, buf, buflen, 0,
	    UIO_SYSSPACE, 0, RLIM_INFINITY, kcred, NULL);
	if (error) {
		cachefs_freeblocks(cachep, 1, cp->c_metadata.md_rltype);
		goto out;
	}

	cp->c_metadata.md_flags |= MD_POPULATED;
	cp->c_flags |= CN_NEED_FRONT_SYNC;
	cp->c_flags |= CN_UPDATED;

out:
	if (CACHEFS_LOG_LOGGING(cachep, CACHEFS_LOG_CSYMLINK))
		cachefs_log_csymlink(cachep, error, fscp->fs_cfsvfsp,
		    &cp->c_metadata.md_cookie, cp->c_id.cid_fileno, buflen);

	return (error);
}

/*
 * Reads the full contents of the symbolic link from the back file system.
 * *bufp is set to a MAXPATHLEN buffer that must be freed when done
 * *buflenp is the length of the link
 */
int
cachefs_readlink_back(cnode_t *cp, cred_t *cr, caddr_t *bufp, int *buflenp)
{
	int error;
	struct uio uio;
	struct iovec iov;
	caddr_t buf;
	fscache_t *fscp = C_TO_FSCACHE(cp);

	ASSERT(CACHEFS_MUTEX_HELD(&cp->c_statelock));

	*bufp = NULL;

	/* get back vnode */
	if (cp->c_backvp == NULL) {
		error = cachefs_getbackvp(fscp, cp);
		if (error)
			return (error);
	}

	/* set up for the readlink */
	bzero((caddr_t)&uio, sizeof (struct uio));
	bzero((caddr_t)&iov, sizeof (struct iovec));
	buf = (char *)cachefs_kmem_alloc(MAXPATHLEN, KM_SLEEP);
	iov.iov_base = buf;
	iov.iov_len = MAXPATHLEN;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_resid = MAXPATHLEN;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_loffset = 0;
	uio.uio_fmode = 0;
	uio.uio_llimit = (rlim64_t)RLIM_INFINITY;

	/* get the link data */
	error = VOP_READLINK(cp->c_backvp, &uio, cr);
	if (error) {
		cachefs_kmem_free(buf, MAXPATHLEN);
	} else {
		*bufp = buf;
		*buflenp = MAXPATHLEN - uio.uio_resid;
	}

	return (error);
}

int
cachefs_getbackvp(struct fscache *fscp, struct cnode *cp)
{
	int error = 0;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_CHEAT | CFSDEBUG_BACK)
		printf("cachefs_getbackvp: ENTER fscp %x cp %x\n",
		    (int)fscp, (int)cp);
#endif
	ASSERT(cp != NULL);
	ASSERT(CACHEFS_MUTEX_HELD(&cp->c_statelock));
	ASSERT(cp->c_backvp == NULL);

	/*
	 * If destroy is set then the last link to a file has been
	 * removed.  Oddly enough NFS will still return a vnode
	 * for the file if the timeout has not expired.
	 * This causes headaches for cachefs_push because the
	 * vnode is really stale.
	 * So we just short circuit the problem here.
	 */
	if (cp->c_flags & CN_DESTROY)
		return (ESTALE);

	ASSERT(fscp->fs_backvfsp);
	if (fscp->fs_backvfsp == NULL)
		return (ETIMEDOUT);
	error = VFS_VGET(fscp->fs_backvfsp, &cp->c_backvp,
	    (struct fid *)&cp->c_cookie);
	if (cp->c_backvp && cp->c_cred &&
	    ((cp->c_flags & CN_NEEDOPEN) || (cp->c_attr.va_type == VREG))) {
		cp->c_flags &= ~CN_NEEDOPEN;

		/*
		 * XXX bob: really should pass in the correct flag,
		 * fortunately nobody pays attention to it
		 */
		error = VOP_OPEN(&cp->c_backvp, 0, cp->c_cred);
		if (error) {
			VN_RELE(cp->c_backvp);
			cp->c_backvp = NULL;
		}
	}

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_GENERAL | CFSDEBUG_BACK) {
		if (error || cp->c_backvp == NULL) {
			printf("Stale cookie cp %x fileno %lu type %d \n",
			    (int)cp, cp->c_id.cid_fileno, CTOV(cp)->v_type);
		}
	}
#endif

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_CHEAT | CFSDEBUG_BACK)
		printf("cachefs_getbackvp: EXIT error = %d\n", error);
#endif
	return (error);
}

int
cachefs_getcookie(vp, cookiep, attrp, cr)
	vnode_t *vp;
	struct fid *cookiep;
	struct vattr *attrp;
	cred_t *cr;
{
	int error = 0;

#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_CHEAT)
		printf("cachefs_getcookie: ENTER vp %x\n", (int)vp);
#endif
	/*
	 * This assumes that the cookie is a full size fid, if we go to
	 * variable length fids we will need to change this.
	 */
	cookiep->fid_len = MAXFIDSZ;
	error = VOP_FID(vp, cookiep);
	if (!error) {
		if (attrp) {
			ASSERT(attrp != NULL);
			attrp->va_mask = AT_ALL;
			error = VOP_GETATTR(vp, attrp, 0, cr);
		}
	} else {
		if (error == ENOSPC) {
			/*
			 * This is an indication that the underlying filesystem
			 * needs a bigger fid.  For now just map to EINVAL.
			 */
			error = EINVAL;
		}
	}
#ifdef CFSDEBUG
	CFS_DEBUG(CFSDEBUG_CHEAT)
		printf("cachefs_getcookie: EXIT error = %d\n", error);
#endif
	return (error);
}

void
cachefs_workq_init(struct cachefs_workq *qp)
{
	qp->wq_head = qp->wq_tail = NULL;
	qp->wq_length =
	    qp->wq_thread_count =
	    qp->wq_max_len =
	    qp->wq_halt_request = 0;
	qp->wq_keepone = 0;
	cv_init(&qp->wq_req_cv, "cachefs async io cv", CV_DEFAULT, NULL);
	cv_init(&qp->wq_halt_cv, "cachefs halt drain cv",
		CV_DEFAULT, NULL);
	cachefs_mutex_init(&qp->wq_queue_lock, "cachefs work q lock",
			MUTEX_DEFAULT, NULL);
}

/*
 * return non-zero if it's `okay' to queue more requests (policy)
 */

static int cachefs_async_max = 512;
static int cachefs_async_count = 0;
cachefs_mutex_t cachefs_async_lock;

int
cachefs_async_okay(void)
{
	/*
	 * a value of -1 for max means to ignore freemem
	 */

	if (cachefs_async_max == -1)
		return (1);

	if (freemem < minfree)
		return (0);

	/*
	 * a value of 0 for max means no arbitrary limit (only `freemen')
	 */

	if (cachefs_async_max == 0)
		return (1);

	ASSERT(cachefs_async_max > 0);

	/*
	 * check the global count against the max.
	 *
	 * we don't need to grab cachefs_async_lock -- we're just
	 * looking, and a little bit of `fuzz' is okay.
	 */

	if (cachefs_async_count >= cachefs_async_max)
		return (0);

	return (1);
}

void
cachefs_async_start(struct cachefs_workq *qp)
{
	struct cachefs_req *rp;
	int left;
	callb_cpr_t cprinfo;

#ifdef CFS_MUTEX_DEBUG
	CALLB_CPR_INIT(&cprinfo,
		&qp->wq_queue_lock.cm_mutex,
		callb_generic_cpr, "cas");
#else
	CALLB_CPR_INIT(&cprinfo,
		&qp->wq_queue_lock,
		callb_generic_cpr, "cas");
#endif
	cachefs_mutex_enter(&qp->wq_queue_lock);
	left = 1;
	for (;;) {
		/* if there are no pending requests */
		if ((qp->wq_head == NULL) && (qp->wq_logwork == 0)) {
			/* see if thread should exit */
			if (qp->wq_halt_request || (left == -1)) {
				if ((qp->wq_thread_count > 1) ||
				    (qp->wq_keepone == 0))
					break;
			}

			/* wake up thread in async_halt if necessary */
			if (qp->wq_halt_request)
				cv_signal(&qp->wq_halt_cv);

			CALLB_CPR_SAFE_BEGIN(&cprinfo);
			/* sleep until there is something to do */
			left = cachefs_cv_timedwait(&qp->wq_req_cv,
				&qp->wq_queue_lock, CFS_ASYNC_TIMEOUT + lbolt);
#ifdef CFS_MUTEX_DEBUG
			CALLB_CPR_SAFE_END(&cprinfo,
				&qp->wq_queue_lock.cm_mutex);
#else
			CALLB_CPR_SAFE_END(&cprinfo,
				&qp->wq_queue_lock);
#endif
			if ((qp->wq_head == NULL) && (qp->wq_logwork == 0))
				continue;
		}
		left = 1;

		if (qp->wq_logwork) {
			qp->wq_logwork = 0;
			cachefs_mutex_exit(&qp->wq_queue_lock);
			cachefs_log_process_queue(qp->wq_cachep, 1);
			cachefs_mutex_enter(&qp->wq_queue_lock);
			continue;
		}

		/* remove request from the list */
		rp = qp->wq_head;
		qp->wq_head = rp->cfs_next;
		if (rp->cfs_next == NULL)
			qp->wq_tail = NULL;

		/* do the request */
		cachefs_mutex_exit(&qp->wq_queue_lock);
		cachefs_do_req(rp);
		cachefs_mutex_enter(&qp->wq_queue_lock);

		/* decrement count of requests */
		qp->wq_length--;
		cachefs_mutex_enter(&cachefs_async_lock);
		--cachefs_async_count;
		cachefs_mutex_exit(&cachefs_async_lock);
	}
	ASSERT(qp->wq_head == NULL);
	qp->wq_thread_count--;
	if (qp->wq_halt_request && qp->wq_thread_count == 0)
		cv_signal(&qp->wq_halt_cv);
	CALLB_CPR_SAFE_BEGIN(&cprinfo);
	CALLB_CPR_EXIT(&cprinfo);
	cachefs_mutex_exit(&qp->wq_queue_lock);
	thread_exit();
	/*NOTREACHED*/
}

/*
 * attempt to halt all the async threads associated with a given workq
 */
int
cachefs_async_halt(struct cachefs_workq *qp, int force)
{
	int error = 0;
	int tend;

	cachefs_mutex_enter(&qp->wq_queue_lock);
	if (force)
		qp->wq_keepone = 0;

	if (qp->wq_thread_count > 0) {
		qp->wq_halt_request = 1;
		cv_broadcast(&qp->wq_req_cv);
		tend = lbolt + (60 * hz);
		(void) cachefs_cv_timedwait(&qp->wq_halt_cv,
			&qp->wq_queue_lock, tend);
		qp->wq_halt_request = 0;
		if (qp->wq_thread_count > 0) {
			if ((qp->wq_thread_count == 1) &&
			    (qp->wq_length == 0) && qp->wq_keepone)
				error = EAGAIN;
			else
				error = EBUSY;
		} else {
			ASSERT(qp->wq_length == 0 && qp->wq_head == NULL);
		}
	}
	cachefs_mutex_exit(&qp->wq_queue_lock);
	return (error);
}

int
cachefs_addqueue(struct cachefs_req *rp, struct cachefs_workq *qp)
{
	int error = 0;

	cachefs_mutex_enter(&qp->wq_queue_lock);
	if (qp->wq_thread_count < cachefs_max_threads) {
		if (qp->wq_thread_count == 0 ||
		    (qp->wq_length >= (qp->wq_thread_count * 2))) {
			if (thread_create(NULL, NULL, cachefs_async_start,
			    (caddr_t)qp, 0, &p0, TS_RUN, 60) != NULL) {
				qp->wq_thread_count++;
			} else {
				if (qp->wq_thread_count == 0) {
					error = EBUSY;
					goto out;
				}
			}
		}
	}
	cachefs_mutex_enter(&rp->cfs_req_lock);
	if (qp->wq_tail)
		qp->wq_tail->cfs_next = rp;
	else
		qp->wq_head = rp;
	qp->wq_tail = rp;
	rp->cfs_next = NULL;
	qp->wq_length++;
	if (qp->wq_length > qp->wq_max_len)
		qp->wq_max_len = qp->wq_length;
	cachefs_mutex_enter(&cachefs_async_lock);
	++cachefs_async_count;
	cachefs_mutex_exit(&cachefs_async_lock);

	cv_signal(&qp->wq_req_cv);
	cachefs_mutex_exit(&rp->cfs_req_lock);
out:
	cachefs_mutex_exit(&qp->wq_queue_lock);
	return (error);
}

void
cachefs_async_putpage(struct cachefs_putpage_req *prp, cred_t *cr)
{
	struct cnode *cp = VTOC(prp->cp_vp);

	(void) VOP_PUTPAGE(prp->cp_vp, prp->cp_off, prp->cp_len,
		prp->cp_flags, cr);

	cachefs_mutex_enter(&cp->c_iomutex);
	if (--cp->c_nio == 0)
		cv_broadcast(&cp->c_iocv);
	if (prp->cp_off == 0 && prp->cp_len == 0 &&
	    (cp->c_ioflags & CIO_PUTPAGES)) {
		cp->c_ioflags &= ~CIO_PUTPAGES;
	}
	cachefs_mutex_exit(&cp->c_iomutex);
}

void
cachefs_async_populate(struct cachefs_populate_req *pop, cred_t *cr)
{
	struct cnode *cp = VTOC(pop->cpop_vp);
	struct fscache *fscp = C_TO_FSCACHE(cp);
	struct filegrp *fgp = cp->c_filegrp;
	int error = 0; /* not returned -- used as a place-holder */
	vnode_t *frontvp = NULL, *backvp = NULL;
	int havelock = 0;
	vattr_t va;

	if (((cp->c_filegrp->fg_flags & CFS_FG_WRITE) == 0) ||
	    (fscp->fs_cdconnected != CFS_CD_CONNECTED)) {
		cachefs_mutex_enter(&cp->c_statelock);
		cp->c_flags &= ~CN_ASYNC_POPULATE;
		cachefs_mutex_exit(&cp->c_statelock);
		return; /* goto out */
	}

	error = cachefs_cd_access(fscp, 0, 0);
	if (error) {
#ifdef CFSDEBUG
		CFS_DEBUG(CFSDEBUG_ASYNCPOP)
			printf("async_pop: cd_access: err %d con %d\n",
			    error, fscp->fs_cdconnected);
#endif /* CFSDEBUG */
		cachefs_mutex_enter(&cp->c_statelock);
		cp->c_flags &= ~CN_ASYNC_POPULATE;
		cachefs_mutex_exit(&cp->c_statelock);
		return; /* goto out */
	}

	/*
	 * grab the statelock for some minimal things
	 */

	rw_enter(&cp->c_rwlock, RW_READER);
	cachefs_mutex_enter(&cp->c_statelock);
	havelock = 1;

	if ((cp->c_flags & CN_ASYNC_POPULATE) == 0)
		goto out;

	/* there can be only one */
	ASSERT((cp->c_flags & CN_ASYNC_POP_WORKING) == 0);
	cp->c_flags |= CN_ASYNC_POP_WORKING;

	if (cp->c_metadata.md_flags & MD_POPULATED)
		goto out;

	if (cp->c_flags & CN_NOCACHE) {
#ifdef CFSDEBUG
		CFS_DEBUG(CFSDEBUG_ASYNCPOP)
			printf("cachefs_async_populate: nocache bit on\n");
#endif /* CFSDEBUG */
		error = EINVAL;
		goto out;
	}

	if (cp->c_frontvp == NULL) {
		if ((cp->c_metadata.md_flags & MD_FILE) == 0) {
			struct cfs_cid cid = cp->c_id;

			cachefs_mutex_exit(&cp->c_statelock);
			havelock = 0;

			/*
			 * if frontfile doesn't exist, drop the lock
			 * to do some of the file creation stuff.
			 */

			if (fgp->fg_flags & CFS_FG_ALLOC_ATTR) {
				error = filegrp_allocattr(fgp);
				if (error != 0)
					goto out;
			}
			if (fgp->fg_flags & CFS_FG_ALLOC_FILE) {
				cachefs_mutex_enter(&fgp->fg_mutex);
				if (fgp->fg_flags & CFS_FG_ALLOC_FILE) {
					if (fgp->fg_header->ach_nffs == 0)
						error = filegrpdir_create(fgp);
					else
						error = filegrpdir_find(fgp);
					if (error != 0) {
						cachefs_mutex_exit(
							&fgp->fg_mutex);
						goto out;
					}
				}
				cachefs_mutex_exit(&fgp->fg_mutex);
			}

			if (fgp->fg_dirvp != NULL) {
				char name[CFS_FRONTFILE_NAME_SIZE];
				struct vattr *attrp;

				attrp = (struct vattr *)
				    cachefs_kmem_zalloc(sizeof (struct vattr),
					KM_SLEEP);
				attrp->va_mode = S_IFREG | 0666;
				attrp->va_uid = 0;
				attrp->va_gid = 0;
				attrp->va_type = VREG;
				attrp->va_size = 0;
				attrp->va_mask =
				    AT_SIZE | AT_TYPE | AT_MODE |
				    AT_UID | AT_GID;

				make_ascii_name(&cid, name);

				(void) VOP_CREATE(fgp->fg_dirvp, name, attrp,
				    EXCL, 0666, &frontvp, kcred, 0);

				cachefs_kmem_free((caddr_t)attrp,
				    sizeof (struct vattr));
			}

			cachefs_mutex_enter(&cp->c_statelock);
			havelock = 1;
		}
		error = cachefs_getfrontfile(cp);
		ASSERT((error != 0) ||
		    (frontvp == NULL) ||
		    (frontvp == cp->c_frontvp));
	}
	if ((error != 0) || (cp->c_frontvp == NULL)) {
#if 0
		printf("cachefs_async_populate: getfrontfile\n");
#endif
		goto out;
	}
	if (frontvp != NULL) {
		VN_RELE(frontvp);
	}
	frontvp = cp->c_frontvp;
	if (frontvp != NULL) {
		VN_HOLD(frontvp);
	} else {
		goto out;
	}

	if (cp->c_backvp == NULL)
		error = cachefs_getbackvp(fscp, cp);
	if ((error != 0) || (cp->c_backvp == NULL)) {
#ifdef CFSDEBUG
		CFS_DEBUG(CFSDEBUG_ASYNCPOP)
			printf("cachefs_async_populate: getbackvp\n");
#endif /* CFSDEBUG */
		goto out;
	}
	backvp = cp->c_backvp;
	if (backvp != NULL) {
		VN_HOLD(backvp);
	} else {
		goto out;
	}

	switch (pop->cpop_vp->v_type) {
	case VREG:
		cachefs_mutex_exit(&cp->c_statelock);
		havelock = 0;
		error = cachefs_async_populate_reg(pop, cr, backvp, frontvp);
		break;
	case VDIR:
		error = cachefs_async_populate_dir(pop, cr, backvp, frontvp);
		cachefs_mutex_exit(&cp->c_statelock);
		havelock = 0;
		break;
	default:
#ifdef CFSDEBUG
		printf("cachefs_async_populate: warning: vnode type = %d\n",
		    pop->cpop_vp->v_type);
		ASSERT(0);
#endif /* CFSDEBUG */
		error = EINVAL;
		break;
	}

	if (error != 0) {
#if 0
		printf("cachefs_async_populate: do the work\n");
#endif
		goto out;
	}

	error = VOP_FSYNC(frontvp, FSYNC, cr);
	if (error != 0) {
#ifdef CFSDEBUG
		CFS_DEBUG(CFSDEBUG_ASYNCPOP)
			printf("cachefs_async_populate: fsync\n");
#endif /* CFSDEBUG */
		goto out;
	}

	/* grab the lock and finish up */
	cachefs_mutex_enter(&cp->c_statelock);
	havelock = 1;

	/* if went nocache while lock was dropped, get out */
	if ((cp->c_flags & CN_NOCACHE) || (cp->c_frontvp == NULL)) {
		error = EINVAL;
		goto out;
	}

	va.va_mask = AT_MTIME;
	error = VOP_GETATTR(cp->c_frontvp, &va, 0, cr);
	if (error) {
#ifdef CFSDEBUG
		CFS_DEBUG(CFSDEBUG_ASYNCPOP)
			printf("cachefs_async_populate: getattr\n");
#endif /* CFSDEBUG */
		goto out;
	}
	cp->c_metadata.md_timestamp = va.va_mtime;
	cp->c_metadata.md_flags |= MD_POPULATED;
	cp->c_metadata.md_flags &= ~MD_INVALREADDIR;
	cp->c_flags |= CN_UPDATED;

out:
	if (! havelock)
		cachefs_mutex_enter(&cp->c_statelock);

	/* see if an error happened behind our backs */
	if ((error == 0) && (cp->c_flags & CN_NOCACHE)) {
#ifdef CFSDEBUG
		CFS_DEBUG(CFSDEBUG_ASYNCPOP)
			printf("cachefs_async_populate: "
			    "nocache behind our backs\n");
#endif /* CFSDEBUG */
		error = EINVAL;
	}

	cp->c_flags &=
	    ~(CN_NEED_FRONT_SYNC |
		CN_POPULATION_PENDING |
		CN_ASYNC_POPULATE |
		CN_ASYNC_POP_WORKING);

	if (error != 0) {
#if 0
		printf("cachefs_async_populate: ino %d error %d\n",
		    cp->c_fileno, error);
#endif
		cachefs_nocache(cp);
	}

	cachefs_mutex_exit(&cp->c_statelock);
	rw_exit(&cp->c_rwlock);
	cachefs_cd_release(fscp);

	if (backvp != NULL) {
		VN_RELE(backvp);
	}
	if (frontvp != NULL) {
		VN_RELE(frontvp);
	}
}

/*
 * only to be called from cachefs_async_populate
 */

static int
cachefs_async_populate_reg(struct cachefs_populate_req *pop, cred_t *cr,
    vnode_t *backvp, vnode_t *frontvp)
{
	struct cnode *cp = VTOC(pop->cpop_vp);
	int error = 0;
	u_offset_t popoff;
	u_int popsize;

	cachefs_cluster_allocmap(pop->cpop_off, &popoff,
	    &popsize, pop->cpop_size, cp);
	if (popsize == 0) {
#ifdef CFSDEBUG
		CFS_DEBUG(CFSDEBUG_ASYNCPOP)
			printf("cachefs_async_populate: popsize == 0\n");
#endif /* CFSDEBUG */
		goto out;
	}

	error = cachefs_populate(cp, popoff, popsize, frontvp, backvp,
	    cp->c_size, cr);
	if (error != 0) {
#ifdef CFSDEBUG
		CFS_DEBUG(CFSDEBUG_ASYNCPOP)
			printf("cachefs_async_populate: cachefs_populate\n");
#endif /* CFSDEBUG */
		goto out;
	}

out:
	return (error);
}

void
cachefs_do_req(struct cachefs_req *rp)
{
	struct cachefscache *cachep;

	cachefs_mutex_enter(&rp->cfs_req_lock);
	switch (rp->cfs_cmd) {
	case CFS_INVALID:
		cmn_err(CE_PANIC,
		    "cachefs_do_req: CFS_INVALID operation on queue\n");
		break; /* wouldn't want to fall through... */
	case CFS_CACHE_SYNC:
		cachep = rp->cfs_req_u.cu_fs_sync.cf_cachep;
		cachefs_cache_sync(cachep);
		break;
	case CFS_IDLE:
		cachefs_cnode_idle(rp->cfs_req_u.cu_idle.ci_vp, rp->cfs_cr);
		break;
	case CFS_PUTPAGE:
		cachefs_async_putpage(&rp->cfs_req_u.cu_putpage, rp->cfs_cr);
		VN_RELE(rp->cfs_req_u.cu_putpage.cp_vp);
		break;
	case CFS_POPULATE:
		cachefs_async_populate(&rp->cfs_req_u.cu_populate, rp->cfs_cr);
		VN_RELE(rp->cfs_req_u.cu_populate.cpop_vp);
		break;
	case CFS_NOOP:
		break;
	default:
		panic("c_do_req: Invalid CFS async operation\n");
	}
	crfree(rp->cfs_cr);
	rp->cfs_cmd = CFS_INVALID;
	cachefs_mutex_exit(&rp->cfs_req_lock);
	kmem_cache_free(cachefs_req_cache, rp);
}




int cachefs_mem_usage = 0;

struct km_wrap {
	int kw_size;
	struct km_wrap *kw_other;
};

cachefs_mutex_t cachefs_kmem_lock;

caddr_t
cachefs_kmem_alloc(int size, int flag)
{
#ifdef DEBUG
	caddr_t mp = NULL;
	struct km_wrap *kwp;
	int n = (size + (2 * sizeof (struct km_wrap)) + 3) & ~3;

	ASSERT(n >= (size + 8));
	mp = kmem_alloc(n, flag);
	if (mp == NULL) {
		return (NULL);
	}
	/*LINTED alignment okay*/
	kwp = (struct km_wrap *)mp;
	kwp->kw_size = n;
	/*LINTED alignment okay*/
	kwp->kw_other = (struct km_wrap *)(mp + n - sizeof (struct km_wrap));
	kwp = (struct km_wrap *)kwp->kw_other;
	kwp->kw_size = n;
	/*LINTED alignment okay*/
	kwp->kw_other = (struct km_wrap *)mp;

	cachefs_mutex_enter(&cachefs_kmem_lock);
	ASSERT(cachefs_mem_usage >= 0);
	cachefs_mem_usage += n;
	cachefs_mutex_exit(&cachefs_kmem_lock);

	return (mp + sizeof (struct km_wrap));
#else /* DEBUG */
	return (kmem_alloc(size, flag));
#endif /* DEBUG */
}

caddr_t
cachefs_kmem_zalloc(int size, int flag)
{
#ifdef DEBUG
	caddr_t mp = NULL;
	struct km_wrap *kwp;
	int n = (size + (2 * sizeof (struct km_wrap)) + 3) & ~3;

	ASSERT(n >= (size + 8));
	mp = kmem_zalloc(n, flag);
	if (mp == NULL) {
		return (NULL);
	}
	/*LINTED alignment okay*/
	kwp = (struct km_wrap *)mp;
	kwp->kw_size = n;
	/*LINTED alignment okay*/
	kwp->kw_other = (struct km_wrap *)(mp + n - sizeof (struct km_wrap));
	kwp = (struct km_wrap *)kwp->kw_other;
	kwp->kw_size = n;
	/*LINTED alignment okay*/
	kwp->kw_other = (struct km_wrap *)mp;

	cachefs_mutex_enter(&cachefs_kmem_lock);
	ASSERT(cachefs_mem_usage >= 0);
	cachefs_mem_usage += n;
	cachefs_mutex_exit(&cachefs_kmem_lock);

	return (mp + sizeof (struct km_wrap));
#else /* DEBUG */
	return (kmem_zalloc(size, flag));
#endif /* DEBUG */
}

void
cachefs_kmem_free(caddr_t mp, int size)
{
#ifdef DEBUG
	struct km_wrap *front_kwp;
	struct km_wrap *back_kwp;
	int n = (size + (2 * sizeof (struct km_wrap)) + 3) & ~3;
	caddr_t p;

	ASSERT(n >= (size + 8));
	/*LINTED alignment okay*/
	front_kwp = (struct km_wrap *)(mp - sizeof (struct km_wrap));
	back_kwp = (struct km_wrap *)
		((caddr_t)front_kwp + n - sizeof (struct km_wrap));

	ASSERT(front_kwp->kw_other == back_kwp);
	ASSERT(front_kwp->kw_size == n);
	ASSERT(back_kwp->kw_other == front_kwp);
	ASSERT(back_kwp->kw_size == n);

	cachefs_mutex_enter(&cachefs_kmem_lock);
	cachefs_mem_usage -= n;
	ASSERT(cachefs_mem_usage >= 0);
	cachefs_mutex_exit(&cachefs_kmem_lock);

	p = (caddr_t)front_kwp;
	front_kwp->kw_size = back_kwp->kw_size = 0;
	front_kwp->kw_other = back_kwp->kw_other = NULL;
	kmem_free(p, n);
#else /* DEBUG */
	kmem_free(mp, size);
#endif /* DEBUG */
}

char *
cachefs_strdup(char *s)
{
	char *rc;

	ASSERT(s != NULL);

	rc = cachefs_kmem_alloc(strlen(s) + 1, KM_SLEEP);
	(void) strcpy(rc, s);

	return (rc);
}

int
cachefs_stats_kstat_snapshot(kstat_t *ksp, void *buf, int rw)
{
	struct fscache *fscp = (struct fscache *)ksp->ks_data;
	cachefscache_t *cachep = fscp->fs_cache;

	if (rw == KSTAT_WRITE) {
		bcopy((caddr_t)buf, (caddr_t)&fscp->fs_stats,
		    sizeof (fscp->fs_stats));
		cachep->c_gc_count = fscp->fs_stats.st_gc_count;
		cachep->c_gc_time = fscp->fs_stats.st_gc_time;
		cachep->c_gc_before = fscp->fs_stats.st_gc_before_atime;
		cachep->c_gc_after = fscp->fs_stats.st_gc_after_atime;
		return (0);
	}

	fscp->fs_stats.st_gc_count = cachep->c_gc_count;
	fscp->fs_stats.st_gc_time = cachep->c_gc_time;
	fscp->fs_stats.st_gc_before_atime = cachep->c_gc_before;
	fscp->fs_stats.st_gc_after_atime = cachep->c_gc_after;
	bcopy((caddr_t)&fscp->fs_stats, (caddr_t)buf,
	    sizeof (fscp->fs_stats));

	return (0);
}

#ifdef DEBUG
cachefs_debug_info_t *
cachefs_debug_save(cachefs_debug_info_t *oldcdb, int chain,
    char *message, u_int flags, int number, void *pointer,
    cachefscache_t *cachep, struct fscache *fscp, struct cnode *cp)
{
	cachefs_debug_info_t *cdb;

	if ((chain) || (oldcdb == NULL))
		cdb = (cachefs_debug_info_t *)
		    cachefs_kmem_zalloc(sizeof (*cdb), KM_SLEEP);
	else
		cdb = oldcdb;
	if (chain)
		cdb->cdb_next = oldcdb;

	if (message != NULL) {
		if (cdb->cdb_message != NULL)
			cachefs_kmem_free(cdb->cdb_message,
			    strlen(cdb->cdb_message) + 1);
		cdb->cdb_message = cachefs_kmem_alloc(strlen(message) + 1,
		    KM_SLEEP);
		(void) strcpy(cdb->cdb_message, message);
	}
	cdb->cdb_flags = flags;
	cdb->cdb_int = number;
	cdb->cdb_pointer = pointer;

	cdb->cdb_count++;

	cdb->cdb_cnode = cp;
	if (cp != NULL) {
		cdb->cdb_frontvp = cp->c_frontvp;
		cdb->cdb_backvp = cp->c_backvp;
	}
	if (fscp != NULL)
		cdb->cdb_fscp = fscp;
	else if (cp != NULL)
		cdb->cdb_fscp = C_TO_FSCACHE(cp);
	if (cachep != NULL)
		cdb->cdb_cachep = cachep;
	else if (cdb->cdb_fscp != NULL)
		cdb->cdb_cachep = cdb->cdb_fscp->fs_cache;

	cdb->cdb_thread = curthread;
	cdb->cdb_timestamp = gethrtime();
	cdb->cdb_depth = getpcstack(cdb->cdb_stack, CACHEFS_DEBUG_DEPTH);

	return (cdb);
}

void
cachefs_debug_show(cachefs_debug_info_t *cdb)
{
	hrtime_t now = gethrtime();
	timestruc_t ts;
	int i;

	while (cdb != NULL) {
		hrt2ts(now - cdb->cdb_timestamp, &ts);
		printf("cdb: %x count: %d timelapse: %ld.%9ld\n",
		    (u_int)cdb, cdb->cdb_count, ts.tv_sec, ts.tv_nsec);
		if (cdb->cdb_message != NULL)
			printf("message: %s", cdb->cdb_message);
		printf("flags: %x int: %d pointer: %x\n",
		    cdb->cdb_flags, cdb->cdb_int, (u_int)cdb->cdb_pointer);

		printf("cnode: %x fscp: %x cachep: %x\n",
		    (u_int)cdb->cdb_cnode,
		    (u_int)cdb->cdb_fscp, (u_int)cdb->cdb_cachep);
		printf("frontvp: %x backvp: %x\n",
		    (u_int)cdb->cdb_frontvp, (u_int)cdb->cdb_backvp);

		printf("thread: %x stack...\n", (u_int)cdb->cdb_thread);
		for (i = 0; i < cdb->cdb_depth; i++) {
			u_int off;
			char *sym;

			sym = kobj_getsymname(cdb->cdb_stack[i], &off);
			printf("%s+%x\n", sym ? sym : "?", off);
		}
		delay(2*hz);
		cdb = cdb->cdb_next;
	}
	debug_enter(NULL);
}
#endif /* DEBUG */

/*
 * Passes a cred pointer, returns a 32 bit checksum.
 */
ulong_t
cachefs_cred_checksum(cred_t *cr)
{
	ulong_t sum;
	ulong_t tmp;
	int xx;

	/*
	 * Disconnected cachefs allows only one writer (as indicated
	 * by the credentials) to a file during the period of
	 * disconnection.  This is to prevent problems when rolling
	 * changes back to the server with determining which writes
	 * to the file went with which creds.
	 *
	 * A simple approach to implement this is to compute a checksum
	 * on the creds and compare for equality.  In theory a well
	 * designed checksum algorithm would have an acceptably small
	 * chance of computing the same checksum for different sets
	 * of credentials.
	 *
	 * Unfortunetly in practice constructing a well designed
	 * checksum algorithm is non-trivial.  In the event that
	 * this algorithm is not sufficient and there is general doubt
	 * that a suitable checksum algorithm can be designed,
	 * this approach can be replaced by doing an actual comparison
	 * of the creds.  To do this keep the offset into the log
	 * file where the first creds are stored.  Put this offset
	 * in the metadata.  Then on first write to a file get the
	 * good creds from the log file and put them in the cnode.
	 */

#define	ADDSUM(VAL) \
	tmp = sum >> 24; \
	sum <<= 8; \
	sum = (sum | tmp) ^ VAL

	sum = 0;
	ADDSUM(cr->cr_uid);
	ADDSUM(cr->cr_gid);
	ADDSUM(cr->cr_ruid);
	ADDSUM(cr->cr_rgid);
	ADDSUM(cr->cr_suid);
	ADDSUM(cr->cr_sgid);
	ADDSUM(cr->cr_ngroups);
	for (xx = 0; xx < cr->cr_ngroups; xx++) {
		ADDSUM(cr->cr_groups[xx]);
	}
	printf("checksum: %x\n", (int)sum);
	return (sum);
}

/*
 * This function compares the values in 2 sets of credentials,
 * if any field compared does not match return 0. Otherwise,
 * return a 1.
 */
int
cachefs_cred_cmp(cred_t *cr1, cred_t *cr2)
{
	if (cr1->cr_uid != cr2->cr_uid) {
	    return (0);
	}
	if (cr1->cr_gid != cr2->cr_gid) {
	    return (0);
	}
	if (cr1->cr_ruid != cr2->cr_ruid) {
	    return (0);
	}
	if (cr1->cr_rgid != cr2->cr_rgid) {
	    return (0);
	}
	if (cr1->cr_suid != cr2->cr_suid) {
	    return (0);
	}
	if (cr1->cr_sgid != cr2->cr_sgid) {
	    return (0);
	}
	if (cr1->cr_ngroups != cr2->cr_ngroups) {
	    return (0);
	}

	return (1);
}

/*
 * Changes the size of the front file.
 * Returns 0 for success or error if cannot set file size.
 * NOCACHE bit is ignored.
 * c_size is ignored.
 * statelock must be held, frontvp must be set.
 * File must be populated if setting to a size other than zero.
 */
int
cachefs_frontfile_size(cnode_t *cp, u_offset_t length)
{
	cachefscache_t *cachep = C_TO_FSCACHE(cp)->fs_cache;
	vattr_t va;
	int nblks, blkdelta;
	int error = 0;
	int alloc = 0;
	struct cachefs_allocmap *allocp;

	ASSERT(CACHEFS_MUTEX_HELD(&cp->c_statelock));
	ASSERT(cp->c_frontvp);

	/* if growing the file, allocate space first, we charge for holes */
	if (length) {
		ASSERT(cp->c_metadata.md_flags & MD_POPULATED);
		nblks = (length + MAXBSIZE - 1) / MAXBSIZE;
		if (nblks > cp->c_metadata.md_frontblks) {
			blkdelta = nblks - cp->c_metadata.md_frontblks;
			error = cachefs_allocblocks(cachep, blkdelta,
			    cp->c_metadata.md_rltype);
			if (error)
				goto out;
			alloc = 1;
		}
	}

	/* change the size of the front file */
	va.va_mask = AT_SIZE;
	va.va_size = length;
	error = VOP_SETATTR(cp->c_frontvp, &va, 0, kcred);
	if (error)
		goto out;

	/* zero out the alloc map */
	bzero((caddr_t)&cp->c_metadata.md_allocinfo,
		cp->c_metadata.md_allocents * sizeof (struct cachefs_allocmap));
	cp->c_metadata.md_allocents = 0;

	if (length == 0) {
		/* free up blocks */
		if (cp->c_metadata.md_frontblks) {
			cachefs_freeblocks(cachep, cp->c_metadata.md_frontblks,
			    cp->c_metadata.md_rltype);
			cp->c_metadata.md_frontblks = 0;
		}
	} else {
		/* update number of blocks if shrinking file */
		nblks = (length + MAXBSIZE - 1) / MAXBSIZE;
		if (nblks < cp->c_metadata.md_frontblks) {
			blkdelta = cp->c_metadata.md_frontblks - nblks;
			cachefs_freeblocks(cachep, blkdelta,
			    cp->c_metadata.md_rltype);
			cp->c_metadata.md_frontblks = nblks;
		}

		/* fix up alloc map to reflect new size */
		allocp = cp->c_metadata.md_allocinfo;
		allocp->am_start_off = 0;
		allocp->am_size = length;
		cp->c_metadata.md_allocents = 1;
	}
	cp->c_flags |= CN_UPDATED | CN_NEED_FRONT_SYNC;

out:
	if (error && alloc)
		cachefs_freeblocks(cachep, blkdelta, cp->c_metadata.md_rltype);
	return (error);
}

/*ARGSUSED*/
int
cachefs_req_create(void *voidp, void *cdrarg, int kmflags)
{
	struct cachefs_req *rp = (struct cachefs_req *)voidp;

	/*
	 * XXX don't do this!  if you need this, you can't use this
	 * constructor.
	 */

	bzero(rp, sizeof (struct cachefs_req));

	cachefs_mutex_init(&rp->cfs_req_lock, "CFS Request Mutex",
	    MUTEX_DEFAULT, NULL);
	return (0);
}

/*ARGSUSED*/
void
cachefs_req_destroy(void *voidp, void *cdrarg)
{
	struct cachefs_req *rp = (struct cachefs_req *)voidp;

	cachefs_mutex_destroy(&rp->cfs_req_lock);
}

#ifdef CFS_MUTEX_DEBUG
void
cachefs_mutex_init(cachefs_mutex_t *mp, char *name,
	kmutex_type_t type, void *arg)
{
	bzero((char *)mp->cm_stack, sizeof (u_int) * DEBUG_STACK_DEPTH);
	mutex_init(&mp->cm_mutex, name, type, arg);
}

void
cachefs_mutex_enter(cachefs_mutex_t *mp)
{
	int depth;

	mutex_enter(&mp->cm_mutex);
	depth = getpcstack(mp->cm_stack, DEBUG_STACK_DEPTH);
	if (depth > DEBUG_STACK_DEPTH) {
		printf("mutex_enter stack depth: %d\n", depth);
	}
}

void
cachefs_mutex_exit(cachefs_mutex_t *mp)
{
	bzero((char *)mp->cm_stack, sizeof (u_int) * DEBUG_STACK_DEPTH);
	mutex_exit(&mp->cm_mutex);
}

void
cachefs_mutex_destroy(cachefs_mutex_t *mp)
{
	mutex_destroy(&mp->cm_mutex);
}

int
cachefs_cv_wait_sig(kcondvar_t *cvp, cachefs_mutex_t *mp)
{
	return (cv_wait_sig(cvp, &mp->cm_mutex));
}

int
cachefs_cv_timedwait(kcondvar_t *cvp, cachefs_mutex_t *mp, long time)
{
	return (cv_timedwait(cvp, &mp->cm_mutex, time));
}
#endif	/* CFS_MUTEX_DEBUG */
