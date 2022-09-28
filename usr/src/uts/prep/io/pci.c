/*
 * Copyright (c) 1994-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pci.c	1.36	96/09/24 SMI"

/*
 *	Host to PCI local bus driver
 */

#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/modctl.h>
#include <sys/autoconf.h>
#include <sys/ddi_impldefs.h>
#include <sys/pci.h>
#include <sys/pci_impl.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

/*
 * This variable controls the default setting of the command register
 * for PCI devices.  See pci_initchild() for details.
 */

static u_short pci_command_default = PCI_COMM_SERR_ENABLE |
					PCI_COMM_WAIT_CYC_ENAB |
					PCI_COMM_PARITY_DETECT |
					PCI_COMM_ME |
					PCI_COMM_MAE |
					PCI_COMM_IO;

static int pci_bus_map(dev_info_t *, dev_info_t *, ddi_map_req_t *,
	off_t, off_t, caddr_t *);
static int pci_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t,
	void *, void *);
static ddi_intrspec_t pci_get_intrspec(dev_info_t *, dev_info_t *, uint_t);
static int pci_dma_bindhdl(dev_info_t *dip, dev_info_t *rdip,
	ddi_dma_handle_t handle, struct ddi_dma_req *dmareq,
	ddi_dma_cookie_t *cp, u_int *ccountp);
static int pci_dma_mctl(dev_info_t *dip, dev_info_t *rdip,
	ddi_dma_handle_t handle, enum ddi_dma_ctlops request,
	off_t *offp, u_int *lenp, caddr_t *objp, u_int flags);

static struct bus_ops pci_bus_ops = {
	BUSO_REV,
	pci_bus_map,
	pci_get_intrspec,
	i_ddi_add_intrspec,
	i_ddi_remove_intrspec,
	i_ddi_map_fault,
	ddi_dma_map,
	ddi_dma_allochdl,
	ddi_dma_freehdl,
	pci_dma_bindhdl,
	ddi_dma_unbindhdl,
	ddi_dma_flush,
	ddi_dma_win,
	pci_dma_mctl,
	pci_ctlops,
	ddi_bus_prop_op
};

static int pci_identify(dev_info_t *devi);
static int pci_probe(dev_info_t *);
static int pci_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);
static int pci_detach(dev_info_t *devi, ddi_detach_cmd_t cmd);
static int pci_info(dev_info_t *dip, ddi_info_cmd_t infocmd,
	void *arg, void **result);

static struct dev_ops pci_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* refcnt  */
	pci_info,		/* info */
	pci_identify,		/* identify */
	pci_probe,		/* probe */
	pci_attach,		/* attach */
	pci_detach,		/* detach */
	nulldev,		/* reset */
	(struct cb_ops *)0,	/* driver operations */
	&pci_bus_ops		/* bus operations */

};

static int get_assigned_addr(dev_info_t *dip, dev_info_t *rdip,
	pci_regspec_t *pci_rp, u_int *base);
static void pci_removechild(dev_info_t *child);
static int pci_initchild(dev_info_t *child);
static int pci_create_pci_prop(dev_info_t *child, uint_t *foundslot,
	uint_t *foundfunc);
static int pci_get_devid(dev_info_t *child, uint_t *bus, uint_t *devnum,
	uint_t *funcnum);
static int pci_devclass_to_ipl(int class);
static int get_reg_set(dev_info_t *dip, dev_info_t *rdip, int rnumber,
	pci_regspec_t *rp);
static int pci_map_config(dev_info_t *dip, dev_info_t *rdip,
	ddi_map_req_t *mp, pci_regspec_t *pci_rp, off_t offset, off_t len,
	caddr_t *vaddrp);
static int xlate_reg_prop(dev_info_t *dip, dev_info_t *rdip,
	pci_regspec_t *pci_rp, off_t off, off_t len, struct regspec *rp);

static uint8_t pci_config_rd8(ddi_acc_impl_t *hdlp, uint8_t *addr);
static uint16_t pci_config_rd16(ddi_acc_impl_t *hdlp, uint16_t *addr);
static uint32_t pci_config_rd32(ddi_acc_impl_t *hdlp, uint32_t *addr);
static uint64_t pci_config_rd64(ddi_acc_impl_t *hdlp, uint64_t *addr);

static void pci_config_wr8(ddi_acc_impl_t *hdlp, uint8_t *addr,
				uint8_t value);
static void pci_config_wr16(ddi_acc_impl_t *hdlp, uint16_t *addr,
				uint16_t value);
static void pci_config_wr32(ddi_acc_impl_t *hdlp, uint32_t *addr,
				uint32_t value);
static void pci_config_wr64(ddi_acc_impl_t *hdlp, uint64_t *addr,
				uint64_t value);

static void pci_config_rep_rd8(ddi_acc_impl_t *hdlp, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags);
static void pci_config_rep_rd16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags);
static void pci_config_rep_rd32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags);
static void pci_config_rep_rd64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags);

static void pci_config_rep_wr8(ddi_acc_impl_t *hdlp, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags);
static void pci_config_rep_wr16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags);
static void pci_config_rep_wr32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags);
static void pci_config_rep_wr64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags);

static uchar_t i_pci_getb(int bus, int device, int function, int reg);
static ushort_t i_pci_getw(int bus, int device, int function, int reg);
static ulong_t i_pci_getl(int bus, int device, int function, int reg);
static void i_pci_putb(int bus, int device, int function, int reg,
	uchar_t val);
static void i_pci_putw(int bus, int device, int function, int reg,
	ushort val);
static void i_pci_putl(int bus, int device, int function, int reg,
	ulong_t val);

static  int	pci_check();

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module */
	"host to PCI nexus driver",
	&pci_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
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

/*ARGSUSED*/
static int
pci_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	return (DDI_FAILURE);
}

/*ARGSUSED*/
static int
pci_identify(dev_info_t *dip)
{
	return (DDI_IDENTIFIED);
}

/*ARGSUSED*/
static int
pci_probe(register dev_info_t *devi)
{
	return (DDI_PROBE_SUCCESS);
}

/*ARGSUSED*/
static int
pci_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	int rc;

	rc = pci_check();
	ASSERT(rc == DDI_SUCCESS);

	ddi_prop_create(DDI_DEV_T_NONE, devi, DDI_PROP_CANSLEEP,
		"device_type", (caddr_t)"pci", 4);
	ddi_report_dev(devi);
	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
pci_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	return (DDI_SUCCESS);
}

static int
pci_bus_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
	off_t offset, off_t len, caddr_t *vaddrp)
{
	struct regspec regspec;
	ddi_map_req_t mr;
	pci_regspec_t pci_reg;
	pci_regspec_t *pci_rp;
	int rval;

	/*
	 * Get a pointer to an appropriate reg spec
	 */
	switch (mp->map_type) {
	case DDI_MT_REGSPEC:
		pci_rp = (pci_regspec_t *)(mp->map_obj.rp);
		break;
	case DDI_MT_RNUMBER:
		rval = get_reg_set(dip, rdip,  mp->map_obj.rnumber, &pci_reg);
		if (rval != DDI_SUCCESS)
			return (rval);
		pci_rp = &pci_reg;
		break;
	}

	/*
	 * Config space doesn't translate directly into root memory
	 * space, and requires special handling.
	 */
	if ((pci_rp->pci_phys_hi & PCI_REG_ADDR_M) == PCI_ADDR_CONFIG) {
		return (pci_map_config(dip, rdip, mp, pci_rp,
			    offset, len, vaddrp));
	}

	/*
	 * range check
	 */
	if ((offset >= pci_rp->pci_size_low) ||
	    (len > pci_rp->pci_size_low) ||
	    (offset + len > pci_rp->pci_size_low)) {
		return (DDI_FAILURE);
	}

	rval = xlate_reg_prop(dip, rdip, pci_rp, offset, len, &regspec);
	if (rval != DDI_SUCCESS)
		return (rval);

	mr = *mp;
	mr.map_obj.rp = &regspec;
	mr.map_type = DDI_MT_REGSPEC;
	return (ddi_map(dip, &mr, (off_t)0, (off_t)0, vaddrp));
}

static int
pci_map_config(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
	pci_regspec_t *pci_rp, off_t offset, off_t len, caddr_t *vaddrp)
{
	ddi_acc_hdl_t *hp;
	ddi_acc_impl_t *ap;
	pci_acc_cfblk_t *cfp;

#if defined(lint)
	dip = dip;
	rdip = rdip;
#endif
	/*
	 * User mappings aren't allowed for config space
	 */
	if (mp->map_op == DDI_MO_MAP_HANDLE)
		return (DDI_FAILURE);

	/*
	 * Don't need to do anything for unmap
	 */
	if ((mp->map_op == DDI_MO_UNMAP) || (mp->map_op == DDI_MO_UNLOCK))
		return (DDI_SUCCESS);

	hp = (ddi_acc_hdl_t *)mp->map_handlep;

	/* Can't map config space without a handle */
	if (hp == NULL)
		return (DDI_FAILURE);

	/*
	 * It is unreasonable to attempt to talk to config space
	 * in big-endian, because config space is defined to be
	 * little-endian.
	 */
	if (hp->ah_acc.devacc_attr_endian_flags == DDI_STRUCTURE_BE_ACC)
		return (DDI_FAILURE);

	/*
	 * range check
	 */
	if ((offset >= 256) || (len > 256) || (offset + len > 256))
		return (DDI_FAILURE);

	*vaddrp = (caddr_t)offset;

	ap = (ddi_acc_impl_t *)hp->ah_platform_private;

	ap->ahi_put8 = pci_config_wr8;
	ap->ahi_get8 = pci_config_rd8;
	ap->ahi_put64 = pci_config_wr64;
	ap->ahi_get64 = pci_config_rd64;
	ap->ahi_rep_put8 = pci_config_rep_wr8;
	ap->ahi_rep_get8 = pci_config_rep_rd8;
	ap->ahi_rep_put64 = pci_config_rep_wr64;
	ap->ahi_rep_get64 = pci_config_rep_rd64;
	ap->ahi_get16 = pci_config_rd16;
	ap->ahi_get32 = pci_config_rd32;
	ap->ahi_put16 = pci_config_wr16;
	ap->ahi_put32 = pci_config_wr32;
	ap->ahi_rep_get16 = pci_config_rep_rd16;
	ap->ahi_rep_get32 = pci_config_rep_rd32;
	ap->ahi_rep_put16 = pci_config_rep_wr16;
	ap->ahi_rep_put32 = pci_config_rep_wr32;

	/* record the device address for future reference */
	cfp = (pci_acc_cfblk_t *)&hp->ah_bus_private;
	cfp->c_busnum = PCI_REG_BUS_G(pci_rp->pci_phys_hi);
	cfp->c_devnum = PCI_REG_DEV_G(pci_rp->pci_phys_hi);
	cfp->c_funcnum = PCI_REG_FUNC_G(pci_rp->pci_phys_hi);

	return (DDI_SUCCESS);
}

/*
 * pci_get_intrspec: construct the interrupt specification from
 *		interrupt line and class-code configuration registers.
 */
/*ARGSUSED*/
ddi_intrspec_t
pci_get_intrspec(dev_info_t *dip, dev_info_t *rdip, uint_t inumber)
{
	struct ddi_parent_private_data *pdptr;
	struct intrspec *ispec;
	uint_t	bus;
	uint_t	devnum;
	uint_t	funcnum;
	int 	class;
	int	rc;

	pdptr = (struct ddi_parent_private_data *)ddi_get_parent_data(rdip);
	if (!pdptr)
		return (NULL);

	ispec = pdptr->par_intr;
	ASSERT(ispec);

	/* check if the intrspec has been initialized */
	if (ispec->intrspec_pri != 0)
		return (ispec);

	rc = pci_get_devid(rdip, &bus, &devnum, &funcnum);
	if (rc != DDI_SUCCESS)
		return (NULL);

	/* get the 'class' property to derive the intr priority */
	class = ddi_prop_get_int(DDI_DEV_T_ANY, rdip,
		DDI_PROP_DONTPASS, "class-code", -1);
	if (class == -1)
		ispec->intrspec_pri = 1;
	else
		ispec->intrspec_pri = pci_devclass_to_ipl(class);

	/* get interrupt line value */
	ispec->intrspec_vec = i_pci_getb(bus, devnum, funcnum, PCI_CONF_ILINE);
	return (ispec);
}

/*
 * translate from device class to ipl
 */
static int
pci_devclass_to_ipl(int class)
{
	int	base_cl;
	int	sub_cl;

	base_cl = (class & 0xff0000) >> 16;
	sub_cl = (class & 0xff00) >> 8;

	/* XXX need to become configurable by consulting the .conf */

	if (base_cl == PCI_CLASS_MASS) {
		if (sub_cl == PCI_MASS_FD)
			return (5);
		return (5);
	}
	if (base_cl == PCI_CLASS_NET)
		return (5);
	if (base_cl == PCI_CLASS_MM)
		return (6);
	return (1);
}

static int
pci_dma_bindhdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, struct ddi_dma_req *dmareq,
    ddi_dma_cookie_t *cp, u_int *ccountp)
{
	int rval;

	/*
	 * It might be appropriate to adjust the DMA limits structure
	 * here, but I don't think so.
	 */
	rval = ddi_dma_bindhdl(dip, rdip, handle, dmareq, cp, ccountp);

	switch (rval) {
	case DDI_DMA_MAPPED:
	case DDI_DMA_PARTIAL_MAP:
		/*
		 * Translate from CPU-relative physical address to
		 * device-relative address.
		 *
		 * Translate address field in the DMA cookie(s) to the
		 * corresponding device-relative address. Here we depend
		 * on the implementation of ddi_dma_nextcookie which assumes
		 * how the cookies are stored. The current implementation
		 * is that the dmai_cookie pointer in ddi_dma_impl_t
		 * structure points to the second cookie in the list of
		 * cookies for this object.
		 *
		 * (Jordan) I'm not sure how ISA DMA masters should work.
		 */
		/* translate the first cookie */
		cp->dmac_address += PCI_DMA_BASE;

		if (*ccountp > 1) { /* translate other cookies now */
			register ddi_dma_cookie_t *np;
			register int i = *ccountp - 1;

			np = ((ddi_dma_impl_t *)handle)->dmai_cookie;
			while (i--) {
			    np->dmac_address += PCI_DMA_BASE;
			    np++;
			}
		}
		break;
	}
	return (rval);
}

static int
pci_dma_mctl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_handle_t handle,
    enum ddi_dma_ctlops request, off_t *offp, u_int *lenp,
    caddr_t *objp, u_int flags)
{
	register ddi_dma_cookie_t *cp;
	int rval;

	rval = ddi_dma_mctl(dip, rdip, handle, request, offp,
	    lenp, objp, flags);

	if (rval == 0) {
		switch (request) {
		case DDI_DMA_MOVWIN:
		case DDI_DMA_SEGTOC:
		case DDI_DMA_HTOC:
			/*
			 * Translate from CPU-relative physical address to
			 * device-relative address.
			 */
			cp = (ddi_dma_cookie_t *)objp;
			if (cp) {
				cp->dmac_address += PCI_DMA_BASE;
			}
			break;
		}
	}
	return (rval);
}

/*ARGSUSED*/
static int
pci_ctlops(dev_info_t *dip, dev_info_t *rdip,
	ddi_ctl_enum_t ctlop, void *arg, void *result)
{
	pci_regspec_t *drv_regp;
	int	reglen;
	int	rn;
	int	totreg;

	switch (ctlop) {
	case DDI_CTLOPS_REPORTDEV:
		if (rdip == (dev_info_t *)0)
			return (DDI_FAILURE);
		cmn_err(CE_CONT, "?PCI-device: %s%d\n",
		    ddi_get_name(rdip), ddi_get_instance(rdip));
		return (DDI_SUCCESS);

	case DDI_CTLOPS_INITCHILD:
		return (pci_initchild((dev_info_t *)arg));

	case DDI_CTLOPS_UNINITCHILD:
		pci_removechild((dev_info_t *)arg);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_NINTRS:
		if (ddi_get_parent_data(rdip))
			*(int *)result = 1;
		else
			*(int *)result = 0;
		return (DDI_SUCCESS);

	case DDI_CTLOPS_XLATE_INTRS:
		return (DDI_SUCCESS);

	case DDI_CTLOPS_SIDDEV:
		return (DDI_SUCCESS);

	case DDI_CTLOPS_REGSIZE:
	case DDI_CTLOPS_NREGS:
		if (rdip == (dev_info_t *)0)
			return (DDI_FAILURE);
		break;

	default:
		return (ddi_ctlops(dip, rdip, ctlop, arg, result));
	}

	*(int *)result = 0;
	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, rdip,
			DDI_PROP_DONTPASS, "reg", (int **)&drv_regp,
			(u_int *)&reglen) != DDI_PROP_SUCCESS) {
		return (DDI_FAILURE);
	}

	totreg = reglen / (sizeof (pci_regspec_t) / sizeof (int));
	if (ctlop == DDI_CTLOPS_NREGS)
		*(int *)result = totreg;
	else if (ctlop == DDI_CTLOPS_REGSIZE) {
		rn = *(int *)arg;
		if (rn > totreg) {
			ddi_prop_free(drv_regp);
			return (DDI_FAILURE);
		}
		*(off_t *)result = drv_regp[rn].pci_size_low;
	}
	ddi_prop_free(drv_regp);

	return (DDI_SUCCESS);
}

static int
pci_create_pci_prop(dev_info_t *child, uint_t *foundslot, uint_t *foundfunc)
{
	pci_regspec_t *pci_rp;
	int	length;
	int	value;

	/* get child "reg" property */
	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, child,
			DDI_PROP_DONTPASS, "reg", (int **)&pci_rp,
			(u_int *)&length) != DDI_PROP_SUCCESS) {
		return (DDI_NOT_WELL_FORMED);
	}

	if (ddi_prop_update_int_array(DDI_DEV_T_NONE, child, "reg",
			(int *)pci_rp, (u_int)length) != DDI_PROP_SUCCESS) {
		cmn_err(CE_NOTE, "pci: cannot create 'reg'");
	}

	/* copy the device identifications */
	*foundslot = PCI_REG_DEV_G(pci_rp->pci_phys_hi);
	*foundfunc = PCI_REG_FUNC_G(pci_rp->pci_phys_hi);

	/*
	 * free the memory allocated by ddi_prop_lookup_int_array ().
	 */
	ddi_prop_free(pci_rp);

	/* assign the basic PCI Properties */

	value = ddi_prop_get_int(DDI_DEV_T_ANY, child, 0, "vendor-id", -1);
	if (value != -1) {
		if (ddi_prop_update_int(DDI_DEV_T_NONE, child, "vendor-id",
				value) != DDI_PROP_SUCCESS) {
			cmn_err(CE_NOTE, "pci: cannot create 'vendor-id'");
		}
	}

	value = ddi_prop_get_int(DDI_DEV_T_ANY, child, 0, "device-id", -1);
	if (value != -1) {
		if (ddi_prop_update_int(DDI_DEV_T_NONE, child, "device-id",
				value) != DDI_PROP_SUCCESS) {
			cmn_err(CE_NOTE, "pci: cannot create 'device-id'");
		}
	}

	value = ddi_prop_get_int(DDI_DEV_T_ANY, child, 0, "interrupts", -1);
	if (value != -1) {
		if (ddi_prop_update_int(DDI_DEV_T_NONE, child, "interrupts",
				value) != DDI_PROP_SUCCESS) {
			cmn_err(CE_NOTE, "pci: cannot create 'interrupts'");
		}

	}

	return (DDI_SUCCESS);
}

static int
pci_initchild(dev_info_t *child)
{
	struct ddi_parent_private_data *pdptr;
	char name[80];
	int ret;
	uint_t slot, func;
	unsigned short command;
	unsigned short command_preserve;
	ddi_acc_handle_t cfg_handle;

	if ((ret = pci_create_pci_prop(child, &slot, &func)) != DDI_SUCCESS)
		return (ret);
	if (func != 0)
		sprintf(name, "%x,%x", slot, func);
	else
		sprintf(name, "%x", slot);
	ddi_set_name_addr(child, name);

	if (ddi_prop_get_int(DDI_DEV_T_ANY, child, DDI_PROP_DONTPASS,
			"interrupts", -1) != -1) {
		pdptr = (struct ddi_parent_private_data *)
			kmem_zalloc((sizeof (struct ddi_parent_private_data) +
			sizeof (struct intrspec)), KM_SLEEP);
		pdptr->par_intr = (struct intrspec *)(pdptr + 1);
		pdptr->par_nintr = 1;
		ddi_set_parent_data(child, (caddr_t)pdptr);
	} else
		ddi_set_parent_data(child, NULL);

	/*
	 * Support for the "command-preserve" property.  Note that we
	 * add PCI_COMM_BACK2BACK_ENAB to the bits to be preserved
	 * since the firmware will set this if the device supports
	 * it and all targets on the same bus support it.
	 */
	command_preserve = ddi_prop_get_int(DDI_DEV_T_ANY, child,
			DDI_PROP_DONTPASS, "command-preserve", 0);

	if ((ret = pci_config_setup(child, &cfg_handle)) != DDI_SUCCESS)
		return (ret);

	command = pci_config_getw(cfg_handle, PCI_CONF_COMM);
	command &= (command_preserve | PCI_COMM_BACK2BACK_ENAB);
	command |= (pci_command_default & ~command_preserve);
	pci_config_putw(cfg_handle, PCI_CONF_COMM, command);

	pci_config_teardown(&cfg_handle);

	return (DDI_SUCCESS);
}

static void
pci_removechild(dev_info_t *dip)
{
	register struct ddi_parent_private_data *pdptr;

	pdptr = (struct ddi_parent_private_data *)ddi_get_parent_data(dip);
	if (pdptr != NULL) {
		kmem_free(pdptr, (sizeof (*pdptr) + sizeof (struct intrspec)));
		ddi_set_parent_data(dip, NULL);
	}
	ddi_set_name_addr(dip, NULL);

	/*
	 * Strip the node to properly convert it back to prototype form
	 */
	ddi_remove_minor_node(dip, NULL);

	impl_rem_dev_props(dip);
}

static int
pci_get_devid(dev_info_t *child, uint_t *bus, uint_t *devnum, uint_t *funcnum)
{
	pci_regspec_t *pci_rp;
	int	length;
	int	rc;

	/* get child "reg" property */
	rc = ddi_prop_lookup_int_array(DDI_DEV_T_ANY, child,
		DDI_PROP_DONTPASS, "reg", (int **)&pci_rp,
		(u_int *)&length);
	if ((rc != DDI_SUCCESS) || (length <
			(sizeof (pci_regspec_t) / sizeof (int)))) {
		return (DDI_FAILURE);
	}

	/* copy the device identifications */
	*bus = PCI_REG_BUS_G(pci_rp->pci_phys_hi);
	*devnum = PCI_REG_DEV_G(pci_rp->pci_phys_hi);
	*funcnum = PCI_REG_FUNC_G(pci_rp->pci_phys_hi);

	/*
	 * free the memory allocated by ddi_prop_lookup_int_array().
	 */
	ddi_prop_free(pci_rp);
	return (DDI_SUCCESS);
}

/*
 * get_reg_set
 *
 * The routine will get an IEEE 1275 PCI format regspec for a given
 * device node and register number.
 *
 * used by: pci_map()
 *
 * return value:
 *
 *	DDI_SUCCESS		- on success
 *	DDI_ME_RNUMBER_RANGE	- rnumber out of range
 */
static int
get_reg_set(dev_info_t *dip, dev_info_t *rdip, int rnumber, pci_regspec_t *rp)
{
	pci_regspec_t *pci_rp;
	int i, rval;

#if defined(lint)
	dip = dip;
#endif
	/*
	 * Get the reg property for the device.
	 */
	rval = ddi_getlongprop(DDI_DEV_T_ANY, rdip, DDI_PROP_DONTPASS, "reg",
				(caddr_t)&pci_rp, &i);
	if (rval != DDI_SUCCESS)
		return (rval);

	if (rnumber < 0 || rnumber >= i / sizeof (pci_regspec_t)) {
		rval = DDI_ME_RNUMBER_RANGE;
	} else {
		*rp = pci_rp[rnumber];
		rval = DDI_SUCCESS;
	}
	kmem_free((caddr_t)pci_rp, i);
	return (rval);
}

/*
 * xlate_reg_prop
 *
 * This routine converts an IEEE 1275 PCI format regspec and caller-supplied
 * offset and length to a standard regspec containing the corresponding
 * system address.
 *
 * used by: pci_map()
 *
 * return value:
 *
 *	DDI_SUCCESS		- on success
 *	DDI_ME_INVAL		- regspec is invalid
 */
static int
xlate_reg_prop(dev_info_t *dip, dev_info_t *rdip, pci_regspec_t *pci_rp,
	off_t off, off_t len, struct regspec *rp)
{
	u_int phys_hi, phys_low, size_low;
	unsigned int assigned_base;
	int rval;

	phys_hi = pci_rp->pci_phys_hi;

	/*
	 * Regardless of type code, phys_mid must always be zero
	 * because we don't (yet) support 64-bit addresses.
	 */
	if (pci_rp->pci_phys_mid != 0 || pci_rp->pci_size_hi != 0)
		return (DDI_ME_INVAL);

	/*
	 * If the "reg" property specifies relocatable, get and interpret the
	 * "assigned-addresses" property.
	 */
	phys_low = pci_rp->pci_phys_low;
	if ((phys_hi & PCI_RELOCAT_B) == 0) {
		rval = get_assigned_addr(dip, rdip, pci_rp, &assigned_base);
		if (rval != DDI_SUCCESS)
			return (rval);
		phys_low += assigned_base;
	}

	/*
	 * Adjust the mapping request for the length and offset parameters.
	 */
	phys_low += off;
	size_low = (len == 0 ? pci_rp->pci_size_low : len);

	/*
	 * Build the regspec based on the address type code.
	 */
	rp->regspec_bustype = 0;
	rp->regspec_size = size_low;

	switch (phys_hi & PCI_ADDR_MASK) {

	case PCI_ADDR_MEM64:
		return (DDI_FAILURE);

	case PCI_ADDR_MEM32:

		/*
		 * Build the regspec in our parent's format.
		 */
		rp->regspec_addr = phys_low + PCIMEMORYBASE;

		break;

	case PCI_ADDR_IO:

		/*
		 * Build the regspec in our parent's format.
		 */
		rp->regspec_addr = phys_low + PCIIOBASE;
		break;

	}
	return (DDI_SUCCESS);
}


/*
 * get_assigned_addr
 *
 * This routine interprets the "assigned-addresses" property for a given
 * IEEE 1275 PCI format regspec, returning the base address associated
 * with the given regspec.
 *
 * used by: xlate_reg_prop()
 *
 * return value:
 *
 *	1	- on success
 *	0	- on failure
 */
static int
get_assigned_addr(dev_info_t *dip, dev_info_t *rdip, pci_regspec_t *pci_rp,
	u_int *base)
{
	pci_regspec_t *assigned_addr;
	int num_addr_entries;
	u_int desired_conf_addr;
	boolean_t match;
	int i;

	/*
	 * Attempt to get the "assigned-addresses" property for the
	 * requesting device.
	 */
	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, rdip,
			DDI_PROP_DONTPASS, "assigned-addresses",
			(int **)&assigned_addr,
			(u_int *)&num_addr_entries) != DDI_PROP_SUCCESS) {
#define	BACKWARDS_COMPATIBILITY_HACK
#if	defined(BACKWARDS_COMPATIBILITY_HACK)
		/*
		 * Since early (through at least 09/14/95) VOFs do
		 * not provide an assigned-addresses, and supply
		 * instead real addresses in "reg", if there's no
		 * assigned-addresses then we'll just return 0 and
		 * everything will work out OK.
		 */
		*base = 0;
		return (DDI_SUCCESS);
#else
		cmn_err(CE_WARN, "%s%d: no assigned-addresses property\n",
			ddi_get_name(rdip), ddi_get_instance(rdip));
		return (DDI_FAILURE);
#endif
	}
	num_addr_entries /= (sizeof (pci_regspec_t) / sizeof (int));

	/*
	 * Scan the "assigned-addresses" for one that matches the specified
	 * "reg" property entry.
	 */
	desired_conf_addr = pci_rp->pci_phys_hi & PCI_CONF_ADDR_MASK;
	match = B_FALSE;
	for (i = 0; i < num_addr_entries; i++) {
		if ((assigned_addr[i].pci_phys_hi & PCI_CONF_ADDR_MASK) ==
				desired_conf_addr) {
			*base = assigned_addr[i].pci_phys_low;
			match = B_TRUE;
			break;
		}
	}

	ddi_prop_free((void *)assigned_addr);

	if (!match) {
		cmn_err(CE_WARN,
		"%s #%d: no assigned-addresses for %s #%d (%x,%x,%x,%x,%x)\n",
			ddi_get_name(dip), ddi_get_instance(dip),
			ddi_get_name(rdip), ddi_get_instance(rdip),
			pci_rp->pci_phys_hi, pci_rp->pci_phys_mid,
			pci_rp->pci_phys_low, pci_rp->pci_size_hi,
			pci_rp->pci_size_low);
		return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}

/*
 * These are the get and put functions to be shared with drivers. They
 * acquire the mutex  enable the configuration space and then call the
 * internal function to do the appropriate access mechanism.
 */

static uint8_t
pci_config_rd8(ddi_acc_impl_t *hdlp, uint8_t *addr)
{
	pci_acc_cfblk_t *cfp;
	uint8_t	rval;

	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;
	rval = i_pci_getb(cfp->c_busnum, cfp->c_devnum,
			    cfp->c_funcnum, (int)addr);
	return (rval);
}

static void
pci_config_rep_rd8(ddi_acc_impl_t *hdlp, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags)
{
	pci_acc_cfblk_t *cfp;
	uint8_t *h, *d;

	h = host_addr;
	d = dev_addr;
	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			*h++ = i_pci_getb(cfp->c_busnum, cfp->c_devnum,
						cfp->c_funcnum, (int)d++);
	else
		for (; repcount; repcount--)
			*h++ = i_pci_getb(cfp->c_busnum, cfp->c_devnum,
						cfp->c_funcnum, (int)d);
}

static uint16_t
pci_config_rd16(ddi_acc_impl_t *hdlp, uint16_t *addr)
{
	pci_acc_cfblk_t *cfp;
	uint16_t rval;

	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;
	rval = i_pci_getw(cfp->c_busnum, cfp->c_devnum,
			    cfp->c_funcnum, (int)addr);
	return (rval);
}

static void
pci_config_rep_rd16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
	pci_acc_cfblk_t *cfp;
	uint16_t *h, *d;

	h = host_addr;
	d = dev_addr;
	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			*h++ = i_pci_getw(cfp->c_busnum, cfp->c_devnum,
						cfp->c_funcnum, (int)d++);
	else
		for (; repcount; repcount--)
			*h++ = i_pci_getw(cfp->c_busnum, cfp->c_devnum,
						cfp->c_funcnum, (int)d);
}

static uint32_t
pci_config_rd32(ddi_acc_impl_t *hdlp, uint32_t *addr)
{
	pci_acc_cfblk_t *cfp;
	uint32_t rval;

	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;
	rval = i_pci_getl(cfp->c_busnum, cfp->c_devnum,
			    cfp->c_funcnum, (int)addr);
	return (rval);
}

static void
pci_config_rep_rd32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	pci_acc_cfblk_t *cfp;
	uint32_t *h, *d;

	h = host_addr;
	d = dev_addr;
	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			*h++ = i_pci_getl(cfp->c_busnum, cfp->c_devnum,
						cfp->c_funcnum, (int)d++);
	else
		for (; repcount; repcount--)
			*h++ = i_pci_getl(cfp->c_busnum, cfp->c_devnum,
						cfp->c_funcnum, (int)d);
}

static void
pci_config_wr8(ddi_acc_impl_t *hdlp, uint8_t *addr, uint8_t value)
{
	pci_acc_cfblk_t *cfp;

	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;
	i_pci_putb(cfp->c_busnum, cfp->c_devnum,
		    cfp->c_funcnum, (int)addr, value);
}

static void
pci_config_rep_wr8(ddi_acc_impl_t *hdlp, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags)
{
	pci_acc_cfblk_t *cfp;
	uint8_t *h, *d;

	h = host_addr;
	d = dev_addr;
	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			i_pci_putb(cfp->c_busnum, cfp->c_devnum,
				cfp->c_funcnum, (int)d++, *h++);
	else
		for (; repcount; repcount--)
			i_pci_putb(cfp->c_busnum, cfp->c_devnum,
				cfp->c_funcnum, (int)d, *h++);
}

static void
pci_config_wr16(ddi_acc_impl_t *hdlp, uint16_t *addr, uint16_t value)
{
	pci_acc_cfblk_t *cfp;

	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;
	i_pci_putw(cfp->c_busnum, cfp->c_devnum,
		    cfp->c_funcnum, (int)addr, value);
}

static void
pci_config_rep_wr16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
	pci_acc_cfblk_t *cfp;
	uint16_t *h, *d;

	h = host_addr;
	d = dev_addr;
	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			i_pci_putw(cfp->c_busnum, cfp->c_devnum,
					cfp->c_funcnum, (int)d++, *h++);
	else
		for (; repcount; repcount--)
			i_pci_putw(cfp->c_busnum, cfp->c_devnum,
					cfp->c_funcnum, (int)d, *h++);
}

static void
pci_config_wr32(ddi_acc_impl_t *hdlp, uint32_t *addr, uint32_t value)
{
	pci_acc_cfblk_t *cfp;

	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;
	i_pci_putl(cfp->c_busnum, cfp->c_devnum,
		    cfp->c_funcnum, (int)addr, value);
}

static void
pci_config_rep_wr32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	pci_acc_cfblk_t *cfp;
	uint32_t *h, *d;

	h = host_addr;
	d = dev_addr;
	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			i_pci_putl(cfp->c_busnum, cfp->c_devnum,
					cfp->c_funcnum, (int)d++, *h++);
	else
		for (; repcount; repcount--)
			i_pci_putl(cfp->c_busnum, cfp->c_devnum,
					cfp->c_funcnum, (int)d, *h++);
}

static uint64_t
pci_config_rd64(ddi_acc_impl_t *hdlp, uint64_t *addr)
{
	uint32_t lw_val;
	uint32_t hi_val;
	uint32_t *dp;
	uint64_t val;

	dp = (uint32_t *)addr;
	lw_val = pci_config_rd32(hdlp, dp);
	dp++;
	hi_val = pci_config_rd32(hdlp, dp);
	val = ((uint64_t)hi_val << 32) | lw_val;
	return (val);
}

static void
pci_config_wr64(ddi_acc_impl_t *hdlp, uint64_t *addr, uint64_t value)
{
	uint32_t lw_val;
	uint32_t hi_val;
	uint32_t *dp;

	dp = (uint32_t *)addr;
	lw_val = (uint32_t) (value & 0xffffffff);
	hi_val = (uint32_t) (value >> 32);
	pci_config_wr32(hdlp, dp, lw_val);
	dp++;
	pci_config_wr32(hdlp, dp, hi_val);
}

static void
pci_config_rep_rd64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	if (flags == DDI_DEV_AUTOINCR) {
		for (; repcount; repcount--)
			*host_addr++ = pci_config_rd64(hdlp, dev_addr++);
	} else {
		for (; repcount; repcount--)
			*host_addr++ = pci_config_rd64(hdlp, dev_addr);
	}
}

static void
pci_config_rep_wr64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	if (flags == DDI_DEV_AUTOINCR) {
		for (; repcount; repcount--)
			pci_config_wr64(hdlp, host_addr++, *dev_addr++);
	} else {
		for (; repcount; repcount--)
			pci_config_wr64(hdlp, host_addr++, *dev_addr);
	}
}

#define	PCI_PROBE_DIRECT		1

/*
 * Internal structures and functions
 */
static 	int pci_cfg_type;
static	kmutex_t pci_mutex;

static  int	pci_identify_cfg_type();
static	int	pci_probe_direct(void);

/*
 * These two variables can be used to force a configuration mechanism or
 * to force which function is used to probe for the presence of the PCI bus.
 */
int	PCI_CFG_TYPE = 0;
int	PCI_PROBE_TYPE = 0;

#if	defined(PCI_MECHANISM_SANDALFOOT)
static unsigned long pci_sandalfoot_conf_offsets[] = {
	0x00000800, /* onboard SIO */
	0x00001000, /* onboard SCSI */
	0x00002000, /* lower PCI slot */
	0x00004000, /* upper PCI slot */
	0x00008000, /* reserved line 15 */
	0x00010000, /* reserved line 16 */
	0x00020000, /* reserved line 17 */
	0x00040000, /* reserved line 18 */
};
#endif

#if	defined(PCI_MECHANISM_2)
/*
 * the PCI LOCAL BUS SPECIFICATION 2.0 does not say that you need to
 * save the value of the register and restore them.  The intel chip
 * set documentation indicates that you should.
 */
static unchar
pci_config_enable(unchar bus, unchar function)
{
	unchar	old;

	old = inb(PCI_CSE_PORT);
	outb(PCI_CSE_PORT, old | 0x10 | ((function & PCI_FUNC_MASK) << 1));
	outb(PCI_FORW_PORT, bus);
	return (old);
}

static void
pci_config_restore(unchar oldstatus)
{
	outb(PCI_CSE_PORT, oldstatus);
}
#endif	/* PCI_MECHANISM_2 */

#if	defined(PCI_MECHANISM_SANDALFOOT)
#define	NELEM(a)	(sizeof (a) / sizeof ((a)[0]))
#define	PCI_SANDALFOOT_MAX_DEVS	NELEM(pci_sandalfoot_conf_offsets)

static void *
pci_sandalfoot_addr(int bus, int device, int function, int reg)
{
	static caddr_t sandalfoot_pci = (caddr_t)PCI_CONFIG_VBASE;

	if (bus != 0 || device < 0 || device >= PCI_SANDALFOOT_MAX_DEVS)
		return (NULL);

	return ((void *)((char *)sandalfoot_pci +
		pci_sandalfoot_conf_offsets[device] +
		function*PCI_CONF_HDR_SIZE +
		reg));
}
#endif

static uchar_t
i_pci_getb(int bus, int device, int function, int reg)
{
	uchar_t	val;

	mutex_enter(&pci_mutex);
	switch (pci_cfg_type) {
#if	defined(PCI_MECHANISM_1)
	case PCI_MECHANISM_1:
		outl(PCI_CONFADD, PCI_CADDR1(bus, device, function, reg));
		val = inb(PCI_CONFDATA | (reg & 0x3));
		break;
#endif
#if	defined(PCI_MECHANISM_2)
	case PCI_MECHANISM_2: {
		unchar	tmp;

		tmp = pci_config_enable(bus, function);
		val = inb(PCI_CADDR2(device, reg));
		pci_config_restore(tmp);
		break;
	}
#endif
#if	defined(PCI_MECHANISM_SANDALFOOT)
	case PCI_MECHANISM_SANDALFOOT: {
		void *p;

		p = pci_sandalfoot_addr(bus, device, function, reg);
		if (p != NULL) val = *(unsigned char *)p;
		else val = 0xff;
		break;
		}
#endif
	}
	mutex_exit(&pci_mutex);
	return (val);
}

static ushort_t
i_pci_getw(int bus, int device, int function, int reg)
{
	ushort_t val;

	mutex_enter(&pci_mutex);
	switch (pci_cfg_type) {
#if	defined(PCI_MECHANISM_1)
	case PCI_MECHANISM_1:
		outl(PCI_CONFADD, PCI_CADDR1(bus, device, function, reg));
		val = inw(PCI_CONFDATA | (reg & 0x2));
		break;
#endif
#if	defined(PCI_MECHANISM_2)
	case PCI_MECHANISM_2: {
		unchar	tmp;

		tmp = pci_config_enable(bus, function);
		val = inw(PCI_CADDR2(device, reg));
		pci_config_restore(tmp);
		break;
	}
#endif
#if	defined(PCI_MECHANISM_SANDALFOOT)
	case PCI_MECHANISM_SANDALFOOT: {
		void *p;

		p = pci_sandalfoot_addr(bus, device, function, reg);
		if (p != NULL) val = *(unsigned short *)p;
		else val = 0xffff;
		break;
		}
#endif
	}
	mutex_exit(&pci_mutex);
	return (val);
}

static ulong_t
i_pci_getl(int bus, int device, int function, int reg)
{
	ulong_t	val;

	mutex_enter(&pci_mutex);
	switch (pci_cfg_type) {
#if	defined(PCI_MECHANISM_1)
	case PCI_MECHANISM_1:
		outl(PCI_CONFADD, PCI_CADDR1(bus, device, function, reg));
		val = inl(PCI_CONFDATA);
		break;
#endif
#if	defined(PCI_MECHANISM_2)
	case PCI_MECHANISM_2: {
		unchar	tmp;
		tmp = pci_config_enable(bus, function);
		val = inl(PCI_CADDR2(device, reg));
		pci_config_restore(tmp);
		break;
	}
#endif
#if	defined(PCI_MECHANISM_SANDALFOOT)
	case PCI_MECHANISM_SANDALFOOT: {
		void *p;

		p = pci_sandalfoot_addr(bus, device, function, reg);
		if (p != NULL)
			val = *(unsigned long *)p;
		else
			val = 0xffffffffU;
		break;
		}
#endif
	}
	mutex_exit(&pci_mutex);
	return (val);
}

static void
i_pci_putb(int bus, int device, int function, int reg, unchar val)
{
	mutex_enter(&pci_mutex);
	switch (pci_cfg_type) {
#if	defined(PCI_MECHANISM_1)
	case PCI_MECHANISM_1:
		outl(PCI_CONFADD, PCI_CADDR1(bus, device, function, reg));
		outb(PCI_CONFDATA | (reg & 0x3), val);
		break;
#endif
#if	defined(PCI_MECHANISM_2)
	case PCI_MECHANISM_2: {
		unchar	tmp;

		tmp = pci_config_enable(bus, function);
		outb(PCI_CADDR2(device, reg), val);
		pci_config_restore(tmp);
		break;
	}
#endif
#if	defined(PCI_MECHANISM_SANDALFOOT)
	case PCI_MECHANISM_SANDALFOOT: {
		void *p;

		p = pci_sandalfoot_addr(bus, device, function, reg);
		if (p != NULL) *(unsigned char *)p = val;
		break;
		}
#endif
	}
	mutex_exit(&pci_mutex);
}

static void
i_pci_putw(int bus, int device, int function, int reg, ushort val)
{
	mutex_enter(&pci_mutex);
	switch (pci_cfg_type) {
#if	defined(PCI_MECHANISM_1)
	case PCI_MECHANISM_1:
		outl(PCI_CONFADD, PCI_CADDR1(bus, device, function, reg));
		outw(PCI_CONFDATA | (reg & 0x2), val);
		break;
#endif
#if	defined(PCI_MECHANISM_2)
	case PCI_MECHANISM_2: {
		unchar	tmp;

		tmp = pci_config_enable(bus, function);
		outw(PCI_CADDR2(device, reg), val);
		pci_config_restore(tmp);
		break;
	}
#endif
#if	defined(PCI_MECHANISM_SANDALFOOT)
	case PCI_MECHANISM_SANDALFOOT: {
		void *p;

		p = pci_sandalfoot_addr(bus, device, function, reg);
		if (p != NULL) *(unsigned short *)p = val;
		break;
		}
#endif
	}
	mutex_exit(&pci_mutex);
}

static void
i_pci_putl(int bus, int device, int function, int reg, ulong val)
{
	mutex_enter(&pci_mutex);
	switch (pci_cfg_type) {
#if	defined(PCI_MECHANISM_1)
	case PCI_MECHANISM_1:
		outl(PCI_CONFADD, PCI_CADDR1(bus, device, function, reg));
		outl(PCI_CONFDATA, val);
		break;
#endif
#if	defined(PCI_MECHANISM_2)
	case PCI_MECHANISM_2: {
		unchar	tmp;

		tmp = pci_config_enable(bus, function);
		outl(PCI_CADDR2(device, reg), val);
		pci_config_restore(tmp);
		break;
	}
#endif
#if	defined(PCI_MECHANISM_SANDALFOOT)
	case PCI_MECHANISM_SANDALFOOT: {
		void *p;

		p = pci_sandalfoot_addr(bus, device, function, reg);
		if (p != NULL) *(unsigned long *)p = val;
		break;
		}
#endif
	}
	mutex_exit(&pci_mutex);
}

/*
 * This code determines if this system supports PCI and which
 * type of configuration access method is used
 */

static int
pci_check()
{
	int rc;

	rc = pci_identify_cfg_type();
	if (rc == 0)
		return (DDI_FAILURE);
	pci_cfg_type = rc;
	mutex_init(&pci_mutex, "PCI Global Mutex", MUTEX_DRIVER, (void *)NULL);
	return (DDI_SUCCESS);
}

static int
pci_identify_cfg_type()
{
	/* Check to see if the config mechanism has been set in /etc/system */
	switch (PCI_CFG_TYPE) {
	default:
	case 0:
		break;
#if	defined(PCI_MECHANISM_1)
	case PCI_MECHANISM_1:
		return (PCI_MECHANISM_1);
#endif
#if	defined(PCI_MECHANISM_2)
	case PCI_MECHANISM_2:
		return (PCI_MECHANISM_2);
#endif
#if	defined(PCI_MECHANISM_SANDALFOOT)
	case PCI_MECHANISM_SANDALFOOT:
		return (PCI_MECHANISM_SANDALFOOT);
#endif
	case -1:
		return (0);
	}

	/* call one of the PCI detection algorithms */
	switch (PCI_PROBE_TYPE) {
	default:
#if	defined(PCI_PROBE_DIRECT)
	case PCI_PROBE_DIRECT:
		return (pci_probe_direct());
#endif
	case -1:
		return (0);
	}
}

#if	defined(PCI_PROBE_DIRECT)
#if	defined(__ppc) && defined(PCI_MECHANISM_SANDALFOOT) && \
	defined(PCI_MECHANISM_1)
static int
pci_probe_direct(void)
{
	if (inl(PCI_CONFADD) == -1) {
		return (PCI_MECHANISM_SANDALFOOT);
	} else {
		return (PCI_MECHANISM_1);
	}
}
#endif	/* ppc && sandalfoot && mech_1 */
#endif	/* PCI_PROBE_DIRECT */
