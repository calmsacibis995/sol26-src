/*
 * Copyright (c) 1996, Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident   "@(#)cachefs_module.c 1.36     96/09/12 SMI"

#include <sys/errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <rpc/types.h>
#include <sys/mode.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/fs/cachefs_fs.h>

/*
 * This is the loadable module wrapper.
 */
#include <sys/systm.h>
#include <sys/modctl.h>
#include "sys/syscall.h"

extern struct vfsops cachefs_vfsops;
extern time_t time;

static int cachefs_init();

static int cachefs_unloadable = 0; /* you can change w/ kadb or /etc/system */

u_int cachefs_max_apop_inqueue = CACHEFS_MAX_APOP_INQUEUE;

/*
 * this is a list of possible hash table sizes, for the `double
 * hashing' algorithm described in rosen's `elementary number theory
 * and its applications'.  minimally, this needs to be a list of
 * increasing prime integers, terminated by a 0.  ideally, they should
 * be the larger of twin primes; i.e. P and P-2 are both prime.
 */

int cachefs_hash_sizes[] = {5, 2029, 4093, 8089, 16363, 32719, 0};

/*
 * Module linkage information for the kernel.
 */
extern struct mod_ops mod_syscallops;

static struct vfssw vfs_z = {
	CACHEFS_BASETYPE,
	cachefs_init,
	&cachefs_vfsops,
	0
};
extern struct mod_ops mod_fsops;


static struct modlfs modlfs = {
	&mod_fsops,
	"CACHE filesystem",
	&vfs_z
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlfs, NULL
};

int cachefs_module_keepcnt = 0;

char _depends_on[] = "fs/nfs strmod/rpcmod";

_init(void)
{
/*
	if ((status = cachefs_init()) != 0) {
		cmn_err(CE_WARN, "_init: nfs_srvinit failed\n");
		return (status);
	}
*/
	return (mod_install(&modlinkage));
}

_info(modinfop)
	struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}

int
_fini(void)
{
	kstat_t *ksp;

	if (! cachefs_unloadable)
		return (EBUSY);
	if (cachefs_module_keepcnt != 0) {
		return (EBUSY);
	}

	kmem_cache_destroy(cachefs_cnode_cache);
	kmem_cache_destroy(cachefs_req_cache);
	kmem_cache_destroy(cachefs_fscache_cache);
	kmem_cache_destroy(cachefs_filegrp_cache);
	kmem_cache_destroy(cachefs_cache_kmcache);
#ifdef CFSRLDEBUG
	if (cachefs_rl_debug_cache != NULL)
		kmem_cache_destroy(cachefs_rl_debug_cache);
#endif /* CFSRLDEBUG */

	/*
	 * destroy cachefs.0.key kstat.  currently, you can only do a
	 * modunload if cachefs_unloadable is nonzero, and that's
	 * pretty much just for debugging.  however, if there ever
	 * comes a day when cachefs is more freely unloadable
	 * (e.g. the modunload daemon can do it normally), then we'll
	 * have to make changes in the stats_ API.  this is because a
	 * stats_cookie_t holds the id # derived from here, and it
	 * will all go away at modunload time.  thus, the API will
	 * need to somehow be more robust than is currently necessary.
	 *
	 */

	mutex_enter(&kstat_chain_lock);
	ksp = kstat_lookup_byname("cachefs", 0, "key");
	mutex_exit(&kstat_chain_lock);
	if (ksp == NULL)
		cmn_err(CE_WARN, "cachefs _fini: cannot find cachefs.0.key\n");
	else
		kstat_delete(ksp);
	if (cachefs_kstat_key != NULL) {
		cachefs_kstat_key_t *key;
		int i;

		for (i = 0; i < cachefs_kstat_key_n; i++) {
			key = cachefs_kstat_key + i;

			cachefs_kmem_free(key->ks_mountpoint,
			    strlen(key->ks_mountpoint) + 1);
			cachefs_kmem_free(key->ks_backfs,
			    strlen(key->ks_backfs) + 1);
			cachefs_kmem_free(key->ks_cachedir,
			    strlen(key->ks_cachedir) + 1);
			cachefs_kmem_free(key->ks_cacheid,
			    strlen(key->ks_cacheid) + 1);
		}

		cachefs_kmem_free((caddr_t)cachefs_kstat_key,
		    cachefs_kstat_key_n * sizeof (*cachefs_kstat_key));
	}

	return (mod_remove(&modlinkage));
}

extern cachefs_mutex_t cachefs_cachelock;		/* Cache list mutex */
extern cachefs_mutex_t cachefs_newnum_lock;
extern cachefs_mutex_t cachefs_kstat_key_lock;
extern cachefs_mutex_t cachefs_rename_lock;
extern cachefs_mutex_t cachefs_minor_lock;	/* Lock for minor device map */
extern cachefs_mutex_t cachefs_kmem_lock;
extern cachefs_mutex_t cachefs_async_lock;	/* global async work count */
extern int cachefs_major;

/*
 * Cache initialization routine.  This routine should only be called
 * once.  It performs the following tasks:
 *	- Initalize all global locks
 * 	- Call sub-initialization routines (localize access to variables)
 */
static int
cachefs_init(vswp, fstyp)
	struct vfssw *vswp;
	int fstyp;
{
	static boolean_t cachefs_up = B_FALSE;	/* XXX - paranoid */
	extern int cachefsfstyp;
	kstat_t *ksp;

	ASSERT(cachefs_up == B_FALSE);

	cachefs_mutex_init(&cachefs_cachelock, "cachefs cache list lock",
		MUTEX_DEFAULT, NULL);
	cachefs_mutex_init(&cachefs_newnum_lock, "cachefs newnum_lock",
		MUTEX_DEFAULT, NULL);
	cachefs_mutex_init(&cachefs_kstat_key_lock, "cachefs key kstat lock",
		MUTEX_DEFAULT, NULL);
	cachefs_mutex_init(&cachefs_kmem_lock, "Kmem debugging mutex",
	    MUTEX_DEFAULT, NULL);
	cachefs_mutex_init(&cachefs_rename_lock, "cachefs rename lock",
		MUTEX_DEFAULT, NULL);
	cachefs_mutex_init(&cachefs_minor_lock, "minor dev lock",
		MUTEX_DEFAULT, NULL);
	cachefs_mutex_init(&cachefs_async_lock, "global async count lock",
	    MUTEX_DEFAULT, NULL);
#ifdef CFSRLDEBUG
	cachefs_mutex_init(&cachefs_rl_debug_mutex, "rl debug lock",
	    MUTEX_DEFAULT, NULL);
#endif /* CFSRLDEBUG */

	/*
	 * set up kmem_cache entities
	 */

	cachefs_cnode_cache = kmem_cache_create("cachefs_cnode_cache",
	    sizeof (struct cnode), 0, NULL, NULL, NULL, NULL, NULL, 0);
	cachefs_req_cache = kmem_cache_create("cachefs_async_request",
	    sizeof (struct cachefs_req), 0,
	    cachefs_req_create, cachefs_req_destroy, NULL, NULL, NULL, 0);
	cachefs_fscache_cache = kmem_cache_create("cachefs_fscache",
	    sizeof (fscache_t), 0, NULL, NULL, NULL, NULL, NULL, 0);
	cachefs_filegrp_cache = kmem_cache_create("cachefs_filegrp",
	    sizeof (filegrp_t), 0,
	    filegrp_cache_create, filegrp_cache_destroy, NULL, NULL, NULL, 0);
	cachefs_cache_kmcache = kmem_cache_create("cachefs_cache_t",
	    sizeof (cachefscache_t), 0, NULL, NULL, NULL, NULL, NULL, 0);

	/*
	 * set up the cachefs.0.key kstat
	 */

	cachefs_kstat_key = NULL;
	cachefs_kstat_key_n = 0;
	ksp = kstat_create("cachefs", 0, "key", "misc", KSTAT_TYPE_RAW, 1,
	    KSTAT_FLAG_VIRTUAL | KSTAT_FLAG_VAR_SIZE);
	if (ksp != NULL) {
		ksp->ks_data = &cachefs_kstat_key;
		ksp->ks_update = cachefs_kstat_key_update;
		ksp->ks_snapshot = cachefs_kstat_key_snapshot;
		ksp->ks_lock = CFSMUTEX2KMUTEX(&cachefs_kstat_key_lock);
		kstat_install(ksp);
	}

	/*
	 * Assign unique major number for all nfs mounts
	 */

	if ((cachefs_major = getudev()) == -1) {
		cmn_err(CE_WARN,
			"cachefs: init: can't get unique device number");
		cachefs_major = 0;
	}
	cachefs_up = B_TRUE;
#ifdef CFSRLDEBUG
	cachefs_dbvalid = time;
#endif /* CFSRLDEBUG */
	vswp->vsw_vfsops = &cachefs_vfsops;
	cachefsfstyp = fstyp;
	return (0);
}
