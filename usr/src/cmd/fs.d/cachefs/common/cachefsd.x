/*
 *  Copyright (c) 1996, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

/* -----------------------------------------------------------------
 *
 *			cachefsd.x
 *
 * Rpcgen file for generating cachefsd interface.
 */

/* #ident	"@(#)cachefsd.x	1.6	96/01/16 SMI" */


/*
 * List of caches.
 */
struct cachefsd_caches_id {
	int		cci_cacheid;		/* id of cache */
	string		cci_name<>;		/* pathname of cache */
};
struct cachefsd_caches_return {
	u_long			ccr_modify;	/* changes when modified */
	cachefsd_caches_id	ccr_ids<>;	/* list of caches */
};

/*
 * Stats about a single cache.
 * All sizes are in 1K blocks.
 */
struct cachefsd_cache_status {
	int		ccs_id;			/* id of cache */
	string		ccs_name<>;		/* name of cache */
	longlong_t	ccs_size;		/* size of cache */
	longlong_t	ccs_lrusize;		/* size of lru files */
	longlong_t	ccs_packsize;		/* size of packed files */
	longlong_t	ccs_freesize;		/* size of free space */
	u_long		ccs_lrutime;		/* time of oldest item on lru */
	u_long		ccs_modify;		/* changes when modified */
};

/*
 * List of file systems in a cache.
 */
struct cachefsd_mount {
	int		cm_fsid;		/* id of file system */
	string		cm_name<>;		/* name of file system */
};
struct cachefsd_mount_returns {
	cachefsd_mount	cmr_names<>;		/* list of file systems */
	u_long		cmr_modify;		/* changes when modified */
	int		cmr_error;		/* 0 if no error */
};

/*
 * Stats about a single file system in a cache.
 */
struct cachefsd_mount_stat_args {
	int		cma_cacheid;		/* id of cache */
	int		cma_fsid;		/* id of file system */
};
struct cachefsd_mount_stat {
	int		cms_cacheid;		/* id of cache */
	int		cms_fsid;		/* id of file system */
	string		cms_name<>;		/* name of file system */
	string		cms_backfs<>;		/* back file system */
	string		cms_mountpt<>;		/* most recent mount point */
	string		cms_backfstype<>;	/* type of back file system */
	string		cms_writemode<>;	/* write mode */
	string		cms_options<>;		/* remaining options */
	int		cms_mounted;		/* 1 if mounted */
	int		cms_connected;		/* 1 if connected */
	int		cms_reconcile;		/* 1 if reconcile in progress */
	int		cms_changes;		/* 1 if changes to push back */
	u_long		cms_time_state;		/* time of state change */
	u_long		cms_mnttime;		/* time of last mount/umount */
	u_long		cms_modify;		/* changes when modified */
};

/*
 * Sent by mount command to indicate a new mounted file system.
 */
struct cachefsd_fs_mounted {
	string		mt_cachedir<>;		/* cache directory path */
	string		mt_cacheid<>;		/* cache id */
};

/*
 * Sets file system to simulate disconnection.
 */
struct cachefsd_disconnection_args {
	string		cda_mntpt<>;		/* mntpt of file system */
	int		cda_disconnect;		/* 1 disconnect, 0 connect */
};

/*
 * -----------------------------------------------------------------
 * This is the definition of the routines supported by the service.
 */
program CACHEFSDPROG {
	version CACHEFSDVERS {
		cachefsd_caches_return CACHEFSD_CACHES(void) = 1;
		cachefsd_cache_status CACHEFSD_CACHE_STATUS(int id) = 2;
		cachefsd_mount_returns CACHEFSD_MOUNTS(int id) = 3;
		cachefsd_mount_stat CACHEFSD_MOUNT_STAT(
		    struct cachefsd_mount_stat_args) = 4;
		void CACHEFSD_FS_MOUNTED(struct cachefsd_fs_mounted) = 5;
		int CACHEFSD_FS_UNMOUNTED(string mntpt) = 6;
		int CACHEFSD_DISCONNECTION(struct cachefsd_disconnection_args)
		    = 7;
	} = 1;
} = 100235;

#ifdef RPC_HDR
%
#endif /* RPC_HDR */
