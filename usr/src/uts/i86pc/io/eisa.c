/*
 * Copyright (c) 1990-1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)eisa.c	1.19	96/08/30 SMI"

/*
 *	EISA bus nexus driver
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
 * #define EISA_DEBUG 1
 */

/*
 *	Local data
 */
static	ddi_dma_lim_t EISA_dma_limits = {
	0,		/* address low					*/
	(u_long) 0xffffffff, /* address high				*/
	0,		/* counter max					*/
	1,		/* burstsize 					*/
	DMA_UNIT_8,	/* minimum xfer					*/
	0,		/* dma speed					*/
	(u_int) DMALIM_VER0, /* version					*/
	(u_long) 0xffffffff, /* address register			*/
	0x00ffffff,	/* counter register				*/
	1,		/* sector size					*/
	0x00001fff,	/* scatter/gather list length - cmd chain	*/
	(u_int) 0xffffffff /* request size				*/
};

static ddi_dma_attr_t EISA_dma_attr = {
	DMA_ATTR_V0,
	(unsigned long long)0,
	(unsigned long long)0xffffffff,
	0x00ffffff,
	1,
	1,
	1,
	(unsigned long long)0xffffffff,
	(unsigned long long)0xffffffff,
	0x00001fff,
	1,
	0
};


/*
 * Config information
 */
static int EISA_chaining = -1;

static int
eisa_dma_allochdl(dev_info_t *, dev_info_t *, ddi_dma_attr_t *,
    int (*waitfp)(caddr_t), caddr_t arg, ddi_dma_handle_t *);

static int
eisa_dma_mctl(dev_info_t *, dev_info_t *, ddi_dma_handle_t, enum ddi_dma_ctlops,
    off_t *, u_int *, caddr_t *, u_int);

static int
eisa_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t, void *, void *);

struct bus_ops eisa_bus_ops = {
	BUSO_REV,
	i_ddi_bus_map,
	i_ddi_get_intrspec,
	i_ddi_add_intrspec,
	i_ddi_remove_intrspec,
	i_ddi_map_fault,
	ddi_dma_map,
	eisa_dma_allochdl,
	ddi_dma_freehdl,
	ddi_dma_bindhdl,
	ddi_dma_unbindhdl,
	ddi_dma_flush,
	ddi_dma_win,
	eisa_dma_mctl,
	eisa_ctlops,
	ddi_bus_prop_op,
	0,	/* (*bus_get_eventcookie)();	*/
	0,	/* (*bus_add_eventcall)();	*/
	0,	/* (*bus_remove_eventcall)();	*/
	0	/* (*bus_post_event)();		*/
};

static int eisa_identify(dev_info_t *devi);
static int eisa_probe(dev_info_t *);
static int eisa_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);

struct dev_ops eisa_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	ddi_no_info,		/* info */
	eisa_identify,		/* identify */
	eisa_probe,		/* probe */
	eisa_attach,		/* attach */
	nodev,			/* detach */
	nodev,			/* reset */
	(struct cb_ops *)0,	/* driver operations */
	&eisa_bus_ops	/* bus operations */

};

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This is EISA bus driver */
	"eisa nexus driver for 'EISA'",
	&eisa_ops,	/* driver ops */
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
eisa_identify(dev_info_t *devi)
{
	if (strcmp(ddi_get_name(devi), DEVI_EISA_NEXNAME) == 0) {
		return (DDI_IDENTIFIED);
	}
	return (DDI_NOT_IDENTIFIED);
}

static int
eisa_probe(register dev_info_t *devi)
{
	int len;
	char bus_type[16];

	len = sizeof (bus_type);
	if (ddi_prop_op(DDI_DEV_T_ANY, devi, PROP_LEN_AND_VAL_BUF, 0,
	    "bus-type", (caddr_t)&bus_type, &len) != DDI_PROP_SUCCESS)
		return (DDI_PROBE_FAILURE);
	if (strcmp(bus_type, DEVI_EISA_NEXNAME))
		return (DDI_PROBE_FAILURE);
	return (DDI_PROBE_SUCCESS);
}

static int
eisa_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	int ninterrupts;
	int rval;

	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);

	if ((rval = i_dmae_init(devi)) == DDI_SUCCESS)
		ddi_report_dev(devi);
	if (ddi_dev_nintrs(devi, &ninterrupts) != DDI_SUCCESS ||
	    ninterrupts != 1) {
		/*
		 * improper interrupt configuration disables
		 * DMA buffer chaining
		 */
		EISA_chaining = 0;
		EISA_dma_limits.dlim_sgllen = 1;
		cmn_err(CE_NOTE, "!eisa: DMA buffer-chaining not enabled\n");
	}
	return (rval);
}

static int
eisa_dma_allochdl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_attr_t *dma_attr,
    int (*waitfp)(caddr_t), caddr_t arg, ddi_dma_handle_t *handlep)
{
	ddi_dma_attr_merge(dma_attr, &EISA_dma_attr);
	return (ddi_dma_allochdl(dip, rdip, dma_attr, waitfp, arg, handlep));
}

static int
eisa_dma_mctl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, enum ddi_dma_ctlops request,
    off_t *offp, u_int *lenp, caddr_t *objp, u_int flags)
{
	ddi_dma_lim_t defalt;

	switch (request) {

	case DDI_DMA_E_PROG:
		if (!EISA_chaining && offp &&
		    ((struct ddi_dmae_req *)offp)->der_bufprocess ==
			DMAE_BUF_CHAIN)
			((struct ddi_dmae_req *)offp)->der_bufprocess =
			    DMAE_BUF_NOAUTO;
		return i_dmae_prog(rdip, (struct ddi_dmae_req *)offp,
		    (ddi_dma_cookie_t *)lenp, (int)objp);

	case DDI_DMA_E_ACQUIRE:
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

	case DDI_DMA_E_DISABLE:
		i_dmae_disable(rdip, (int)objp);
		return (DDI_SUCCESS);

	case DDI_DMA_E_GETCNT:
		i_dmae_get_chan_stat(rdip, (int)objp, (u_long *)0, (int *)lenp);
		return (DDI_SUCCESS);

	case DDI_DMA_E_SWSETUP:
		if (!EISA_chaining &&
		    ((struct ddi_dmae_req *)offp)->der_bufprocess ==
			DMAE_BUF_CHAIN)
			((struct ddi_dmae_req *)offp)->der_bufprocess =
			    DMAE_BUF_NOAUTO;
		return (i_dmae_swsetup(rdip, (struct ddi_dmae_req *)offp,
		    (ddi_dma_cookie_t *)lenp, (int)objp));

	case DDI_DMA_E_SWSTART:
		i_dmae_swstart(rdip, (int)objp);
		return (DDI_SUCCESS);

	case DDI_DMA_E_GETLIM:
		bcopy((caddr_t)&EISA_dma_limits, (caddr_t)objp,
		    sizeof (ddi_dma_lim_t));
		return (DDI_SUCCESS);

	case DDI_DMA_E_GETATTR:
		bcopy((caddr_t)&EISA_dma_attr, (caddr_t)objp,
		    sizeof (ddi_dma_attr_t));
		return (DDI_SUCCESS);

	case DDI_DMA_E_1STPTY:
		{
			struct ddi_dmae_req req1stpty =
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
			if ((int)objp == 0) {
				req1stpty.der_command = DMAE_CMD_TRAN;
				req1stpty.der_trans = DMAE_TRANS_DMND;
			} else
				req1stpty.der_trans = DMAE_TRANS_CSCD;
			return (i_dmae_prog(rdip, &req1stpty,
			    (ddi_dma_cookie_t *)0, (int)objp));
		}

	case DDI_DMA_IOPB_ALLOC:	/* get contiguous DMA-able memory */
	case DDI_DMA_SMEM_ALLOC:
		if (!offp) {
			defalt = EISA_dma_limits;
			offp = (off_t *)&defalt;
		}
		/*FALLTHROUGH*/
	default:
		return (ddi_dma_mctl(dip, rdip, handle, request, offp, lenp,
		    objp, flags));
	}
}

/*ARGSUSED*/
static int
eisa_ctlops(dev_info_t *dip, dev_info_t *rdip,
	ddi_ctl_enum_t ctlop, void *arg, void *result)
{
	extern int ignore_hardware_nodes;	/* force flag from ddi_impl.c */

	switch (ctlop) {
	case DDI_CTLOPS_REPORTDEV:
		if (rdip == (dev_info_t *)0)
			return (DDI_FAILURE);
		cmn_err(CE_CONT, "?EISA-device: %s%d\n",
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
