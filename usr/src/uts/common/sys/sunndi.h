/*
 * Copyright (c) 1990-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SUNNDI_H
#define	_SYS_SUNNDI_H

#pragma ident	"@(#)sunndi.h	1.9	96/10/15 SMI"

/*
 * Sun Specific NDI definitions
 */


#include <sys/esunddi.h>
#include <sys/sunddi.h>
#include <sys/obpdefs.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _KERNEL

#define	NDI_SUCCESS	0	/* successful return */
#define	NDI_FAILURE	-1	/* unsuccessful return */
#define	NDI_NOMEM	-2	/* failed to allocate resources */
#define	NDI_BADHANDLE	-3	/* bad handle passed to in function */
#define	NDI_FAULT	-4	/* fault during copyin/copyout */
#define	NDI_BUSY	-5	/* device busy - could not offline */

/*
 * Property functions:   See also, ddipropdefs.h.
 *			In general, the underlying driver MUST be held
 *			to call it's property functions.
 */

/*
 * Used to create, modify, and lookup integer properties
 */
int
ndi_prop_update_int(dev_t match_dev, dev_info_t *dip, char *name, int data);

int
ndi_prop_update_int_array(dev_t match_dev, dev_info_t *dip, char *name,
    int *data, u_int nelements);

/*
 * Used to create, modify, and lookup string properties
 */
int
ndi_prop_update_string(dev_t match_dev, dev_info_t *dip, char *name,
    char *data);

int
ndi_prop_update_string_array(dev_t match_dev, dev_info_t *dip,
    char *name, char **data, u_int nelements);

/*
 * Used to create, modify, and lookup byte properties
 */
int
ndi_prop_update_byte_array(dev_t match_dev, dev_info_t *dip,
    char *name, u_char *data, u_int nelements);

/*
 * Used to remove properties
 */
int
ndi_prop_remove(dev_t dev, dev_info_t *dip, char *name);

void
ndi_prop_remove_all(dev_info_t *dip);

/*
 * Nexus Driver Functions
 */
/*
 * Allocate and initialize a new dev_info structure.
 * This routine will often be called at interrupt time by a nexus in
 * response to a hotplug event, therefore memory allocations are
 * not allowed to sleep.
 */
int
ndi_devi_alloc(dev_info_t *parent, char *node_name, dnode_t nodeid,
    dev_info_t **ret_dip);

/*
 * Remove an initialized (but not yet attached) dev_info
 * node from it's parent.
 */
int
ndi_devi_free(dev_info_t *dip);

/*
 * Schedule an initialized dev_info node to be attached by the hotplug
 * work thread.
 *
 * Flags:	0
 */
int
ndi_devi_attach_driver(dev_info_t *dip, uint_t flags);

/*
 * Place the devinfo in the ONLINE state, allowing deferred
 * attach requests to re-attach the device instance.
 *
 * Flags:
 *	NDI_ONLINE_ATTACH - Attach driver to devinfo node when placing
 *			    the device Online.
 */
int
ndi_devi_online(dev_info_t *dip, uint_t flags);

/*
 * Take a device node "Offline".
 *
 * Offline means to detach the device instance from the bound
 * driver and setting the devinfo state to prevent deferred attach
 * from re-attaching the device instance.
 *
 * Flags:
 *	NDI_DEVI_REMOVE	- Remove the node from the devinfo tree after
 *			  first taking it Offline.
 */

#define	NDI_DEVI_REMOVE		0x01
#define	NDI_ONLINE_ATTACH	0x02

int
ndi_devi_offline(dev_info_t *dip, uint_t flags);

/*
 * Find the child dev_info node of parent nexus 'p' whose name
 * matches "cname"@"caddr".
 */
dev_info_t *
ndi_devi_find(dev_info_t *p, char *cname, char *caddr);

/*
 * Copy in the devctl IOCTL data structure and the strings referenced
 * by the structure.
 *
 * Convenience functions for use by nexus drivers as part of the
 * implementation of devctl IOCTL handling.
 */
int
ndi_dc_allochdl(void *iocarg, struct devctl_iocdata **rdcp);

void
ndi_dc_freehdl(struct devctl_iocdata *dcp);

char *
ndi_dc_getpath(struct devctl_iocdata *dcp);

char *
ndi_dc_getname(struct devctl_iocdata *dcp);

char *
ndi_dc_getaddr(struct devctl_iocdata *dcp);

char *
ndi_dc_getminorname(struct devctl_iocdata *dcp);

int
ndi_dc_return_dev_state(dev_info_t *dip, struct devctl_iocdata *dcp);

int
ndi_dc_return_bus_state(dev_info_t *dip, struct devctl_iocdata *dcp);

int
ndi_get_bus_state(dev_info_t *dip, uint_t *rstate);

int
ndi_set_bus_state(dev_info_t *dip, uint_t state);

/*
 * Post an event notification up the device tree hierarchy to the
 * parent nexus, until claimed by a bus nexus driver or the top
 * of the dev_info tree is reached.
 */
int
ndi_post_event(dev_info_t *dip, dev_info_t *rdip, ddi_eventcookie_t eventhdl,
    void *impl_data);

/*
 * Called by a bus nexus driver's implementation of the
 * (*bus_add_eventcall)() interface up the device tree hierarchy,
 * until claimed by a bus nexus driver or the top of the dev_info
 * tree is reached.
 */
int
ndi_busop_add_eventcall(dev_info_t *dip, dev_info_t *rdip,
    ddi_eventcookie_t eventhdl, int (*callback)(), void *arg);

/*
 * Called by a bus nexus driver's implementation of the
 * (*bus_remove_eventcall)() interface up the device tree hierarchy,
 * until claimed by a bus nexus driver or the top of the dev_info
 * tree is reached.
 */
int
ndi_busop_remove_eventcall(dev_info_t *dip, dev_info_t *rdip,
    ddi_eventcookie_t eventhdl);

/*
 * Called by a bus nexus driver's implementation of the
 * (*bus_get_eventcookie)() interface up the device tree hierarchy,
 * until claimed by a bus nexus driver or the top of the dev_info
 * tree is reached.
 */
int
ndi_busop_get_eventcookie(dev_info_t *dip, dev_info_t *rdip, char *name,
    ddi_eventcookie_t *event_cookiep, ddi_plevel_t *plevelp,
    ddi_iblock_cookie_t *iblock_cookiep);

/*
 * Called by a bus nexus driver.
 * Given a string that contains the name of a bus-specific event, lookup
 * or create a unique handle for the event "name".
 */
ddi_eventcookie_t
ndi_event_getcookie(char *name);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SUNNDI_H */
