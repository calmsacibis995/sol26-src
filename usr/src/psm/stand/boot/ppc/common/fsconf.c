/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */
#pragma ident	"@(#)fsconf.c	1.4	96/06/23 SMI"

#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/obpdefs.h>
#include <sys/stat.h>
#include <sys/bootvfs.h>
#include <sys/bootcfs.h>
#include <sys/bootdebug.h>
#include <sys/promif.h>
#include <sys/salib.h>

extern int verbosemode;

/*
 * boot module support
 */
extern struct boot_fs_ops boot_cachefs_ops;
extern struct boot_fs_ops boot_ufs_ops;
extern struct boot_fs_ops boot_nfs_ops;
extern struct boot_fs_ops boot_pcfs_ops;
extern struct boot_fs_ops boot_compfs_ops;

extern void translate_v2tov0(char *bkdev, char *npath);

/*
 * Filesystem switch
 */
struct boot_fs_ops *boot_fsw[] = {
	&boot_cachefs_ops,
	&boot_ufs_ops,
	&boot_nfs_ops,
	&boot_pcfs_ops,
	&boot_compfs_ops,
};
int boot_nfsw = sizeof (boot_fsw) / sizeof (boot_fsw[0]);

/*
 * COMPFS support
 */
struct boot_fs_ops *extendfs_ops = NULL;
struct boot_fs_ops *origfs_ops = NULL;

/*
 * CACHEFS support
 */
static char backfsdev[OBP_MAXPATHLEN];
static char frontfsdev[OBP_MAXPATHLEN];
char *backfs_dev = backfsdev;
char *backfs_fstype = NULL;
char *frontfs_dev = frontfsdev;
char *frontfs_fstype = NULL;

int nfs_readsize = 1024;

static char *ufsname = "ufs";
static char *nfsname = "nfs";
static char *cfsname = "cachefs";
static char *pcfsname = "pcfs";
static char *compfsname = "compfs";

char *
set_fstype(char *v2path, char *bpath)
{
	char netpath[OBP_MAXPATHLEN];
	char *fstype = NULL;
	int hascfs = 0;

	/*
	 * if we are booting over the network, we are performing either
	 * a diskless boot or a Cache-Only-Client boot from the back
	 * filesystem.  Prepare for either case.
	 */
	if (prom_devicetype(prom_getphandle(prom_open(prom_bootpath())),
	    "network")) {
		fstype = nfsname;
		backfs_fstype = nfsname;
		strcpy(backfsdev, v2path);
		frontfs_fstype = "";
		frontfs_dev = "";
	} else {
		/*
		 * booted off the local disk
		 * if UFS, and the /.cachefsinfo file is here, this
		 * is a cachefs boot.  Read the backfs fstype and
		 * device path from the /.cachefsinfo file.
		 */

		if (has_ufs_fs(bpath, &hascfs) == SUCCESS) {
			fstype = ufsname;
			if (hascfs == 1) {
				fstype = cfsname;
				frontfs_fstype = ufsname;
				strcpy(frontfsdev, v2path);

				/*
				 * Read the Cache-Only-Client /.cachefsinfo
				 * file for the name of the back filesystem
				 * device (the network device)
				 */
				if (get_backfsinfo(bpath, backfsdev) != SUCCESS)
					prom_panic("ufsboot: Corrupted"
					" cachefs info file\n"
					"Please reinitialize with:"
					"\t\"boot net -f\"\n");
			}
		} else {
			prom_panic("ufsboot: cannot determine filesystem type "
				" of root device.");
		}
	}

	/*
	 * Everything is now ready!
	 */
	if (hascfs == 0) {
		/*
		 * We are doing a 'regular' NFS or UFS boot.
		 */
		extendfs_ops = get_fs_ops_pointer(pcfsname);
		origfs_ops = get_fs_ops_pointer(fstype);
		set_default_fs(compfsname);
	} else {
		/*
		 * CFS boot from frontfs.
		 */
		if (verbosemode) {
			printf("ufsboot: cachefs\n");
			printf("\tfrontfs device=%s\n", frontfsdev);
			printf("\tbackfs device=%s\n", backfsdev);
		}

		set_default_fs(backfs_fstype);

		/*
		 * undo any device name translations on the
		 * network (backfs) device path.
		 * mount back filesystem for cache miss processing
		 */
		translate_v2tov0(backfsdev, netpath);

		if (mountroot(netpath) != SUCCESS)
			prom_panic("ufsboot: Failed to mount back "
			    "filesystem\n");

		frontfs_ops = get_fs_ops_pointer(frontfs_fstype);
		backfs_ops = get_fs_ops_pointer(backfs_fstype);
		set_default_fs(cfsname);
	}
	return (fstype);
}
