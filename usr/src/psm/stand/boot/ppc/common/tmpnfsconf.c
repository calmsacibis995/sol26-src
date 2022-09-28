/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */
#pragma ident	"@(#)tmpnfsconf.c	1.5	96/06/26 SMI"

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
extern struct boot_fs_ops boot_nfs_ops;
extern struct boot_fs_ops boot_pcfs_ops;
extern struct boot_fs_ops boot_compfs_ops;

/*
 * Filesystem switch
 */
struct boot_fs_ops *boot_fsw[] = {
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

static char *nfsname = "nfs";
static char *pcfsname = "pcfs";
static char *compfsname = "compfs";

char *
set_fstype(char *v2path, char *bpath)
{
	backfs_fstype = nfsname;
	strcpy(backfsdev, v2path);
	frontfs_fstype = "";
	frontfs_dev = "";
	extendfs_ops = get_fs_ops_pointer(pcfsname);
	origfs_ops = get_fs_ops_pointer(nfsname);
	set_default_fs(compfsname);
	return (nfsname);
}
