/*
 * Copyright (c) 1990-1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mc.c	1.14	96/08/30 SMI"

/*
 *	MCA bus nexus driver
 */

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/ddidmareq.h>
#include <sys/ddi_impldefs.h>
#include <sys/dma_engine.h>
#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>


/*
 *      Local data
 */
static  ddi_dma_lim_t MCA_dma_limits = {
	0,		/* address low			*/
	0x00ffffff,	/* address high			*/
	0,		/* counter max			*/
	1,		/* burstsize			*/
	DMA_UNIT_8,	/* minimum xfer			*/
	0,		/* dma speed			*/
	(u_int) DMALIM_VER0, /* version			*/
	0x0000ffff,	/* address register		*/
	0x0000ffff,	/* counter register		*/
	1,		/* sector size			*/
	0x00000001,	/* scatter/gather list length	*/
	(u_long) 0xffffffff /* request size		*/
};

static ddi_dma_attr_t MCA_dma_attr = {
	DMA_ATTR_V0,
	(unsigned long long)0,
	(unsigned long long)0x00ffffff,
	0x0000ffff,
	1,
	1,
	1,
	(unsigned long long)0xffffffff,
	(unsigned long long)0x0000ffff,
	1,
	1,
	0
};


/*
 * Config information
 */

static int
mc_dma_allochdl(dev_info_t *, dev_info_t *, ddi_dma_attr_t *,
    int (*waitfp)(caddr_t), caddr_t arg, ddi_dma_handle_t *);

static int
mc_dma_mctl(dev_info_t *, dev_info_t *, ddi_dma_handle_t, enum ddi_dma_ctlops,
    off_t *, u_int *, caddr_t *, u_int);

static int
mc_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t, void *, void *);

struct bus_ops mc_bus_ops = {
	BUSO_REV,
	i_ddi_bus_map,
	i_ddi_get_intrspec,
	i_ddi_add_intrspec,
	i_ddi_remove_intrspec,
	i_ddi_map_fault,
	ddi_dma_map,
	mc_dma_allochdl,
	ddi_dma_freehdl,
	ddi_dma_bindhdl,
	ddi_dma_unbindhdl,
	ddi_dma_flush,
	ddi_dma_win,
	mc_dma_mctl,
	mc_ctlops,
	ddi_bus_prop_op,
	0,	/* (*bus_get_eventcookie)();	*/
	0,	/* (*bus_add_eventcall)();	*/
	0,	/* (*bus_remove_eventcall)();	*/
	0	/* (*bus_post_event)();		*/
};

static int mc_identify(dev_info_t *devi);
static int mc_probe(dev_info_t *);
static int mc_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);

struct dev_ops mc_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	ddi_no_info,		/* info */
	mc_identify,		/* identify */
	mc_probe,		/* probe */
	mc_attach,		/* attach */
	nodev,		/* detach */
	nodev,			/* reset */
	(struct cb_ops *)0,	/* driver operations */
	&mc_bus_ops	/* bus operations */

};

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This is MCA bus driver */
	"mc nexus driver for 'MCA'",
	&mc_ops,	/* driver ops */
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
mc_identify(dev_info_t *devi)
{
#ifdef MCA_DEBUG
	printf("mc: mc_identify()\n");
#endif
	if (strcmp(ddi_get_name(devi), DEVI_MCA_NEXNAME) == 0) {
		return (DDI_IDENTIFIED);
	}
	return (DDI_NOT_IDENTIFIED);
}

static int
mc_probe(register dev_info_t *devi)
{
	int len;
	char bus_type[16];
	extern int (*clock_reenable)();
	int reset_mc_timer();
#ifdef MCA_DEBUG
	printf(" mc_probe()\n");
#endif

	len = sizeof (bus_type);
	if (ddi_prop_op(DDI_DEV_T_ANY, devi, PROP_LEN_AND_VAL_BUF, 0,
	    "bus-type", (caddr_t)&bus_type, &len) != DDI_PROP_SUCCESS)
		return (DDI_PROBE_FAILURE);
	if (strcmp(bus_type, DEVI_MCA_NEXNAME)) {
		return (DDI_PROBE_FAILURE);
	}
	clock_reenable = reset_mc_timer;
	return (DDI_PROBE_SUCCESS);
}

static int
mc_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	int rval;
#ifdef MCA_DEBUG
	printf("mc_attach()\n");
#endif
	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);

	if ((rval = i_dmae_init(devi)) == DDI_SUCCESS)
		ddi_report_dev(devi);
	return (rval);
}

static int
mc_dma_allochdl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_attr_t *dma_attr,
    int (*waitfp)(caddr_t), caddr_t arg, ddi_dma_handle_t *handlep)
{
	ddi_dma_attr_merge(dma_attr, &MCA_dma_attr);
	return (ddi_dma_allochdl(dip, rdip, dma_attr, waitfp, arg, handlep));
}

static int
mc_dma_mctl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, enum ddi_dma_ctlops request,
    off_t *offp, u_int *lenp, caddr_t *objp, u_int flags)
{
	int rval;
	ddi_dma_lim_t defalt;

	switch (request) {

	case DDI_DMA_E_PROG:
		return (i_dmae_prog(rdip, (struct ddi_dmae_req *)offp,
		    (ddi_dma_cookie_t *)lenp, (int)objp));

	case DDI_DMA_E_ACQUIRE:
		if ((int)objp >= NCHANS)
			return (DDI_SUCCESS);
		return (i_dmae_acquire(rdip, (int)objp, (int(*)())offp,
		    (caddr_t)lenp));

	case DDI_DMA_E_FREE:
		return (i_dmae_free(rdip, (int)objp));

	case DDI_DMA_E_STOP:
		i_dmae_stop(rdip, (int)objp);
		return (DDI_SUCCESS);

	case DDI_DMA_E_ENABLE:
		i_dmae_enable(rdip, (int)objp);
		return (DDI_SUCCESS);

	case DDI_DMA_E_1STPTY:
		/*
		 * Adaptors using first-party DMA on the MicroChannel
		 * must be real bus masters.  If the arbitration level
		 * is shared with a DMA channel, then the DMA channel
		 * is disabled.
		 */
		if ((int)objp >= NCHANS)
			return (DDI_SUCCESS);
		/*FALLTHROUGH*/
	case DDI_DMA_E_DISABLE:
		i_dmae_disable(rdip, (int)objp);
		return (DDI_SUCCESS);

	case DDI_DMA_E_GETCNT:
		i_dmae_get_chan_stat(rdip, (int)objp, (u_long *)0, (int *)lenp);
		return (DDI_SUCCESS);

	case DDI_DMA_E_SWSETUP:
		return (i_dmae_swsetup(rdip, (struct ddi_dmae_req *)offp,
		    (ddi_dma_cookie_t *)lenp, (int)objp));

	case DDI_DMA_E_SWSTART:
		i_dmae_swstart(rdip, (int)objp);
		return (DDI_SUCCESS);

	case DDI_DMA_E_GETLIM:
		bcopy((caddr_t)&MCA_dma_limits, (caddr_t)objp,
		    sizeof (ddi_dma_lim_t));
		return (DDI_SUCCESS);

	case DDI_DMA_E_GETATTR:
		bcopy((caddr_t)&MCA_dma_attr, (caddr_t)objp,
		    sizeof (ddi_dma_attr_t));
		return (DDI_SUCCESS);

	case DDI_DMA_IOPB_ALLOC:	/* get contiguous DMA-able memory */
	case DDI_DMA_SMEM_ALLOC:
		if (!offp) {
			defalt = MCA_dma_limits;
			offp = (off_t *)&defalt;
		}
		/*FALLTHROUGH*/
	default:
		rval = ddi_dma_mctl(dip, rdip, handle, request, offp,
		    lenp, objp, flags);
	}
	return (rval);
}

/*ARGSUSED*/
static int
mc_ctlops(dev_info_t *dip, dev_info_t *rdip,
	ddi_ctl_enum_t ctlop, void *arg, void *result)
{
	extern int ignore_hardware_nodes;	/* force flag from ddi_impl.c */

	switch (ctlop) {
	case DDI_CTLOPS_REPORTDEV:
		if (rdip == (dev_info_t *)0)
			return (DDI_FAILURE);
		cmn_err(CE_CONT, "?MCA-device: %s%d\n",
		    ddi_get_name(rdip), ddi_get_instance(rdip));
		return (DDI_SUCCESS);

	case DDI_CTLOPS_INITCHILD:
		/*
		 * older drivers aren't expecting the "standard" device
		 * node format used by the hardware nodes.  these drivers
		 * only expect their own properties set in their driver.conf
		 * files.  so they tell us not to call them with hardware
		 * nodes by setting the property "ignore-hardware-nodes".
		 */
		if ((ddi_get_nodeid((dev_info_t *)arg) != DEVI_PSEUDO_NODEID) &&
		    ((ddi_getprop(DDI_DEV_T_ANY, (dev_info_t *)arg,
		    DDI_PROP_DONTPASS, "ignore-hardware-nodes", -1) != -1) ||
		    ignore_hardware_nodes)) {
			return (DDI_NOT_WELL_FORMED);
		}

		return (impl_ddi_sunbus_initchild((dev_info_t *)arg));

	case DDI_CTLOPS_UNINITCHILD:
		impl_ddi_sunbus_removechild((dev_info_t *)arg);
		return (DDI_SUCCESS);

	default:
		return (ddi_ctlops(dip, rdip, ctlop, arg, result));
	}
}

#define	MCA_SYS_CTRL_PORT_B	0x61	/* MCA watchdog timer port */
#define	MCA_TIMER_RESET_BIT	0x80

int
reset_mc_timer()
{
	register unsigned char curbyte;

	curbyte = inb(MCA_SYS_CTRL_PORT_B);
	(void) outb(MCA_SYS_CTRL_PORT_B, (curbyte | MCA_TIMER_RESET_BIT));
	return (0);
}
