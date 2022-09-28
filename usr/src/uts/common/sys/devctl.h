/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#ifndef	_SYS_DEVCTL_H
#define	_SYS_DEVCTL_H

#pragma ident	"@(#)devctl.h	1.3	96/10/15 SMI"

/*
 * Device control interfaces
 */
#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	DC_EXCL	1

typedef struct devctl_dummy_struct *devctl_hdl_t;

int
devctl_acquire(char *devfs_path, uint_t flags, devctl_hdl_t *dcp);

void
devctl_release(devctl_hdl_t hdl);

int
devctl_device_offline(devctl_hdl_t hdl);

int
devctl_device_online(devctl_hdl_t hdl);

int
devctl_device_reset(devctl_hdl_t hdl);

int
devctl_device_getstate(devctl_hdl_t hdl, uint_t *statep);

int
devctl_bus_quiesce(devctl_hdl_t hdl);

int
devctl_bus_unquiesce(devctl_hdl_t hdl);

int
devctl_bus_reset(devctl_hdl_t hdl);

int
devctl_bus_resetall(devctl_hdl_t hdl);

int
devctl_bus_getstate(devctl_hdl_t hdl, uint_t *statep);

/*
 * structure used to pass IOCTL data between the libdevice interfaces
 * and nexus driver devctl IOCTL interface.
 *
 * Applications and nexus drivers may not access the contents of this
 * structure directly.  Instead, drivers must use the ndi_dc_XXX(9n)
 * interfaces, while applications must use the interfaces provided by
 * libdevice.so.1.
 */
struct devctl_iocdata {
	uint_t	cmd;			/* ioctl cmd */
	char	*dev_path;		/* Full pathname */
	char	*dev_name;		/* MAXNAMELEN */
	char	*dev_addr;		/* MAXNAMELEN */
	char	*dev_minor;		/* MAXNAMELEN */
	uint_t	*ret_state;		/* return from getstate */
};

#define	DEVCTL_IOC		(0xDC << 16)
#define	DEVCTL_BUS_QUIESCE	(DEVCTL_IOC | 1)
#define	DEVCTL_BUS_UNQUIESCE	(DEVCTL_IOC | 2)
#define	DEVCTL_BUS_RESETALL	(DEVCTL_IOC | 3)
#define	DEVCTL_BUS_RESET	(DEVCTL_IOC | 4)
#define	DEVCTL_BUS_GETSTATE	(DEVCTL_IOC | 5)
#define	DEVCTL_DEVICE_ONLINE	(DEVCTL_IOC | 6)
#define	DEVCTL_DEVICE_OFFLINE	(DEVCTL_IOC | 7)
#define	DEVCTL_DEVICE_GETSTATE	(DEVCTL_IOC | 9)
#define	DEVCTL_DEVICE_RESET	(DEVCTL_IOC | 10)

/*
 * Device and Bus State definitions
 *
 * Device state is returned as a set of bit-flags that indicate the current
 * operational state of a device node.
 *
 * Device nodes for leaf devices only contain state information for the
 * device itself.  Nexus device nodes contain both Bus and Device state
 * information.
 *
 * 	DEVICE_ONLINE  - Device is available for use by the system.  Mutually
 *                       exclusive with DEVICE_OFFLINE.
 *
 *	DEVICE_OFFLINE - Device is unavailable for use by the system.
 *			 Mutually exclusive with DEVICE_ONLINE and DEVICE_BUSY.
 *
 *	DEVICE_DOWN    - Device has been placed in the "DOWN" state by
 *			 its controlling driver.
 *
 *	DEVICE_BUSY    - Device has open instances or nexus has INITALIZED
 *                       children (nexi).  A device in this state is by
 *			 definition Online.
 *
 * Bus state is returned as a set of bit-flags which indicates the
 * operational state of a bus associated with the nexus dev_info node.
 *
 * 	BUS_ACTIVE     - The bus associated with the device node is Active.
 *                       I/O requests from child devices attached to the
 *			 are initiated (or queued for initiation) as they
 *			 are received.
 *
 *	BUS_QUIESCED   - The bus associated with the device node has been
 *			 Quieced. I/O requests from child devices attached
 *			 to the bus are held pending until the bus nexus is
 *			 Unquiesced.
 *
 *	BUS_SHUTDOWN   - The bus associated with the device node has been
 *			 shutdown by the nexus driver.  I/O requests from
 *			 child devices are returned with an error indicating
 *			 the requested operation failed.
 */
#define	DEVICE_ONLINE	0x1
#define	DEVICE_BUSY	0x2
#define	DEVICE_OFFLINE  0x4
#define	DEVICE_DOWN	0x8

#define	BUS_ACTIVE	0x10
#define	BUS_QUIESCED	0x20
#define	BUS_SHUTDOWN	0x40

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DEVCTL_H */
