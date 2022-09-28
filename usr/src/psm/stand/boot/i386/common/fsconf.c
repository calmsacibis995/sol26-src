/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident "@(#)fsconf.c	1.8	96/06/23 SMI"

#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/bootvfs.h>
#include <sys/bootcfs.h>

/*
 *  Function prototypes (Global/Imported)
 */

/* UFS Support */
extern	struct boot_fs_ops	boot_ufs_ops;

/* PCFS Support */
extern	struct boot_fs_ops	boot_pcfs_ops;

/* COMPFS Support */
extern	struct boot_fs_ops	boot_compfs_ops;

/* NFS Support */
extern	struct boot_fs_ops	boot_nfs_ops;

/* HSFS Support */
extern struct boot_fs_ops boot_hsfs_ops;

/* CACHEFS Support */
extern struct boot_fs_ops boot_cachefs_ops;


struct boot_fs_ops *boot_fsw[] = {
	&boot_ufs_ops,
	&boot_pcfs_ops,
	&boot_compfs_ops,
	&boot_nfs_ops,
	&boot_hsfs_ops,
	&boot_cachefs_ops
};

int boot_nfsw = sizeof (boot_fsw) / sizeof (boot_fsw[0]);
struct boot_fs_ops *extendfs_ops = NULL;	/* extended file system */
struct boot_fs_ops *origfs_ops = NULL;		/* original file system */

static char *compfsname = "compfs";
static char *pcfsname = "pcfs";
static char *ufsname  = "ufs";

/* These will be needed by CACHEFS */
#ifdef HAS_CACHEFS
static char *cfsname  = "cachefs";
static char *nfsname  = "nfs";
#endif /* HAS_CACHEFS */

int	nfs_readsize = 1000;	/* NFS read will time out if 8192 is used. */

/*
 * Cachefs data
 * backfs data describes the back filesystem
 */
static char backfsdev[256];		/* OBP_MAXPATHLEN */
static char frontfsdev[256];		/* OBP_MAXPATHLEN */
char *backfs_dev = backfsdev;
char *backfs_fstype = "";
char *frontfs_dev = frontfsdev;
char *frontfs_fstype = "";

/*ARGSUSED*/
char *
set_fstype(char *v2path, char *bpath)
{
	char *fstype;

	extendfs_ops = get_fs_ops_pointer(pcfsname);
	origfs_ops = get_fs_ops_pointer(ufsname);
	fstype = ufsname;

	/*
	 * jhd
	 * When we are booting a Cache-Only-Client, the following
	 * property assignments are needed:
	 * 1) set backfs_dev -> network device path, backfs_fstype = nfsname,
	 *    which are read from /.cachefsinfo
	 *
	 * 2) set frontfs_dev -> boot device path, frontfs_fstype = ufsname
	 *
	 * 3) set fstype = cfsname
	 *
	 * See ../../{sparc,ppc}/common/fsconf.c for examples
	 */

	set_default_fs(compfsname);

	return (fstype);
}
