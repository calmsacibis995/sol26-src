/*
 * Copyright (c) 1989-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)openprom.c	1.11	96/09/24 SMI"	/* SVr4 */

/*
 * Ported from 4.1.1_PSRA: "@(#)openprom.c 1.19 91/02/19 SMI";
 *
 * Porting notes:
 *
 * OPROMU2P unsupported after SunOS 4.x.
 *
 * Only one of these devices per system is allowed.
 */

/*
 * Openprom eeprom options/devinfo driver.
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/openpromio.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/modctl.h>
#include <sys/debug.h>
#include <sys/autoconf.h>
#include <sys/promif.h>
#include <sys/prom_config.h>
#include <sys/isa_defs.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#define	MAX_OPENS	32	/* Up to this many simultaneous opens */

/*
 * XXX	Make this dynamic.. or (better still) make the interface stateless
 */
static struct oprom_state {
	dnode_t	 current_id;	/* which node we're fetching properties from */
	int	already_open;	/* if true, this instance is 'active' */
} oprom_state[MAX_OPENS];

static kmutex_t oprom_lock;	/* serialize instance assignment */

static int opromopen(dev_t *, int, int, cred_t *);
static int opromioctl(dev_t, int, intptr_t, int, cred_t *, int *);
static int opromclose(dev_t, int, int, cred_t *);

static int opinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
		void **result);
static int opidentify(dev_info_t *);
static int opattach(dev_info_t *, ddi_attach_cmd_t cmd);
static int opdetach(dev_info_t *, ddi_detach_cmd_t cmd);

static struct cb_ops openeepr_cb_ops = {
	opromopen,		/* open */
	opromclose,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	opromioctl,		/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* prop_op */
	NULL,			/* streamtab  */
	D_NEW | D_MP		/* Driver compatibility flag */
};

static struct dev_ops openeepr_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	opinfo,			/* info */
	opidentify,		/* identify */
	nulldev,		/* probe */
	opattach,		/* attach */
	opdetach,		/* detach */
	nodev,			/* reset */
	&openeepr_cb_ops,	/* driver operations */
	NULL			/* bus operations */
};

/*
 * Module linkage information for the kernel.
 */
static struct modldrv modldrv = {
	&mod_driverops,
	"OPENPROM/NVRAM Driver v1.11",
	&openeepr_ops
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	NULL
};

int
_init(void)
{
	int	error;

	mutex_init(&oprom_lock, "openeepr driver lock", MUTEX_DRIVER, NULL);

	error = mod_install(&modlinkage);
	if (error != 0) {
		mutex_destroy(&oprom_lock);
		return (error);
	}

	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int
_fini(void)
{
	int	error;

	error = mod_remove(&modlinkage);
	if (error != 0)
		return (error);

	mutex_destroy(&oprom_lock);
	return (0);
}

static dev_info_t *opdip;
static dnode_t options_nodeid;

/*ARGSUSED*/
static int
opinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	register int error = DDI_FAILURE;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = (void *)opdip;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		/* All dev_t's map to the same, single instance */
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		break;
	}

	return (error);
}

static int
opidentify(dev_info_t *dip)
{
	if (strcmp(ddi_get_name(dip), "openeepr") == 0)
		return (DDI_IDENTIFIED);
	return (DDI_NOT_IDENTIFIED);
}

static int
opattach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	switch (cmd) {

	case DDI_ATTACH:
		options_nodeid = NULL;
		if (prom_is_openprom()) {
			dev_info_t *options_dip;

			/*
			 * Find 'options' node in the tree, then get the
			 * underlying nodeid as understood by the OBP.
			 */
			if ((options_dip = ddi_find_devinfo("options", -1, 0))
			    == NULL) {
				/*
				 * No options node?
				 */
				cmn_err(CE_WARN, "%s: no options node",
				    ddi_get_name(dip));
			} else
				options_nodeid =
				    (dnode_t)ddi_get_nodeid(options_dip);
		}

		opdip = dip;

		if (ddi_create_minor_node(dip, "openprom", S_IFCHR,
		    0, NULL, NULL) == DDI_FAILURE) {
			return (DDI_FAILURE);
		}

		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

static int
opdetach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	if (cmd != DDI_DETACH)
		return (DDI_FAILURE);

	ddi_remove_minor_node(dip, NULL);
	opdip = NULL;

	return (DDI_SUCCESS);
}

/*
 * Allow multiple opens by tweaking the dev_t such that it looks like each
 * open is getting a different minor device.  Each minor gets a separate
 * entry in the oprom_state[] table.
 */
/*ARGSUSED*/
static int
opromopen(dev_t *devp, int flag, int otyp, cred_t *credp)
{
	register int m;
	register struct oprom_state *st = oprom_state;

	if (getminor(*devp) != 0)
		return (ENXIO);

	mutex_enter(&oprom_lock);
	for (m = 0; m < MAX_OPENS; m++)
		if (st->already_open)
			st++;
		else {
			st->already_open = 1;
			/*
			 * It's ours.
			 */
			st->current_id = 0;
			break;
		}
	mutex_exit(&oprom_lock);

	if (m == MAX_OPENS)  {
		/*
		 * "Thank you for calling, but all our lines are
		 * busy at the moment.."
		 *
		 * We could get sophisticated here, and go into a
		 * sleep-retry loop .. but hey, I just can't see
		 * that many processes sitting in this driver.
		 *
		 * (And if it does become possible, then we should
		 * change the interface so that the 'state' is held
		 * external to the driver)
		 */
		return (EAGAIN);
	}

	*devp = makedevice(getmajor(*devp), (minor_t)m);

	return (0);
}

/*ARGSUSED*/
static int
opromclose(dev_t dev, int flag, int otype, cred_t *cred_p)
{
	register struct oprom_state *st;

	st = &oprom_state[getminor(dev)];
	ASSERT(getminor(dev) < MAX_OPENS && st->already_open != 0);
	st->already_open = 0;

	return (0);
}

/*ARGSUSED*/
static int
opromioctl(dev_t dev, int cmd, intptr_t arg,
	int mode, cred_t *credp, int *rvalp)
{
	struct oprom_state *st;
	register struct openpromio *opp;
	register int valsize;
	register char *valbuf;
	register int error = 0;
	u_int userbufsize;
	dnode_t node_id;
	char nextprop[32];
	prom_config_handle_t pch;

	if (getminor(dev) >= MAX_OPENS)
		return (ENXIO);

	st = &oprom_state[getminor(dev)];
	ASSERT(st->already_open);

	switch (cmd) {
	case OPROMGETOPT:
	case OPROMNXTOPT:
		if ((mode & FREAD) == 0) {
			return (EPERM);
		}
		node_id = options_nodeid;
		break;

	case OPROMSETOPT:
	case OPROMSETOPT2:
		if ((mode & FWRITE) == 0) {
			return (EPERM);
		}
		node_id = options_nodeid;
		break;

	case OPROMNEXT:
	case OPROMCHILD:
	case OPROMGETPROP:
	case OPROMNXTPROP:
		if ((mode & FREAD) == 0) {
			return (EPERM);
		}
		node_id = st->current_id;
		break;

	case OPROMGETCONS:
	case OPROMGETFBNAME:
	case OPROMGETBOOTARGS:
	case OPROMGETVERSION:
	case OPROMPATH2DRV:
	case OPROMDEV2PROMNAME:
	case OPROMPROM2DEVNAME:
		if ((mode & FREAD) == 0) {
			return (EPERM);
		}
		break;

	default:
		return (EINVAL);
	}

	if (copyin((void *)arg, (void *)&userbufsize, sizeof (u_int)) != 0)
		return (EFAULT);

	if (userbufsize == 0 || userbufsize > OPROMMAXPARAM)
		return (EINVAL);

	opp = (struct openpromio *)kmem_zalloc(
	    userbufsize + sizeof (u_int) + 1, KM_SLEEP);

	if (copyin(((caddr_t)arg + sizeof (u_int)),
	    opp->oprom_array, (size_t)userbufsize) != 0) {
		error = EFAULT;
		goto done;
	}

	switch (cmd) {
	case OPROMGETOPT:
	case OPROMGETPROP:
		if ((prom_is_openprom() == 0) ||
		    (node_id == OBP_NONODE) || (node_id == OBP_BADNODE) ||
		    prom_config_begin(&pch, node_id) != DDI_SUCCESS) {
			error = EINVAL;
			goto done;
		}

		valsize = prom_config_getproplen(pch, opp->oprom_array);
		if (valsize > 0 && valsize <= userbufsize) {
			valbuf = kmem_zalloc((size_t)valsize, KM_SLEEP);
			(void) prom_config_getprop(pch, opp->oprom_array,
			    valbuf);
			opp->oprom_size = valsize;
			bzero(opp->oprom_array, userbufsize);
			bcopy(valbuf, (caddr_t)opp->oprom_array,
			    (size_t)valsize);
			kmem_free(valbuf, (size_t)valsize);
		}
		prom_config_end(pch);
		if (copyout(opp, (void *)arg,
		    (size_t)(userbufsize+sizeof (u_int))) != 0)
			error = EFAULT;
		break;

	case OPROMSETOPT:
	case OPROMSETOPT2:
		if ((prom_is_openprom() == 0) ||
		    (node_id == OBP_NONODE) || (node_id == OBP_BADNODE) ||
		    prom_config_begin(&pch, node_id) != DDI_SUCCESS) {
			error = EINVAL;
			goto done;
		}

		/*
		 * The property name is the first string, value second
		 */
		if (strlen(opp->oprom_array) > OBP_MAXPROPNAME) {
			error = EINVAL;
			prom_config_end(pch);
			goto done;
		}
		valbuf = opp->oprom_array + strlen(opp->oprom_array) + 1;
		if (cmd == OPROMSETOPT) {
			valsize = strlen(valbuf) + 1;  /* +1 for the '\0' */
		} else
			valsize = (opp->oprom_array + userbufsize) - valbuf;

		if ((prom_setprop(node_id, opp->oprom_array,
		    valbuf, valsize) < 0) || (prom_config_setprop(pch,
			opp->oprom_array, valbuf, valsize) < 0))
			error = EINVAL;

		prom_config_end(pch);
		break;

	case OPROMNXTOPT:
	case OPROMNXTPROP:
		if ((prom_is_openprom() == 0) ||
		    (node_id == OBP_NONODE) || (node_id == OBP_BADNODE) ||
		    prom_config_begin(&pch, node_id) != DDI_SUCCESS) {
			error = EINVAL;
			goto done;
		}

		valbuf = (char *)prom_config_nextprop(pch, opp->oprom_array,
		    nextprop);
		valsize = strlen(valbuf);

		if (valsize == 0) {
			opp->oprom_size = 0;
		} else if (++valsize <= userbufsize) {
			opp->oprom_size = valsize;
			bzero((caddr_t)opp->oprom_array, (size_t)userbufsize);
			bcopy((caddr_t)valbuf, (caddr_t)opp->oprom_array,
			    (size_t)valsize);
		}

		prom_config_end(pch);
		if (copyout(opp, (void *)arg,
		    userbufsize + sizeof (u_int)) != 0)
			error = EFAULT;
		break;

	case OPROMNEXT:
	case OPROMCHILD:
		if (prom_is_openprom() == 0 ||
		    userbufsize < sizeof (dnode_t *)) {
			error = EINVAL;
			goto done;
		}

		node_id = *(dnode_t *)opp->oprom_array;

		if (prom_config_begin(&pch, node_id) != DDI_SUCCESS) {
			error = EINVAL;
			goto done;
		}


		/*
		 * If the caller is asking for OPROMNEXT of nodeid 0,
		 * that really means 'can I have the nodeid of the root node'.
		 */
		if (node_id == 0 && cmd == OPROMNEXT)
			st->current_id = prom_config_topnode(pch);
		else if (cmd == OPROMNEXT)
			st->current_id = prom_config_nextnode(pch);
		else
			st->current_id = prom_config_childnode(pch);

		opp->oprom_size = sizeof (dnode_t);
		bzero(opp->oprom_array, userbufsize);
		*(dnode_t *)opp->oprom_array = st->current_id;

		prom_config_end(pch);
		if (copyout(opp, (void *)arg,
		    userbufsize + sizeof (u_int)) != 0)
			error = EFAULT;
		break;

	case OPROMGETCONS:
		/*
		 * What type of console are we using?
		 * Is openboot supported on this machine?
		 */
		opp->oprom_size = sizeof (char);
		opp->oprom_array[0] = prom_config_stdin_is_keyboard() ?
		    OPROMCONS_STDIN_IS_KBD : OPROMCONS_NOT_WSCONS;

		opp->oprom_array[0] |= prom_config_stdout_is_framebuffer() ?
		    OPROMCONS_STDOUT_IS_FB : OPROMCONS_NOT_WSCONS;

		opp->oprom_array[0] |= prom_is_openprom() ?
		    OPROMCONS_OPENPROM : 0;

		if (copyout(opp, (void *)arg, sizeof (char) +
		    sizeof (u_int)) != 0)
			error = EFAULT;
		break;

	case OPROMGETFBNAME:
		/*
		 * Return stdoutpath, if it's a frame buffer.
		 * Yes, we are comparing a possibly longer string against
		 * the size we're really going to copy, but so what?
		 */
		if ((prom_config_stdout_is_framebuffer() != 0) &&
		    (userbufsize > strlen(prom_config_stdoutpath()))) {
			prom_strip_options(prom_config_stdoutpath(),
			    opp->oprom_array);	/* strip options and copy */
			opp->oprom_size = strlen(opp->oprom_array);
			error = copyout(opp, (void *)arg,
			    userbufsize + sizeof (u_int));
			if (error != 0)
				error = EFAULT;
		} else
			error = EINVAL;
		break;

	case OPROMGETBOOTARGS: {
		extern char kern_bootargs[];

		(void) strcpy(opp->oprom_array, kern_bootargs);
		opp->oprom_size = strlen(opp->oprom_array);

		if (copyout(opp, (void *)arg,
		    opp->oprom_size + 1 + sizeof (u_int)) != 0)
			error = EFAULT;
	}	break;

	case OPROMPROM2DEVNAME: {
		char *dev_name;

		dev_name = kmem_alloc(MAXPATHLEN, KM_SLEEP);

		/*
		 * convert prom path to the equivalent devfs path
		 */
		error = i_promname_to_devname(opp->oprom_array, dev_name);
		if (error != 0) {
			kmem_free(dev_name, MAXPATHLEN);
			break;
		}
		(void) strcpy(opp->oprom_array, dev_name);
		opp->oprom_size = strlen(dev_name);
		error = copyout(opp, (void *)arg, sizeof (u_int) +
		    opp->oprom_size + 1);
		if (error != 0)
			error = EFAULT;
		kmem_free(dev_name, MAXPATHLEN);
	}	break;

	/*
	 * There is no sane way to do this (presently) on PPC.
	 * The problem is that some of the PROM nodes are hidden
	 * from the kernel.  If a user wishes to convert /pci/isa@0/cmdk@a,b
	 * to its prom equivalent of /pci/isa@0/disk@a,b, the kernel
	 * needs to be able to "see" the 'disk' prom node. The promif "hides
"
	 * all nodes below the isa nexus from prom device tree readers.
	 */
	case OPROMDEV2PROMNAME:
		error = ENXIO;
		break;
	/*
	 * Convert a prom device path name to a driver binding name
	 */
	case OPROMPATH2DRV: {
		char *drv_name;
		major_t maj;

		/* convert a path to a driver binding name */
		if ((drv_name = i_path_to_drv(opp->oprom_array))
		    == 0) {
			error = EINVAL;
			break;
		}
		/* resolve any aliases */
		if (((maj = ddi_name_to_major(drv_name)) == -1) ||
		    ((drv_name = ddi_major_to_name(maj)) == NULL)) {
			error = EINVAL;
			break;
		}
		(void) strcpy(opp->oprom_array, drv_name);
		opp->oprom_size = strlen(drv_name);
		error = copyout(opp, (void *)arg, sizeof (u_int) +
		    opp->oprom_size + 1);
		if (error != 0)
			error = EFAULT;
	}	break;

	case OPROMGETVERSION:
		/*
		 * Get a string representing the running version of the
		 * prom. How to create such a string is platform dependent,
		 * so we just defer to a promif function. If no such
		 * association exists, the promif implementation
		 * may copy the string "unknown" into the given buffer,
		 * and return its length (incl. NULL terminator).
		 *
		 * We expect prom_version_name to return the actual
		 * length of the string, but copy at most userbufsize
		 * bytes into the given buffer, including NULL termination.
		 */

		valsize = prom_config_version_name(opp->oprom_array,
		    userbufsize);
		if (valsize < 0) {
			error = EINVAL;
			break;
		}

		/*
		 * copyout only the part of the user buffer we need to.
		 */
		if (copyout(opp, (void *)arg,
		    (size_t)(min((u_int)valsize, userbufsize) +
		    sizeof (u_int))) != 0)
			error = EFAULT;
		break;
	}
done:
	kmem_free(opp, userbufsize + sizeof (u_int) + 1);
	return (error);
}
