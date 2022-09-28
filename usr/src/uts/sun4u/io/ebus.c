/*
 * Copyright (c) 1991-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)ebus.c	1.17	96/08/30 SMI"

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/pci.h>
#include <sys/sunddi.h>
#include <sys/autoconf.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <sys/ebus.h>
#include <sys/ddi_impldefs.h>

#ifdef DEBUG
static u_int ebus_debug_flags = 0;
#endif

/*
 * The i86pc support in this file was used to debug the multigrain
 * and honey nut cheerio prototype cards on PC's.  There is no plan
 * to support this driver for x86.
 */
#if defined(i86pc)
static u_int ebus_set_tcr = 1;
#endif

/*
 * The values of the following variables are used to initialize
 * the cache line size and latency timer registers in the ebus
 * configuration header.  Variables are used instead of constants
 * to allow tuning from the /etc/system file.
 */
static u_int ebus_cache_line_size = 0x10;	/* 64 bytes */
static u_int ebus_latency_timer = 0x40;		/* 64 PCI cycles */

/*
 * function prototypes for bus ops routines:
 */
static int
ebus_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
	off_t offset, off_t len, caddr_t *addrp);
static int
ebus_ctlops(dev_info_t *dip, dev_info_t *rdip,
	ddi_ctl_enum_t op, void *arg, void *result);

/*
 * function prototypes for dev ops routines:
 */
static int ebus_identify(dev_info_t *dip);
static int ebus_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int ebus_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);
#if defined(i86pc)
static ddi_intrspec_t ebus_get_intrspec(dev_info_t *dip,
	dev_info_t *rdip, u_int inumber);
static int ebus_add_intrspec(dev_info_t *dip, dev_info_t *rdip,
	ddi_intrspec_t intrspec, ddi_iblock_cookie_t *iblock_cookiep,
	ddi_idevice_cookie_t *idevice_cookiep,
	u_int (*int_handler)(caddr_t int_handler_arg),
	caddr_t int_handler_arg, int kind);
static void ebus_remove_intrspec(dev_info_t *dip, dev_info_t *rdip,
	ddi_intrspec_t intrspec, ddi_iblock_cookie_t iblock_cookie);
static u_int ebus_intr(caddr_t arg);
#endif

/*
 * general function prototypes:
 */
static int ebus_config(ebus_devstate_t *ebus_p);
#if defined(i86pc)
static int set_timing_control_regs(dev_info_t *dip);
#endif

#define	getprop(dip, name, addr, intp)		\
		ddi_getlongprop(DDI_DEV_T_NONE, (dip), DDI_PROP_DONTPASS, \
				(name), (caddr_t)(addr), (intp))

/*
 * bus ops and dev ops structures:
 */
static struct bus_ops ebus_bus_ops = {
	BUSO_REV,
	ebus_map,
#if defined(i86pc)
	ebus_get_intrspec,
	ebus_add_intrspec,
	ebus_remove_intrspec,
#else
	i_ddi_get_intrspec,
	i_ddi_add_intrspec,
	i_ddi_remove_intrspec,
#endif
	i_ddi_map_fault,
	ddi_dma_map,
	ddi_dma_allochdl,
	ddi_dma_freehdl,
	ddi_dma_bindhdl,
	ddi_dma_unbindhdl,
	ddi_dma_flush,
	ddi_dma_win,
	ddi_dma_mctl,
	ebus_ctlops,
	ddi_bus_prop_op,
	0,	/* (*bus_get_eventcookie)();	*/
	0,	/* (*bus_add_eventcall)();	*/
	0,	/* (*bus_remove_eventcall)();	*/
	0	/* (*bus_post_event)();		*/
};

static struct dev_ops ebus_ops = {
	DEVO_REV,
	0,
	ddi_no_info,
	ebus_identify,
	0,
	ebus_attach,
	ebus_detach,
	nodev,
	(struct cb_ops *)0,
	&ebus_bus_ops
};


/*
 * module definitions:
 */
#include <sys/modctl.h>
extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops, 	/* Type of module.  This one is a driver */
	"ebus nexus driver",	/* Name of module. */
	&ebus_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

/*
 * driver global data:
 */
static void *per_ebus_state;		/* per-ebus soft state pointer */


int
_init(void)
{
	int e;

	/*
	 * Initialize per-ebus soft state pointer.
	 */
	e = ddi_soft_state_init(&per_ebus_state,
				sizeof (ebus_devstate_t), 1);
	if (e != 0)
		return (e);

	/*
	 * Install the module.
	 */
	e = mod_install(&modlinkage);
	if (e != 0)
		ddi_soft_state_fini(&per_ebus_state);
	return (e);
}

int
_fini(void)
{
	int e;

	/*
	 * Remove the module.
	 */
	e = mod_remove(&modlinkage);
	if (e != 0)
		return (e);

	/*
	 * Free the soft state info.
	 */
	ddi_soft_state_fini(&per_ebus_state);
	return (e);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/* device driver entry points */

/*
 * identify entry point:
 *
 * Identifies with nodes named "ebus".
 */
static int
ebus_identify(dev_info_t *dip)
{
	char *name = ddi_get_name(dip);
	int rc = DDI_NOT_IDENTIFIED;

	DBG1(D_IDENTIFY, NULL, "trying dip=%x\n", dip);
#if defined(i86pc)
	if (strcmp(name, "ebus") == 0 || strcmp(name, "pci108e,1000") == 0) {
#else
	if (strcmp(name, "ebus") == 0) {
#endif
		DBG1(D_IDENTIFY, NULL, "identified dip=%x\n", dip);
		rc = DDI_IDENTIFIED;
	}
	return (rc);
}

/*
 * attach entry point:
 *
 * normal attach:
 *
 *	create soft state structure (dip, reg, nreg and state fields)
 *	map in configuration header
 *	make sure device is properly configured
 *	report device
 */
static int
ebus_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	ebus_devstate_t *ebus_p;	/* per ebus state pointer */
	int i, n, instance;

	DBG1(D_ATTACH, NULL, "dip=%x\n", dip);
	switch (cmd) {
	case DDI_ATTACH:

		/*
		 * Allocate soft state for this instance.
		 */
		instance = ddi_get_instance(dip);
		if (ddi_soft_state_zalloc(per_ebus_state, instance)
				!= DDI_SUCCESS) {
			DBG(D_ATTACH, NULL, "failed to alloc soft state\n");
			return (DDI_FAILURE);
		}
		ebus_p = get_ebus_soft_state(instance);
		ebus_p->dip = dip;

		/*
		 * Get a copy of the "reg" property for the ebus node.
		 * The ebus node has 3 reg entries:
		 *
		 *	1) configuration header (PCI config space)
		 *	2) flash prom (PCI memory space)
		 *	3) rest of ebus (PCI memory space)
		 *
		 * The "reg" property entries for ebus children are
		 * just offsets from 1 or 2.  We keep a copy of the
		 * the ebus node's "reg" property for use in the
		 * map entry point.
		 */
		if (getprop(dip, "reg", &ebus_p->reg, &i) != DDI_SUCCESS) {
			cmn_err(CE_WARN, "%s%d: no reg property\n",
				ddi_get_name(dip), instance);
			free_ebus_soft_state(instance);
			return (DDI_FAILURE);
		}
		ebus_p->nreg = i / sizeof (pci_regspec_t);
		if (ebus_p->nreg < 3) {
			cmn_err(CE_WARN, "%s%d: reg property has %d entries\n",
				ddi_get_name(dip), instance, ebus_p->nreg);
			kmem_free((caddr_t)&ebus_p->reg, i);
			free_ebus_soft_state(instance);
			return (DDI_FAILURE);
		}
#ifdef DEBUG
		DBG1(D_MAP, ebus_p, "%x reg entries:\n", ebus_p->nreg);
		for (n = 0; n < ebus_p->nreg; n++) {
			DBG5(D_MAP, ebus_p, "(%x,%x,%x)(%x,%x)\n",
				ebus_p->reg[n].pci_phys_hi,
				ebus_p->reg[n].pci_phys_mid,
				ebus_p->reg[n].pci_phys_low,
				ebus_p->reg[n].pci_size_hi,
				ebus_p->reg[n].pci_size_low);
		}
#endif

#if defined(i86pc)
		if (ebus_set_tcr) {
			if (!set_timing_control_regs(dip)) {
				kmem_free((caddr_t)&ebus_p->reg, i);
				free_ebus_soft_state(instance);
				return (DDI_FAILURE);
			}
		}
#endif

		/*
		 * Make sure the master enable and memory access enable
		 * bits are set in the config command register.
		 */
		if (!ebus_config(ebus_p)) {
			kmem_free((caddr_t)&ebus_p->reg, i);
			free_ebus_soft_state(instance);
			return (DDI_FAILURE);
		}

#if defined(i86pc)
		/*
		 * Install the common interrupt handler.
		 */
		if (ddi_add_intr(dip, 0, &ebus_p->iblock,
				&ebus_p->idevice, ebus_intr,
				(caddr_t)ebus_p) != DDI_SUCCESS) {
			cmn_err(CE_WARN,
				"%s%d: can't install interrupt handler\n",
				ddi_get_name(dip), instance);
			kmem_free((caddr_t)&ebus_p->reg, i);
			free_ebus_soft_state(instance);
			return (DDI_FAILURE);
		}
		ebus_p->intr_slot[0].name = "se";
		ebus_p->intr_slot[1].name = "su";
		ebus_p->intr_slot[2].name = "SUNW,CS4231";
		ebus_p->intr_slot[3].name = "ecpp";
		ebus_p->intr_slot[4].name = "fdthree";
#endif
		/*
		 * Make the state as attached and report the device.
		 */
		ebus_p->state = ATTACHED;
		ddi_report_dev(dip);
		DBG(D_ATTACH, ebus_p, "returning\n");
		return (DDI_SUCCESS);

	case DDI_RESUME:

		instance = ddi_get_instance(dip);
		ebus_p = get_ebus_soft_state(instance);

		/*
		 * Make sure the master enable and memory access enable
		 * bits are set in the config command register.
		 */
		if (!ebus_config(ebus_p)) {
			kmem_free((caddr_t)&ebus_p->reg, i);
			free_ebus_soft_state(instance);
			return (DDI_FAILURE);
		}

		ebus_p->state = RESUMED;
		return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);
}

/*
 * detach entry point:
 */
static int
ebus_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int instance = ddi_get_instance(dip);
	ebus_devstate_t *ebus_p = get_ebus_soft_state(instance);

	switch (cmd) {
	case DDI_DETACH:
		DBG1(D_DETACH, ebus_p, "DDI_DETACH dip=%x\n", dip);
		kmem_free((caddr_t)&ebus_p->reg,
				ebus_p->nreg * sizeof (pci_regspec_t));
		free_ebus_soft_state(instance);
		return (DDI_SUCCESS);

	case DDI_SUSPEND:
		DBG1(D_DETACH, ebus_p, "DDI_SUSPEND dip=%x\n", dip);
		ebus_p->state = SUSPENDED;
		return (DDI_SUCCESS);

	case DDI_PM_SUSPEND:
		DBG1(D_DETACH, ebus_p, "DDI_PM_SUSPEND dip=%x\n", dip);
		ebus_p->state = PM_SUSPENDED;
		return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);
}

/* bus driver entry points */

/*
 * bus map entry point:
 *
 * 	if map request is for an rnumber
 *		get the corresponding regspec from device node
 * 	build a new regspec in our parent's format
 *	build a new map_req with the new regspec
 *	call up the tree to complete the mapping
 */
static int
ebus_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
	off_t off, off_t len, caddr_t *addrp)
{
	ebus_devstate_t *ebus_p = get_ebus_soft_state(ddi_get_instance(dip));
	ebus_regspec_t *ebus_rp;
	pci_regspec_t pci_reg;
	ddi_map_req_t p_map_request;
	int rnumber, i, n;
	off_t noff, nlen;
	u_int base_reg;
	int ebus_rnumber = -1;

	/*
	 * Handle the mapping according to its type.
	 */
	DBG4(D_MAP, ebus_p, "rdip=%s%d: off=%x len=%x\n",
			ddi_get_name(rdip), ddi_get_instance(rdip), off, len);
	switch (mp->map_type) {
	case DDI_MT_REGSPEC:

		/*
		 * We assume the register specification is in ebus format.
		 * We must convert it into a PCI format regspec and pass
		 * the request to our parent.
		 */
		DBG3(D_MAP, ebus_p, "rdip=%s%d: REGSPEC - handlep=%x\n",
			ddi_get_name(rdip), ddi_get_instance(rdip),
			mp->map_handlep);
		ebus_rp = (ebus_regspec_t *)mp->map_obj.rp;
		base_reg = ebus_rp->addr_hi;
		noff = ebus_rp->addr_low;
		nlen = ebus_rp->size;
		break;

	case DDI_MT_RNUMBER:

		/*
		 * Get the "reg" property from the device node and convert
		 * it to our parent's format.
		 */
		rnumber = mp->map_obj.rnumber;
		DBG4(D_MAP, ebus_p, "rdip=%s%d: rnumber=%x handlep=%x\n",
			ddi_get_name(rdip), ddi_get_instance(rdip),
			rnumber, mp->map_handlep);

		if (getprop(rdip, "reg", &ebus_rp, &i) != DDI_SUCCESS) {
			DBG(D_MAP, ebus_p, "can't get reg property\n");
			return (DDI_ME_RNUMBER_RANGE);
		}
		n = i / sizeof (ebus_regspec_t);

		if (rnumber < 0 || rnumber >= n) {
			DBG(D_MAP, ebus_p, "rnumber out of range\n");
			return (DDI_ME_RNUMBER_RANGE);
		}
		base_reg = ebus_rp[rnumber].addr_hi;
		noff = ebus_rp[rnumber].addr_low;
		nlen = ebus_rp[rnumber].size;
		kmem_free((caddr_t)ebus_rp, i);
		break;

	default:
		return (DDI_ME_INVAL);

	}

	/*
	 * Now we have a copy the "reg" entry we're attempting to map.
	 * It's address is just an offset from the boot ROM or ebus
	 * base.
	 */
	DBG4(D_MAP, ebus_p,
		"regspec: address=%x size=%x base_reg=%x rnumber=%x\n",
		noff, nlen, base_reg, rnumber);
	for (i = 0; i < ebus_p->nreg; i++) {
		DBG5(D_MAP, ebus_p, "(%x,%x,%x)(%x,%x)\n",
			ebus_p->reg[i].pci_phys_hi,
			ebus_p->reg[i].pci_phys_mid,
			ebus_p->reg[i].pci_phys_low,
			ebus_p->reg[i].pci_size_hi,
			ebus_p->reg[i].pci_size_low);
		if (base_reg == PCI_REG_REG_G(ebus_p->reg[i].pci_phys_hi))
			ebus_rnumber = i;
	}
	if (ebus_rnumber == -1) {
		DBG1(D_MAP, ebus_p, "bad base reg specification (%x)\n",
			base_reg);
		return (DDI_ME_INVAL);
	}

	pci_reg = ebus_p->reg[ebus_rnumber];
#ifdef DEBUG
	DBG5(D_MAP, ebus_p, "(%x,%x,%x)(%x,%x)\n",
		pci_reg.pci_phys_hi,
		pci_reg.pci_phys_mid,
		pci_reg.pci_phys_low,
		pci_reg.pci_size_hi,
		pci_reg.pci_size_low);
#endif

	noff += off;
	nlen += len;
	DBG3(D_MAP, ebus_p, "regspec: address=%x size=%x pci_rnumber=%x\n",
		noff, nlen, ebus_rnumber);
	p_map_request = *mp;
	p_map_request.map_type = DDI_MT_REGSPEC;
	p_map_request.map_obj.rp = (struct regspec *)&pci_reg;
	i = ddi_map(dip, &p_map_request, noff, nlen, addrp);
	DBG1(D_MAP, ebus_p, "parent returned %x\n", i);
	return (i);
}

#if defined(i86pc)
ddi_intrspec_t
ebus_get_intrspec(dev_info_t *dip, dev_info_t *rdip, u_int inumber)
{
	ebus_devstate_t *ebus_p = get_ebus_soft_state(ddi_get_instance(dip));
	struct intrspec *ispec;
	int *ip, i;

	/*
	 * Get the requested inumber from the device by checking the
	 * interrupts property.
	 */
	DBG3(D_G_ISPEC, ebus_p, "rdip=%s%d inumber=%x\n",
		ddi_get_name(rdip), ddi_get_instance(rdip), inumber);
	if (inumber != 0)
		return ((ddi_intrspec_t)0);
	if (ddi_get_instance(rdip) != 0)
		return ((ddi_intrspec_t)0);
	return ((ddi_intrspec_t *)ispec);
}

static int
ebus_add_intrspec(dev_info_t *dip, dev_info_t *rdip,
			ddi_intrspec_t intrspec,
			ddi_iblock_cookie_t *iblock_cookiep,
			ddi_idevice_cookie_t *idevice_cookiep,
			u_int (*int_handler)(caddr_t int_handler_arg),
			caddr_t int_handler_arg, int kind)
{
	ebus_devstate_t *ebus_p = get_ebus_soft_state(ddi_get_instance(dip));
	char *rname = ddi_get_name(rdip);
	int i;

	DBG2(D_A_ISPEC, ebus_p, "rdip=%s%d\n", rname, ddi_get_instance(rdip));
	for (i = 0; i < MAX_EBUS_DEVS; i++) {
		if (ebus_p->intr_slot[i].name == NULL)
			continue;
		if (strcmp(ebus_p->intr_slot[i].name, rname) != 0)
			continue;
		if (ebus_p->intr_slot[i].inuse) {
			DBG(D_A_ISPEC, ebus_p, "handler already installed!\n");
			return (DDI_FAILURE);
		}

		ebus_p->intr_slot[i].handler = int_handler;
		ebus_p->intr_slot[i].arg = int_handler_arg;
		ebus_p->intr_slot[i].inuse = 1;

		/*
		 * Program the iblock and idevice cookies.
		 */
		if (iblock_cookiep)
			*iblock_cookiep = (ddi_iblock_cookie_t)
				ebus_p->iblock;
		if (idevice_cookiep)
			*idevice_cookiep = ebus_p->idevice;
		DBG1(D_A_ISPEC, ebus_p, "handler installed in slot %x\n", i);
		return (DDI_SUCCESS);
	}
}

static void
ebus_remove_intrspec(dev_info_t *dip, dev_info_t *rdip,
			ddi_intrspec_t intrspec,
			ddi_iblock_cookie_t iblock_cookie)
{
	ebus_devstate_t *ebus_p = get_ebus_soft_state(ddi_get_instance(dip));
	char *rname = ddi_get_name(rdip);
	int i;

	DBG2(D_R_ISPEC, ebus_p, "rdip=%s%d\n", rname, ddi_get_instance(rdip));
	for (i = 0; i < MAX_EBUS_DEVS; i++) {
		if (ebus_p->intr_slot[i].name == NULL)
			continue;
		if (strcmp(ebus_p->intr_slot[i].name, rname) != 0)
			continue;
		if (ebus_p->intr_slot[i].inuse) {
			ebus_p->intr_slot[i].inuse = 0;
			break;
		}
	}
}
#endif

/*
 * control ops entry point:
 *
 * Requests handled completely:
 *	DDI_CTLOPS_INITCHILD
 *	DDI_CTLOPS_UNINITCHILD
 *	DDI_CTLOPS_REPORTDEV
 *	DDI_CTLOPS_REGSIZE
 *	DDI_CTLOPS_NREGS
 *
 * All others passed to parent.
 */
static int
ebus_ctlops(dev_info_t *dip, dev_info_t *rdip,
	ddi_ctl_enum_t op, void *arg, void *result)
{
	ebus_devstate_t *ebus_p = get_ebus_soft_state(ddi_get_instance(dip));
	ebus_regspec_t *ebus_rp;
	int i, n;
	char name[10];

	switch (op) {
	case DDI_CTLOPS_INITCHILD:

		/*
		 * Set the address portion of the node name based on the
		 * address/offset.
		 */
		DBG2(D_CTLOPS, ebus_p, "DDI_CTLOPS_INITCHILD: rdip=%s%d\n",
			ddi_get_name((dev_info_t *)arg),
			ddi_get_instance((dev_info_t *)arg));
		if (ddi_getlongprop(DDI_DEV_T_NONE, (dev_info_t *)arg,
					DDI_PROP_DONTPASS, "reg",
					(caddr_t)&ebus_rp,
					&i) != DDI_SUCCESS) {
			DBG(D_CTLOPS, ebus_p, "can't get reg property\n");
			return (DDI_FAILURE);
		}
		sprintf(name, "%x,%x", ebus_rp->addr_hi, ebus_rp->addr_low);
		ddi_set_name_addr((dev_info_t *)arg, name);
		kmem_free((caddr_t)ebus_rp, i);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_UNINITCHILD:

		DBG2(D_CTLOPS, ebus_p, "DDI_CTLOPS_UNINITCHILD: rdip=%s%d\n",
			ddi_get_name((dev_info_t *)arg),
			ddi_get_instance((dev_info_t *)arg));
		ddi_set_name_addr((dev_info_t)arg, NULL);
		ddi_remove_minor_node((dev_info_t)arg, NULL);
		impl_rem_dev_props((dev_info_t)arg);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_REPORTDEV:

		DBG2(D_CTLOPS, ebus_p, "DDI_CTLOPS_REPORTDEV: rdip=%s%d\n",
			ddi_get_name(rdip), ddi_get_instance(rdip));
		cmn_err(CE_CONT, "?%s%d at %s%d: offset %s\n",
			ddi_get_name(rdip), ddi_get_instance(rdip),
			ddi_get_name(dip), ddi_get_instance(dip),
			ddi_get_name_addr(rdip));
		return (DDI_SUCCESS);

	case DDI_CTLOPS_REGSIZE:

		DBG2(D_CTLOPS, ebus_p, "DDI_CTLOPS_REGSIZE: rdip=%s%d\n",
			ddi_get_name(rdip), ddi_get_instance(rdip));
		if (getprop(rdip, "reg", &ebus_rp, &i) != DDI_SUCCESS) {
			DBG(D_CTLOPS, ebus_p, "can't get reg property\n");
			return (DDI_FAILURE);
		}
		n = i / sizeof (ebus_regspec_t);
		if (*(int *)arg < 0 || *(int *)arg >= n) {
			DBG(D_MAP, ebus_p, "rnumber out of range\n");
			return (DDI_ME_RNUMBER_RANGE);
		}
		*((off_t *)result) = ebus_rp[*(int *)arg].size;
		kmem_free((caddr_t)ebus_rp, i);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_NREGS:

		DBG2(D_CTLOPS, ebus_p, "DDI_CTLOPS_NREGS: rdip=%s%d\n",
			ddi_get_name(rdip), ddi_get_instance(rdip));
		if (getprop(rdip, "reg", &ebus_rp, &i) != DDI_SUCCESS) {
			DBG(D_CTLOPS, ebus_p, "can't get reg property\n");
			return (DDI_FAILURE);
		}
		*((u_int *) result) = i / sizeof (ebus_regspec_t);
		kmem_free((caddr_t)ebus_rp, i);
		return (DDI_SUCCESS);
	}

	/*
	 * Now pass the request up to our parent.
	 */
	DBG2(D_CTLOPS, ebus_p, "passing request to parent: rdip=%s%d\n",
		ddi_get_name(rdip), ddi_get_instance(rdip));
	return (ddi_ctlops(dip, rdip, op, arg, result));
}


static int
ebus_config(ebus_devstate_t *ebus_p)
{
	ddi_acc_handle_t conf_handle;
	unsigned short comm;

	/*
	 * Make sure the master enable and memory access enable
	 * bits are set in the config command register.
	 */
	if (pci_config_setup(ebus_p->dip, &conf_handle) != DDI_SUCCESS)
		return (0);

	comm = pci_config_getw(conf_handle, PCI_CONF_COMM),
#ifdef DEBUG
	DBG1(D_MAP, ebus_p, "command register was 0x%x\n", comm);
#endif
	comm |= (PCI_COMM_ME|PCI_COMM_MAE|PCI_COMM_SERR_ENABLE|
			PCI_COMM_PARITY_DETECT);
	pci_config_putw(conf_handle, PCI_CONF_COMM, comm),
#ifdef DEBUG
	DBG1(D_MAP, ebus_p, "command register is now 0x%x\n", comm);
#endif
	pci_config_putb(conf_handle, PCI_CONF_CACHE_LINESZ,
			(uchar_t)ebus_cache_line_size);
	pci_config_putb(conf_handle, PCI_CONF_LATENCY_TIMER,
			(uchar_t)ebus_latency_timer);
	pci_config_teardown(&conf_handle);
	return (1);
}

#if defined(i86pc)
static u_int
ebus_intr(caddr_t arg)
{
	ebus_devstate_t *ebus_p = (ebus_devstate_t *)arg;
	u_int r, result = DDI_INTR_UNCLAIMED;
	int i;

	DBG2(D_INTR, ebus_p, "%s%x\n",
		ddi_get_name(ebus_p->dip),
		ddi_get_instance(ebus_p->dip));
	for (i = 0; i < MAX_EBUS_DEVS; i++) {
		if (!ebus_p->intr_slot[i].inuse)
			continue;
		DBG3(D_INTR, ebus_p, "%s%x handler for %s\n",
			ddi_get_name(ebus_p->dip),
			ddi_get_instance(ebus_p->dip),
			ebus_p->intr_slot[i].name);
		r = ebus_p->intr_slot[i].handler(
				ebus_p->intr_slot[i].arg);
		if (r == DDI_INTR_CLAIMED)
			result = DDI_INTR_CLAIMED;
	}
	return (result);
}
#endif

#if defined(i86pc)
static int
set_timing_control_regs(dev_info_t *dip)
{
	ddi_device_acc_attr_t attr;
	ddi_acc_handle_t acc;
	caddr_t a;
	u_int *tcr_p;

	attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;
	attr.devacc_attr_endian_flags = DDI_NEVERSWAP_ACC;
	if (ddi_regs_map_setup(dip, 2, &a, TCR_OFFSET, TCR_LENGTH, &attr,
				&acc) != DDI_SUCCESS)
		return (0);

	tcr_p = (u_int *)a;
	*tcr_p++ = TCR1;
	*tcr_p++ = TCR2;
	*tcr_p++ = TCR3;

	ddi_regs_map_free(&acc);
}
#endif

#ifdef DEBUG
extern void prom_printf(char *, ...);

static void
ebus_debug(u_int flag, ebus_devstate_t *ebus_p, char *fmt,
	int a1, int a2, int a3, int a4, int a5)
{
	char *s;

	if (ebus_debug_flags & flag) {
		switch (flag) {
		case D_IDENTIFY:
			s = "identify"; break;
		case D_ATTACH:
			s = "attach"; break;
		case D_DETACH:
			s = "detach"; break;
		case D_MAP:
			s = "map"; break;
		case D_CTLOPS:
			s = "ctlops"; break;
#if defined(i86pc)
		case D_G_ISPEC:
			s = "get_intrspec"; break;
		case D_A_ISPEC:
			s = "add_intrspec"; break;
		case D_R_ISPEC:
			s = "remove_intrspec"; break;
		case D_INTR:
			s = "intr"; break;
#endif
		}
		if (ebus_p)
			cmn_err(CE_CONT, "%s%d: %s: ",
				ddi_get_name(ebus_p->dip),
				ddi_get_instance(ebus_p->dip), s);
		else
			cmn_err(CE_CONT, "ebus: ");
		cmn_err(CE_CONT, fmt, a1, a2, a3, a4, a5);
	}
}
#endif
