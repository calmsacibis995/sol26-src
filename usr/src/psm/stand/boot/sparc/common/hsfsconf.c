/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */
#ident	"@(#)hsfsconf.c	1.4	96/06/23 SMI"

#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/bootvfs.h>

/*
 * Cachefs support
 */
char *frontfs_fstype = NULL;
char *backfs_fstype = NULL;
char *frontfs_dev = NULL;
char *backfs_dev = NULL;

/*
 *  Function prototypes (Global/Imported)
 */

/* HSFS Support */
extern	struct boot_fs_ops	boot_hsfs_ops;

struct boot_fs_ops *boot_fsw[] = {
	&boot_hsfs_ops,
};

int boot_nfsw = sizeof (boot_fsw) / sizeof (boot_fsw[0]);
static char *fstype = "hsfs";

/*ARGSUSED*/
char *
set_fstype(char *v2path, char *bpath)
{
	set_default_fs(fstype);
	return (fstype);
}
