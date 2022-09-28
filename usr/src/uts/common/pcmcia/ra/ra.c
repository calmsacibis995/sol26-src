
/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident  "@(#)ra.c 1.6     96/05/17 SMI"


#include <sys/types.h>
#include <sys/byteorder.h>
#include <sys/systm.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/kmem.h>


static int ra_getinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int ra_identify(dev_info_t *);
static int ra_attach(dev_info_t *, ddi_attach_cmd_t);
static int ra_detach(dev_info_t *, ddi_detach_cmd_t);

static dev_info_t *ra_dip;


static struct dev_ops ra_devops = {
	DEVO_REV,			/* devo_rev */
	0,				/* devo_refcnt */
	ra_getinfo,			/* devo_getinfo */
	ra_identify,			/* devo_identify */
	nulldev,			/* devo_probe */
	ra_attach,			/* devo_attach */
	ra_detach,			/* devo_detach */
	nulldev,			/* devo_reset */
	NULL,				/* devo_cb_ops */
	NULL				/* devo_bus_ops */
};

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module. This one is a driver */
	"resource allocator driver",	/* Name of the module. */
	&ra_devops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

int
_init()
{
	return (mod_install(&modlinkage));
}

int
_fini()
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*ARGSUSED*/
static int
ra_identify(dev_info_t *dip)
{
	return (DDI_IDENTIFIED);
}

/*ARGSUSED*/
static int
ra_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **result)
{
	int error = DDI_SUCCESS;
	switch (cmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if (ra_dip != NULL)
			*result = ra_dip;
		else
			error = DDI_FAILURE;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

#define	RA_INTR	0x001
#define	RA_DMA	0x002
#define	RA_IO	0x004
#define	RA_MEM	0x008

static int
ra_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	dev_info_t *used, root;
	caddr_t propint, propmem, propio, propdma;
	int lenintr, lenmem, lenio, lendma, new = 0, which = 0;

	switch (cmd) {
	case DDI_ATTACH:
		break;

	default:
		return (DDI_FAILURE);
	}

	/*
	 * Attach handling
	 */
	ra_dip = dip;		/* only one instance */

	/*
	 * all this driver does is take the properties from
	 * its dip and move them to a root property that
	 * the PCMCIA nexus knows about.
	 *
	 * The properties are under a root child called /used-resources
	 * We create that node if it doesn't exist and then put the
	 * properties there.  If it exists, we replace the existing
	 * properties with new ones to allow updating at runtime.
	 */
	root = ddi_root_node();
	used = ddi_find_devinfo("used-resources", -1, 0);
	if (used == NULL) {
		/* need to create the node */
		used = ddi_add_child(root, "used-resources",
					DEVI_PSEUDO_NODEID, 0);
		if (used == NULL)
			return (DDI_FAILURE);
		new++;		/* we created it */
	} else {
		/*
		 * we don't want to mess with what devconf did
		 * so we fail the attach now and that's that.
		 */
		return (DDI_FAILURE);
	}

	/*
	 * if we get here, we have a used node to work with
	 * need to check
	 */

	lenintr = 0;
	if (ddi_getlongprop(DDI_DEV_T_NONE, dip,
			    DDI_PROP_DONTPASS|DDI_PROP_CANSLEEP,
			    "interrupts", (caddr_t)&propint,
			    &lenintr) == DDI_PROP_SUCCESS) {
		which |= RA_INTR;
	} else if (ddi_getlongprop(DDI_DEV_T_NONE, dip,
			    DDI_PROP_DONTPASS|DDI_PROP_CANSLEEP,
			    "used-interrupts", (caddr_t)&propint,
			    &lenintr) == DDI_PROP_SUCCESS) {
		which |= RA_INTR;
	}
	lendma = 0;
	if (ddi_getlongprop(DDI_DEV_T_NONE, dip,
			    DDI_PROP_DONTPASS|DDI_PROP_CANSLEEP,
			    "dma-channels", (caddr_t)&propdma,
			    &lendma) == DDI_PROP_SUCCESS) {
		which |= RA_DMA;
	}

	lenio = 0;
	if (ddi_getlongprop(DDI_DEV_T_NONE, dip,
			    DDI_PROP_DONTPASS|DDI_PROP_CANSLEEP,
			    "io-space", (caddr_t)&propio,
			    &lenio) == DDI_PROP_SUCCESS) {
		which |= RA_IO;
	}

	lenmem = 0;
	if (ddi_getlongprop(DDI_DEV_T_NONE, dip,
			    DDI_PROP_DONTPASS|DDI_PROP_CANSLEEP,
			    "device-memory", (caddr_t)&propmem,
			    &lenmem) == DDI_PROP_SUCCESS) {
		which |= RA_MEM;
	}
	if (new) {
		if (which & RA_INTR) {
			ddi_prop_create(DDI_DEV_T_NONE, used,
					DDI_PROP_CANSLEEP,
					"interrupts", propint, lenintr);
			kmem_free(propint, lenintr);
		}
		if (which & RA_DMA) {
			ddi_prop_create(DDI_DEV_T_NONE, used,
					DDI_PROP_CANSLEEP,
					"dma-channels", propdma, lendma);
			kmem_free(propdma, lendma);
		}
		if (which & RA_IO) {
			ddi_prop_create(DDI_DEV_T_NONE, used,
					DDI_PROP_CANSLEEP,
					"io-space", propio, lenio);
			kmem_free(propio, lenio);
		}
		if (which & RA_MEM) {
			ddi_prop_create(DDI_DEV_T_NONE, used,
					DDI_PROP_CANSLEEP,
					"device-memory", propmem, lenmem);
			kmem_free(propmem, lenmem);
		}
	} else {
		caddr_t newprop, oldprop;
		int oldproplen;
		/*
		 * this is an override situation
		 */
		if (which & RA_INTR) {
			oldproplen = 0;
			oldprop = NULL;
			newprop = NULL;
			if (ddi_getlongprop(DDI_DEV_T_NONE, dip,
					    DDI_PROP_DONTPASS|
					    DDI_PROP_CANSLEEP,
					    "interrupts", (caddr_t)&oldprop,
					    &oldproplen) == DDI_PROP_SUCCESS) {
				newprop = kmem_alloc(lenintr + oldproplen,
							KM_SLEEP);
				if (newprop != NULL) {
					bcopy(oldprop, newprop, oldproplen);
					bcopy(propint, newprop + oldproplen,
						lenintr);
					ddi_prop_modify(DDI_DEV_T_NONE, dip,
							DDI_PROP_CANSLEEP,
							"interrupts",
							newprop,
							oldproplen + lenintr);
				}
			} else {
				ddi_prop_create(DDI_DEV_T_NONE, dip,
						DDI_PROP_CANSLEEP,
						"interrupts",
						newprop,
						oldproplen + lenintr);
			}
			if (oldproplen)
				kmem_free(oldprop, oldproplen);
			if (newprop)
				kmem_free(newprop, oldproplen + lenintr);
			kmem_free(propint, lenintr);
		}
		if (which & RA_MEM) {
			oldproplen = 0;
			oldprop = NULL;
			newprop = NULL;
			if (ddi_getlongprop(DDI_DEV_T_NONE, dip,
					    DDI_PROP_DONTPASS|
					    DDI_PROP_CANSLEEP,
					    "device-memory", (caddr_t)&oldprop,
					    &oldproplen) == DDI_PROP_SUCCESS) {
				newprop = kmem_alloc(lenmem + oldproplen,
							KM_SLEEP);
				if (newprop != NULL) {
					bcopy(oldprop, newprop, oldproplen);
					bcopy(propmem, newprop + oldproplen,
						lenmem);
					ddi_prop_modify(DDI_DEV_T_NONE, dip,
							DDI_PROP_CANSLEEP,
							"device-memory",
							newprop,
							oldproplen + lenmem);
				}
			} else {
				ddi_prop_create(DDI_DEV_T_NONE, dip,
						DDI_PROP_CANSLEEP,
						"device-memory",
						newprop,
						oldproplen + lenmem);
			}
			if (oldproplen)
				kmem_free(oldprop, oldproplen);
			if (newprop)
				kmem_free(newprop, oldproplen + lenmem);
			kmem_free(propmem, lenmem);
		}
		if (which & RA_IO) {
			oldproplen = 0;
			oldprop = NULL;
			newprop = NULL;
			if (ddi_getlongprop(DDI_DEV_T_NONE, dip,
					    DDI_PROP_DONTPASS|
					    DDI_PROP_CANSLEEP,
					    "io-space", (caddr_t)&oldprop,
					    &oldproplen) == DDI_PROP_SUCCESS) {
				newprop = kmem_alloc(lenio + oldproplen,
							KM_SLEEP);
				if (newprop != NULL) {
					bcopy(oldprop, newprop, oldproplen);
					bcopy(propio, newprop + oldproplen,
						lenio);
					ddi_prop_modify(DDI_DEV_T_NONE, dip,
							DDI_PROP_CANSLEEP,
							"io-space",
							newprop,
							oldproplen + lenio);
				}
			} else {
				ddi_prop_create(DDI_DEV_T_NONE, dip,
						DDI_PROP_CANSLEEP,
						"io-space",
						newprop,
						oldproplen + lenio);
			}
			if (oldproplen)
				kmem_free(oldprop, oldproplen);
			if (newprop)
				kmem_free(newprop, oldproplen + lenio);
			kmem_free(propmem, lenio);
		}
		if (which & RA_DMA) {
			oldproplen = 0;
			oldprop = NULL;
			newprop = NULL;
			if (ddi_getlongprop(DDI_DEV_T_NONE, dip,
					    DDI_PROP_DONTPASS|
					    DDI_PROP_CANSLEEP,
					    "dma-channels", (caddr_t)&oldprop,
					    &oldproplen) == DDI_PROP_SUCCESS) {
				newprop = kmem_alloc(lendma + oldproplen,
							KM_SLEEP);
				if (newprop != NULL) {
					bcopy(oldprop, newprop, oldproplen);
					bcopy(propmem, newprop + oldproplen,
						lendma);
					ddi_prop_modify(DDI_DEV_T_NONE, dip,
							DDI_PROP_CANSLEEP,
							"dma-channels",
							newprop,
							oldproplen + lendma);
				}
			} else {
				ddi_prop_create(DDI_DEV_T_NONE, dip,
						DDI_PROP_CANSLEEP,
						"dma-channels",
						newprop,
						oldproplen + lendma);
			}
			if (oldproplen)
				kmem_free(oldprop, oldproplen);
			if (newprop)
				kmem_free(newprop, oldproplen + lendma);
			kmem_free(propmem, lendma);
		}
	}

	return (DDI_FAILURE);
}

/*ARGSUSED*/
static int
ra_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{

	switch (cmd) {
	case DDI_DETACH:
		break;

	default:
		return (DDI_FAILURE);
	}

	/*
	 * We don't support detaching yet
	 */
	return (DDI_FAILURE);
}
