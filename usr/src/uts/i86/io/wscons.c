/*
 * Copyright (c) 1987-1990 by Sun Microsystems, Inc.
 */

#pragma	ident "@(#)wscons.c	1.4	96/05/07 SMI"

/*
 * "Workstation console" multiplexor driver for Sun.
 *
 * All the guts have been removed for the i86 version because the
 * console architecture is different.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/termios.h>
#include <sys/termio.h>
#include <sys/ttold.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/tty.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/vmmac.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/stat.h>

#include <sys/pte.h>
#include <sys/mmu.h>
#include <sys/cpu.h>

#include <sys/kbio.h>

#include <sys/strredir.h>

#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

extern vnode_t *rconsvp;
extern vnode_t *rwsconsvp;

static int wc_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
		void **result);
static int wc_identify(dev_info_t *devi);
static int wc_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);
static dev_info_t *wc_dip;

struct cb_ops	wc_cb_ops = {

	nodev,			/* open */
	nodev,			/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	nodev,			/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev, 			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* cb_prop_op */
	0,			/* streamtab  */
	D_NEW,			/* Driver compatibility flag */

};

struct dev_ops	wc_ops = {

	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	wc_info,		/* info */
	wc_identify,		/* identify */
	nulldev,		/* probe */
	wc_attach,		/* attach */
	nodev,			/* detach */
	nodev,			/* reset */
	&wc_cb_ops,		/* driver operations */
	(struct bus_ops *)0	/* bus operations */
};

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/modctl.h>


extern nodev(), nulldev();
extern dseekneg_flag;
extern struct mod_ops mod_driverops;
extern struct dev_ops wc_ops;

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This one is a pseudo driver */
	"Workstation multiplexer Driver 'wc'",
	&wc_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	NULL
};


int
_init(void)
{
	return (mod_install(&modlinkage));
}


int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
wc_identify(dev_info_t *devi)
{
	if (strcmp(ddi_get_name(devi), "wc") == 0)
		return (DDI_IDENTIFIED);
	else
		return (DDI_NOT_IDENTIFIED);
}

/*ARGSUSED*/
static int
wc_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	if (ddi_create_minor_node(devi, "wscons", S_IFCHR,
	    0, NULL, NULL) == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (-1);
	}
	wc_dip = devi;
	return (DDI_SUCCESS);
}

/* ARGSUSED */
static int
wc_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
	void **result)
{
	register int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if (wc_dip == NULL) {
			error = DDI_FAILURE;
		} else {
			*result = (void *) wc_dip;
			error = DDI_SUCCESS;
		}
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

/*
 * Auxiliary routines, for allowing the workstation console to be redirected.
 */

/*
 * Given a minor device number for a wscons instance, return a held vnode for
 * it.
 *
 * We currently support only one instance, for the "workstation console".
 */
int
wcvnget(unit, vpp)
	int	unit;
	vnode_t	**vpp;
{
	if (unit != 0)
		return (ENXIO);

	/*
	 * rwsconsvp is already held, so we don't have to do it here.
	 */
	*vpp = rwsconsvp;
	return (0);
}

/*
 * Release the vnode that wcvnget returned.
 */
/*ARGSUSED*/
void
wcvnrele(unit, vp)
	int	unit;
	vnode_t	*vp;
{
	/*
	 * Nothing to do, since we only support the workstation console
	 * instance that's held throughout the system's lifetime.
	 */
}

/*
 * The declaration and initialization of the wscons_srvnops has been
 * moved to space.c to allow "wc" to become a loadable module.
 */
