/*
 * Copyright (c) 1990, 1992, 1994 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)pci_to_isa.c	1.23	96/05/15 SMI"

/*
 *	PCI-to-ISA bus nexus driver
 */

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/ddidmareq.h>
#include <sys/ddi_impldefs.h>
#include <sys/dma_engine.h>
#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/pci.h>

/* NEEDSWORK:  should be in a header file */
typedef struct isa_regspec {
	int isa_flags;
	int isa_phys;
	int isa_size;
} isa_regspec_t;
#define	ISA_ADDR_MASK	1
#define	ISA_ADDR_MEM	0
#define	ISA_ADDR_IO	1

/*
 * #define ISA_DEBUG 1
 */
/*
 *      Local data
 */
static ddi_dma_lim_t ISA_dma_limits = {
	0,			/* address low			*/
	(u_long)0xffffffff,	/* address high			*/
	0,			/* counter max			*/
	1,			/* burstsize			*/
	DMA_UNIT_8,		/* minimum xfer			*/
	0,			/* dma speed			*/
	(u_int)DMALIM_VER0,	/* version			*/
	(u_int)0xffffffff,	/* address register		*/
	0x0000ffff,		/* counter register		*/
	1,			/* sector size			*/
	0x00000001,		/* scatter/gather list length	*/
	(u_int)0xffffffff	/* request size			*/
};

/*
 * Config information
 */
static int
isa_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
    off_t offset, off_t len, caddr_t *vaddrp);

static int
isa_dma_mctl(dev_info_t *, dev_info_t *, ddi_dma_handle_t, enum ddi_dma_ctlops,
    off_t *, u_int *, caddr_t *, u_int);

static int
isa_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t, void *, void *);

static ddi_intrspec_t pci_to_isa_get_intrspec(dev_info_t *,
    dev_info_t *, uint_t);

struct bus_ops isa_bus_ops = {
	BUSO_REV,
	isa_map,
	pci_to_isa_get_intrspec,
	i_ddi_add_intrspec,
	i_ddi_remove_intrspec,
	i_ddi_map_fault,
	ddi_dma_map,
	ddi_dma_allochdl,
	ddi_dma_freehdl,
	ddi_dma_bindhdl,
	ddi_dma_unbindhdl,
	ddi_dma_flush,
	ddi_dma_win,
	isa_dma_mctl,
	isa_ctlops,
	ddi_bus_prop_op
};

static int isa_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);

struct dev_ops isa_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	ddi_no_info,		/* info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	isa_attach,		/* attach */
	nodev,			/* detach */
	nodev,			/* reset */
	(struct cb_ops *)0,	/* driver operations */
	&isa_bus_ops	/* bus operations */

};

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This is ISA bus driver */
	"isa nexus driver for 'ISA'",
	&isa_ops,	/* driver ops */
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
isa_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	int rval;

	if (cmd != DDI_ATTACH)
		return (DDI_FAILURE);
	if ((rval = i_dmae_init(devi)) != DDI_SUCCESS)
		return (rval);
	ddi_prop_update_string(DDI_DEV_T_NONE, devi,
		"device_type", "isa");
	ddi_prop_update_string(DDI_DEV_T_NONE, devi,
		"bus-type", "isa");
	ddi_report_dev(devi);
	return (DDI_SUCCESS);
}


static int
isa_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
    off_t offset, off_t len, caddr_t *vaddrp)
{
	pci_regspec_t pci_reg;
	ddi_map_req_t mr;
	isa_regspec_t isa_reg;
	isa_regspec_t *isa_rp;
	int 	rnumber;
	int	length;
	int	rc;

	mr = *mp; /* Get private copy of request */
	mp = &mr;

	/*
	 * check for register number
	 */
	if (mp->map_type != DDI_MT_RNUMBER) {
		isa_rp = (isa_regspec_t *)(mp->map_obj.rp);
	} else {
		rnumber = mp->map_obj.rnumber;
		/*
		 * get ALL "reg" properties for dip, select the one of
		 * of interest.
		 * This routine still performs some validity checks to
		 * make sure that everything is okay.
		 */
		rc = ddi_prop_lookup_int_array(DDI_DEV_T_ANY, rdip,
			DDI_PROP_DONTPASS, "reg", (int **)&isa_rp,
			(u_int *)&length);
		if (rc != DDI_PROP_SUCCESS) {
			return (DDI_FAILURE);
		}

		/*
		 * validate the register number.
		 */
		length /= (sizeof (isa_regspec_t) / sizeof (int));
		if (rnumber >= length) {
			ddi_prop_free(isa_rp);
			return (DDI_FAILURE);
		}

		/*
		 * copy the required entry.
		 */
		isa_reg = isa_rp[rnumber];

		/*
		 * free the memory allocated by ddi_prop_lookup_int_array
		 */
		ddi_prop_free(isa_rp);

		isa_rp = &isa_reg;
		mp->map_type = DDI_MT_REGSPEC;
	}

	/*
	 * convert the isa regsec into the regspec used by the
	 * parent pci nexus driver.  All ISA addresses are non-relocatable.
	 */
	pci_reg.pci_phys_hi = (u_int)PCI_RELOCAT_B;
	switch (isa_rp->isa_flags & ISA_ADDR_MASK) {
	case ISA_ADDR_MEM:
		pci_reg.pci_phys_hi |= PCI_ADDR_MEM32;
		break;
	case ISA_ADDR_IO:
		pci_reg.pci_phys_hi |= PCI_ADDR_IO;
		break;
	}
	pci_reg.pci_phys_mid = 0;
	pci_reg.pci_phys_low = isa_rp->isa_phys;
	pci_reg.pci_size_hi = 0;
	pci_reg.pci_size_low = isa_rp->isa_size;

	mp->map_obj.rp = (struct regspec *)&pci_reg;
	return (ddi_map(dip, mp, offset, len, vaddrp));
}

static int
isa_dma_mctl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, enum ddi_dma_ctlops request,
    off_t *offp, u_int *lenp, caddr_t *objp, u_int flags)
{
	int rval;

	switch (request) {

	case DDI_DMA_E_PROG:
		return (i_dmae_prog(rdip, (struct ddi_dmae_req *)offp,
		    (ddi_dma_cookie_t *)lenp, (int)objp));

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
		return (i_dmae_swsetup(rdip, (struct ddi_dmae_req *)offp,
		    (ddi_dma_cookie_t *)lenp, (int)objp));

	case DDI_DMA_E_SWSTART:
		i_dmae_swstart(rdip, (int)objp);
		return (DDI_SUCCESS);

	case DDI_DMA_E_GETLIM:
		bcopy((caddr_t)&ISA_dma_limits, (caddr_t)objp,
		    sizeof (ddi_dma_lim_t));
		return (DDI_SUCCESS);

	case DDI_DMA_E_1STPTY:
#if 0
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
#endif
		return (DDI_FAILURE);

	case DDI_DMA_IOPB_ALLOC:	/* get contiguous DMA-able memory */
	case DDI_DMA_SMEM_ALLOC:
		{
		auto ddi_dma_lim_t defalt;
			if (!offp) {
				defalt = ISA_dma_limits;
				offp = (off_t *)&defalt;
			}
		}
		/* fall through */
	default:
		rval = ddi_dma_mctl(dip, rdip, handle, request, offp,
		    lenp, objp, flags);
	}
	return (rval);
}

/*ARGSUSED*/
static int
isa_ctlops(dev_info_t *dip, dev_info_t *rdip,
	ddi_ctl_enum_t ctlop, void *arg, void *result)
{
	isa_regspec_t *drv_regp;
	int	reglen;
	int	rn;
	int	totreg;

	switch (ctlop) {
	case DDI_CTLOPS_REPORTDEV:
		if (rdip == (dev_info_t *)0)
			return (DDI_FAILURE);
		cmn_err(CE_CONT, "?ISA-device: %s%d\n",
		    ddi_get_name(rdip), ddi_get_instance(rdip));
		return (DDI_SUCCESS);

	case DDI_CTLOPS_INITCHILD:
		return (impl_ddi_sunbus_initchild((dev_info_t *)arg));

	case DDI_CTLOPS_UNINITCHILD:
		impl_ddi_sunbus_removechild((dev_info_t *)arg);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_XLATE_INTRS:
	case DDI_CTLOPS_NINTRS:
		/* XXXPPC:  Ask our *grandparent*. */
		return (ddi_ctlops(ddi_get_parent(dip),
			rdip, ctlop, arg, result));

	case DDI_CTLOPS_REGSIZE:
	case DDI_CTLOPS_NREGS:
		if (rdip == (dev_info_t *)0)
			return (DDI_FAILURE);

		*(int *)result = 0;
		if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, rdip,
				DDI_PROP_DONTPASS, "reg", (int **)&drv_regp,
				(u_int *)&reglen) != DDI_PROP_SUCCESS) {
			return (DDI_FAILURE);
		}

		totreg = reglen / (sizeof (isa_regspec_t) / sizeof (int));
		if (ctlop == DDI_CTLOPS_NREGS)
			*(int *)result = totreg;
		else if (ctlop == DDI_CTLOPS_REGSIZE) {
			rn = *(int *)arg;
			if (rn > totreg) {
				ddi_prop_free(drv_regp);
				return (DDI_FAILURE);
			}
			*(off_t *)result = drv_regp[rn].isa_size;
		}
		ddi_prop_free(drv_regp);

		return (DDI_SUCCESS);

	default:
		return (ddi_ctlops(dip, rdip, ctlop, arg, result));
	}
}

/*
 * pci_to_isa_get_intrspec:	convert an interrupt number to an interrupt
 *			specification. The interrupt number determines which
 *			interrupt will be returned if more than one exists.
 *			returns an interrupt specification if successful and
 *			NULL if the interrupt specification could not be found.
 *			If "name" is NULL, first (and only) interrupt
 *			name is searched for.  this is the wrapper for the
 *			bus function bus_get_intrspec.
 *
 * XXXPPC:  complete slime.  We know our parent won't do the right thing.
 * However, as is always the case with parents and grandparents, our
 * grandparent will!
 */
ddi_intrspec_t
pci_to_isa_get_intrspec(dev_info_t *dip, dev_info_t *rdip, u_int inumber)
{
	dev_info_t *pdip = (dev_info_t *)DEVI(dip)->devi_parent;
	pdip = (dev_info_t *)DEVI(pdip)->devi_parent;	/* grandparent */

	/* request parent to return an interrupt specification */
	return ((*(DEVI(pdip)->devi_ops->devo_bus_ops->bus_get_intrspec))(pdip,
	    rdip, inumber));
}
