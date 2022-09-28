/*
 * Copyright (c) 1993 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_DEVICE_INFO_H
#define	_DEVICE_INFO_H

#pragma ident	"@(#)device_info.h	1.4	96/06/03 SMI"

#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* error return values */
#define	DEVFS_ERR	-1	/* operation not successful */
#define	DEVFS_INVAL	-2	/* invalid argument */
#define	DEVFS_NOMEM	-3	/* out of memory */
#define	DEVFS_PERM	-4 	/* permission denied - not root */
#define	DEVFS_NOTSUP	-5	/* operation not supported */
#define	DEVFS_LIMIT	-6	/* exceeded maximum size of property value */

/*
 * for devfs_set_boot_dev()
 * default behavior is to translate the input logical device name
 * to most compact prom name(i.e. a prom alias, if one exists)
 * as possible.  And to prepend the new entry to the existing
 * list.
 */

/* perform no translation on the input device path */
#define	BOOTDEV_LITERAL		0x1
/* convert the input device path only a prom device path; not an alias */
#define	BOOTDEV_PROMDEV		0x2
/* overwrite the existing entry in boot-device - default is to prepend */
#define	BOOTDEV_OVERWRITE	0x4

/*
 * for devfs_get_prom_names()
 * returns a list of prom names for a given logical device name.
 * the list is sorted first in order of exact aliases, inexact alias
 * matches (where an option override was needed), and finally the
 * equivalent prom device path.  Each sublist is sorted in collating
 * order.
 */
#define	BOOTDEV_NO_PROM_PATH		0x1
#define	BOOTDEV_NO_INEXACT_ALIAS	0x2
#define	BOOTDEV_NO_EXACT_ALIAS		0x4

/* for devfs_get_boot_dev() */
struct boot_dev {
	char *bootdev_element;	/* an entry from the boot-device variable */
	char **bootdev_trans;	/* 0 or more logical dev translations */
};

/* prototypes */
extern int devfs_find(const char *devtype,
    void (*found)(const char *, const char *,
    const dev_info_t *, struct ddi_minor_data *minor_data,
    struct ddi_minor_data *alias_data), int check_aliases);

extern int devfs_find_all(void (*found)(const char *,
    const dev_info_t *));

extern boolean_t devfs_iscbdriver(const dev_info_t *);
extern boolean_t devfs_is_nexus_driver(const dev_info_t *);

extern const char *local_addr(caddr_t addr);

/* return the driver for a given device path */
extern int devfs_path_to_drv(char *devfs_path, char *drv_buf);

/* convert a logical or physical device name to the equivalent prom path */
extern int devfs_dev_to_prom_name(char *, char *);

/* return the driver name after resolving any aliases */
extern char *devfs_resolve_aliases(char *drv);

/* set the boot-device configuration variable */
extern int devfs_bootdev_set_list(const char *, const u_int);

/* is the boot-device variable modifiable on this platform? */
extern int devfs_bootdev_modifiable(void);

/*
 * retrieve the boot-device config variable and corresponding logical
 * device names
 */
extern int devfs_bootdev_get_list(const char *, struct boot_dev ***);
/*
 * free a list of bootdev structs
 */
extern void devfs_bootdev_free_list(struct boot_dev **);
/*
 * given a logical device name, return a list of equivalent
 * prom names (aliases and device paths)
 */
extern int devfs_get_prom_names(const char *, u_int, char ***);

#ifdef	__cplusplus
}
#endif

#endif	/* _DEVICE_INFO_H */
