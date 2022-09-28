/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident   "@(#)devctl.c 1.7     96/10/18 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "libdevice.h"

static int libdevice_db = 0;
#ifdef	DEBUG
#define	dprintf(args) if (libdevice_db) printf args
#else
#define	dprintf
#endif

const char *devctl_minordev = ":devctl";

struct devctl_hdl {
	char	*opath;
	int	fd;
	uint_t	flags;
	uint_t	num_components;
	uint_t	cur_component;
	char	**components;
};
#define	DCP(x)	((struct devctl_hdl *)(x))

static uint_t dn_path_to_components(char *, char ***names);
static char *dn_components_to_path(uint_t, char **);
static void dn_parse_component(char *, char **, char **, char **);
static void free_components(int, char **);
static int dc_childcmd(uint_t, struct devctl_hdl *, uint_t *);
static int dc_buscmd(uint_t, struct devctl_hdl *, uint_t *);


/*
 * release the devctl handle allocated by devctl_acquire()
 */
void
devctl_release(devctl_hdl_t dcp)
{

	if (DCP(dcp)->fd != 0 && DCP(dcp)->fd != -1)
		(void) close(DCP(dcp)->fd);

	if (DCP(dcp)->num_components != 0)
		free_components(DCP(dcp)->num_components, DCP(dcp)->components);

	if (DCP(dcp)->opath != NULL)
		free(DCP(dcp)->opath);

	free(dcp);
}

/*
 * given a devfs (/devices) pathname to a leaf device, access the
 * bus nexus device exporting the Devctl device control interface
 * and return a handle to be passed to the remaining devctl_XXX
 * functions
 */
int
devctl_acquire(char *devfs_path, uint_t flags, devctl_hdl_t *rdcp)
{
	char *iocpath;
	struct devctl_hdl *dcp;
	uint_t otype = O_RDWR;


	/*
	 * perform basic sanity checks on the parameters
	 * the only flag we expect is the exclusive access flag (DC_EXCL)
	 */
	if ((devfs_path == NULL) || (rdcp == NULL) ||
	    ((flags != 0) && (flags != DC_EXCL))) {
		errno = EINVAL;
		return (-1);
	}

	if (strlen(devfs_path) > MAXPATHLEN - 1) {
		errno = EINVAL;
		return (-1);
	}

	dprintf(("devctl_acquire dev (%s) flags (0x%x)\n", devfs_path, flags));

	if ((dcp = calloc(1, sizeof (*dcp))) == NULL) {
		dprintf(("devctl_acquire: calloc failure\n"));
		errno = ENOMEM;
		return (-1);
	}

	if (flags & DC_EXCL)
		otype |= O_EXCL;

	/*
	 * take the devfs pathname and break it into the individual
	 * components.
	 */
	dcp->num_components = dn_path_to_components(devfs_path,
	    &dcp->components);
	if (dcp->num_components == 0) {
		devctl_release((devctl_hdl_t)dcp);
		dprintf(("devctl_acquire: pathname parse failed\n"));
		errno = EINVAL;
		return (-1);
	}

	/* save copy of the original path */
	if ((dcp->opath = strdup(devfs_path)) == NULL) {
		devctl_release((devctl_hdl_t)dcp);
		errno = EINVAL;
		return (-1);
	}

	/*
	 * construct a pathname to the bus nexus driver that exports
	 * the ":devctl" control interface
	 */
	iocpath = dn_components_to_path(dcp->num_components - 1,
	    dcp->components);
	if (iocpath == NULL) {
		devctl_release((devctl_hdl_t)dcp);
		errno = ENOMEM;
		return (-1);
	}

	if ((strlen(iocpath) + strlen(devctl_minordev)) > MAXPATHLEN - 1) {
		free(iocpath);
		devctl_release((devctl_hdl_t)dcp);
		errno = EINVAL;
		return (-1);
	}
	(void) strcat(iocpath, ":devctl");

	dprintf(("devctl_acquire: ioctl device path (%s)\n", iocpath));

	/*
	 * open the bus nexus ":devctl" interface
	 * we can fail because:
	 *	1) no such device (ENXIO)
	 *	2) Already open   (EBUSY)
	 *	3) No permission  (EPERM)
	 */
	dcp->fd = open(iocpath, otype);
	if (dcp->fd == -1) {
		dprintf(("devctl_acquire: open of (%s) failed\n", iocpath));
		free(iocpath);
		devctl_release((devctl_hdl_t)dcp);
		return (-1);
	}

	/*
	 * release the space allocated for the pathname and return the
	 * handle to the caller
	 */
	free(iocpath);
	*rdcp = (devctl_hdl_t)dcp;
	return (0);
}

/*
 * place device "device_path" online
 */
int
devctl_device_online(devctl_hdl_t dcp)
{
	int  rv;

	rv = dc_childcmd(DEVCTL_DEVICE_ONLINE, DCP(dcp), NULL);
	return (rv);
}

/*
 * take device "device_path" offline
 */
int
devctl_device_offline(devctl_hdl_t dcp)
{
	int  rv;

	rv = dc_childcmd(DEVCTL_DEVICE_OFFLINE, DCP(dcp), NULL);
	return (rv);
}


int
devctl_bus_quiesce(devctl_hdl_t dcp)
{
	int  rv;

	rv = dc_buscmd(DEVCTL_BUS_QUIESCE, DCP(dcp), NULL);
	return (rv);
}

int
devctl_bus_unquiesce(devctl_hdl_t dcp)
{
	int  rv;

	rv = dc_buscmd(DEVCTL_BUS_UNQUIESCE, DCP(dcp), NULL);
	return (rv);
}

int
devctl_bus_reset(devctl_hdl_t dcp)
{
	int  rv;

	rv = dc_buscmd(DEVCTL_BUS_RESET, DCP(dcp), NULL);
	return (rv);
}

int
devctl_bus_resetall(devctl_hdl_t dcp)
{
	int  rv;

	rv = dc_buscmd(DEVCTL_BUS_RESETALL, DCP(dcp), NULL);
	return (rv);
}

int
devctl_device_reset(devctl_hdl_t dcp)
{
	int  rv;

	rv = dc_childcmd(DEVCTL_DEVICE_RESET, DCP(dcp), NULL);
	return (rv);
}

int
devctl_device_getstate(devctl_hdl_t dcp, uint_t *devstate)
{
	int  rv;
	uint_t device_state;

	rv = dc_childcmd(DEVCTL_DEVICE_GETSTATE, DCP(dcp), &device_state);

	if (rv == -1)
		*devstate = 0;
	else
		*devstate = device_state;

	return (rv);
}

int
devctl_bus_getstate(devctl_hdl_t dcp, uint_t *devstate)
{
	int  rv;
	uint_t device_state;

	rv = dc_buscmd(DEVCTL_BUS_GETSTATE, DCP(dcp), &device_state);

	if (rv == -1)
		*devstate = 0;
	else
		*devstate = device_state;

	return (rv);
}

static int
dc_childcmd(uint_t cmd, struct devctl_hdl *dcp, uint_t *devstate)
{

	struct devctl_iocdata iocdata;
	int  rv;

	if (dcp == NULL) {
		errno = EINVAL;
		return (-1);
	}

	memset(&iocdata, 0, sizeof (struct devctl_iocdata));

	dn_parse_component(dcp->components[dcp->num_components - 1],
	    &(iocdata.dev_name), &(iocdata.dev_addr), &(iocdata.dev_minor));

	dprintf(("dc_childcmd: components cn %s ca %s cm %s\n",
		(iocdata.dev_name ? iocdata.dev_name : "NULL"),
		(iocdata.dev_addr ? iocdata.dev_addr : "NULL"),
		(iocdata.dev_minor ? iocdata.dev_minor : "NULL")));

	iocdata.cmd = cmd;
	iocdata.ret_state = devstate;
	rv = ioctl(dcp->fd, cmd, &iocdata);
	return (rv);
}

static int
dc_buscmd(uint_t cmd, struct devctl_hdl *dcp, uint_t *devstate)
{

	struct devctl_iocdata iocdata;
	int  rv;

	if (dcp == NULL) {
		errno = EINVAL;
		return (-1);
	}

	memset(&iocdata, 0, sizeof (struct devctl_iocdata));

	iocdata.cmd = cmd;
	iocdata.ret_state = devstate;

	rv = ioctl(dcp->fd, cmd, &iocdata);
	return (rv);
}

/*
 * decompose a pathname into individual components
 */
static uint_t
dn_path_to_components(char *dpath, char ***path_components)
{
	char *parsep;
	char *endp;
	int  nc = 0;
	int  i = 0;
	char *dup_path;
	char **components;

	if ((dpath == NULL) || (path_components == NULL))
		return (0);

	/*
	 * calculate the number of components in the pathname
	 */
	if ((dup_path = strdup(dpath)) == NULL)
		return (0);

	parsep = (char *)strtok_r(dup_path, "/", &endp);
	while (parsep != NULL) {
		nc++;
		parsep = (char *)strtok_r(NULL, "/", &endp);
	}
	free(dup_path);

	if (nc == 0) {
		*path_components = NULL;
		return (0);
	}

	/*
	 * allocate an array of pointers for each component
	 * in the pathname
	 */
	components = (char **)calloc(nc, sizeof (char *));
	if (components == NULL) {
		*path_components = NULL;
		return (0);
	}

	parsep = (char *)strtok_r(dpath, "/", &endp);
	while (parsep != NULL) {
		components[i++] = strdup(parsep);
		parsep = (char *)strtok_r(NULL, "/", &endp);
	}

	*path_components = components;
	return (nc);
}

/*
 * reconstruct a pathname from a list of components
 */
char *
dn_components_to_path(uint_t num_components, char **components)
{
	char *devpath;
	uint_t i;

	if ((num_components == 0) || (components == NULL))
		return (NULL);

	devpath = (char *)malloc(MAXPATHLEN);
	if (devpath == NULL)
		return (NULL);

	*devpath = '\0';
	for (i = 0; i < num_components; i++) {
		if (components[i] == NULL ||
		    (strlen(devpath) + strlen(components[i]) + 2) >
		    MAXPATHLEN) {
			free(devpath);
			return (NULL);
		}
		(void) strcat(devpath, "/");
		(void) strcat(devpath, components[i]);
	}

	return (devpath);
}

/*
 * split a component of a "/devices" pathname into the driver
 * name, device address, and minor device specifier
 */
static void
dn_parse_component(char *devcomp, char **devname, char **devaddr,
    char **minor_spec)
{
	char *dname;
	char *daddr;
	char *mspec;

	if ((devcomp == NULL) || ((devname == NULL) && (devaddr == NULL) &&
	    (minor_spec == NULL)))
		return;

	dname = devcomp;
	daddr = strchr(devcomp, (int)'@');
	mspec = strchr(devcomp, (int)':');

	if (devname != NULL) {
		if (daddr != NULL)
			*daddr = '\0';
		*devname = strdup(dname);
		if (daddr != NULL)
			*daddr = '@';
	}

	if (devaddr != NULL) {
		if (daddr != NULL) {
			if (mspec != NULL)
				*mspec = '\0';
			*devaddr = strdup(daddr + 1);
			if (mspec != NULL)
				*mspec = ':';
		} else {
			*devaddr = NULL;
		}
	}

	if (minor_spec != NULL) {
		if (mspec != NULL)
			*minor_spec = strdup(mspec + 1);
		else
			*minor_spec = NULL;
	}
}

static void
free_components(int num_components, char **components)
{
	uint_t i;

	if ((num_components == 0) || (components == NULL))
		return;

	/*
	 * free the strings in the string array
	 */
	for (i = 0; i < num_components; i++)
		if (components[i] != NULL)
			free(components[i]);

	/*
	 * free the string array itself
	 */
	free(components);
}
