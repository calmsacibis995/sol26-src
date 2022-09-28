/*
 * Copyright (c) 1990-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rootnex.c	1.63	96/09/24 SMI"

/*
 * PowerPC root nexus driver for the PReP platform
 *      based on i86pc root nexus driver.
 */

#include <sys/conf.h>
#include <sys/autoconf.h>
#include <sys/sysmacros.h>
#include <sys/debug.h>
#include <sys/psw.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/ddidmareq.h>
#include <sys/promif.h>
#include <sys/devops.h>
#include <sys/cpu.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_dev.h>
#include <sys/map.h>
#include <sys/mman.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/hat_ppcmmu.h>
#include <sys/vmmac.h>
#include <sys/avintr.h>
#include <sys/errno.h>
#include <sys/modctl.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>



#define	INTLEVEL_SOFT	0		/* XXX temp kludge XXX */

#define	ppc_pp_map(pp, kaddr) \
	segkmem_mapin(&kvseg, kaddr, MMU_PAGESIZE, \
	    PROT_READ | PROT_WRITE | HAT_NOSYNC, page_pptonum(pp), 0)

extern void ppc_va_map(caddr_t vaddr, struct as *asp, caddr_t kaddr);

/*
 * DMA related static data
 */
static uintptr_t dvma_call_list_id = 0;

/*
 * Hack to handle poke faults on Calvin-class machines
 */
extern int pokefault;
static kmutex_t pokefault_mutex;

extern u_int ppcmmu_getpfnum(struct as *as, caddr_t addr);

/*
 * Internal functions
 */
static int rootnex_merge_pseudo(dev_info_t *parent, dev_info_t *child);
static int rootnex_ctl_children(dev_info_t *dip, dev_info_t *rdip,
	ddi_ctl_enum_t ctlop, dev_info_t *child);


/*
 * config information
 */

static int
rootnex_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
    off_t offset, off_t len, caddr_t *vaddrp);

static ddi_intrspec_t
rootnex_get_intrspec(dev_info_t *dip, dev_info_t *rdip,	u_int inumber);

static int
rootnex_add_intrspec(dev_info_t *dip, dev_info_t *rdip,
    ddi_intrspec_t intrspec, ddi_iblock_cookie_t *iblock_cookiep,
    ddi_idevice_cookie_t *idevice_cookiep,
    u_int (*int_handler)(caddr_t int_handler_arg),
    caddr_t int_handler_arg, int kind);

static void
rootnex_remove_intrspec(dev_info_t *dip, dev_info_t *rdip,
    ddi_intrspec_t intrspec, ddi_iblock_cookie_t iblock_cookie);

static int
rootnex_map_fault(dev_info_t *dip, dev_info_t *rdip,
    struct hat *hat, struct seg *seg, caddr_t addr,
    struct devpage *dp, u_int pfn, u_int prot, u_int lock);

static int
rootnex_dma_allochdl(dev_info_t *, dev_info_t *, ddi_dma_attr_t *,
    int (*waitfp)(caddr_t), caddr_t arg, ddi_dma_handle_t *);

static int
    rootnex_dma_freehdl(dev_info_t *, dev_info_t *, ddi_dma_handle_t);

static int
rootnex_dma_bindhdl(dev_info_t *, dev_info_t *, ddi_dma_handle_t,
    struct ddi_dma_req *, ddi_dma_cookie_t *, u_int *);

static int
rootnex_dma_unbindhdl(dev_info_t *, dev_info_t *, ddi_dma_handle_t);

static int
rootnex_dma_flush(dev_info_t *, dev_info_t *, ddi_dma_handle_t,
    off_t, u_int, u_int);

static int
rootnex_dma_win(dev_info_t *, dev_info_t *, ddi_dma_handle_t,
    uint_t, off_t *, uint_t *, ddi_dma_cookie_t *, uint_t *);

static int
rootnex_dma_mctl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, enum ddi_dma_ctlops request,
    off_t *offp, u_int *lenp, caddr_t *objp, u_int cache_flags);

static int
rootnex_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t, void *, void *);

static int rootnex_map_regspec(ddi_map_req_t *mp, caddr_t *vaddrp);
static int rootnex_dma_map(dev_info_t *dip, dev_info_t *rdip,
    struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep);

static struct bus_ops rootnex_bus_ops = {
	BUSO_REV,
	rootnex_map,
	rootnex_get_intrspec,
	rootnex_add_intrspec,
	rootnex_remove_intrspec,
	rootnex_map_fault,
	rootnex_dma_map,
	rootnex_dma_allochdl,
	rootnex_dma_freehdl,
	rootnex_dma_bindhdl,
	rootnex_dma_unbindhdl,
	rootnex_dma_flush,
	rootnex_dma_win,
	rootnex_dma_mctl,
	rootnex_ctlops,
	ddi_bus_prop_op,
};

static int rootnex_identify(dev_info_t *devi);
static int rootnex_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);
static int rootnex_io_rdsync(ddi_dma_impl_t *hp);
static int rootnex_io_wtsync(ddi_dma_impl_t *hp, int);
static int rootnex_io_brkup(dev_info_t *dip, dev_info_t *rdip,
    struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep,
    ddi_dma_lim_t *dma_lim, int type);


static struct dev_ops rootnex_ops = {
	DEVO_REV,
	0,		/* refcnt */
	ddi_no_info,	/* info */
	rootnex_identify,
	0,		/* probe */
	rootnex_attach,
	nodev,		/* detach */
	nodev,		/* reset */
	0,		/* cb_ops */
	&rootnex_bus_ops
};

extern struct mod_ops mod_driverops;
extern struct dev_ops rootnex_ops;

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This one is a nexus driver */
	"prep root nexus",
	&rootnex_ops,	/* Driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (EBUSY);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * rootnex_identify:
 *
 * 	identify the root nexus for the machine.
 *
 */

static int
rootnex_identify(dev_info_t *devi)
{
	if (ddi_root_node() == devi)
		return (DDI_IDENTIFIED);
	else
		return (DDI_NOT_IDENTIFIED);
}

/*
 * rootnex_attach:
 *
 *	attach the root nexus.
 */
/*ARGSUSED*/
static int
rootnex_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	mutex_init(&pokefault_mutex, "pokefault lock",
	    MUTEX_SPIN_DEFAULT, (void *)ipltospl(15));

	cmn_err(CE_CONT, "?root nexus = %s\n", ddi_get_name(devi));
	return (DDI_SUCCESS);
}


/*
 * #define DDI_MAP_DEBUG (c.f. ddi_impl.c)
 */
#ifdef	DDI_MAP_DEBUG
extern int ddi_map_debug_flag;
#define	ddi_map_debug	if (ddi_map_debug_flag) prom_printf
#endif	DDI_MAP_DEBUG


static int
rootnex_map_regspec(ddi_map_req_t *mp, caddr_t *vaddrp)
{
	extern struct seg kvseg;
	u_long base, a, cvaddr;
	u_int npages, pgoffset;
	register struct regspec *rp;
	ddi_acc_hdl_t *hp;

	hp = (ddi_acc_hdl_t *)mp->map_handlep;
	if (hp) {
		impl_acc_hdl_init(hp);
	}

	rp = mp->map_obj.rp;
	base = (u_long)rp->regspec_addr & (~MMU_PAGEOFFSET); /* base addr */
	pgoffset = (u_long)rp->regspec_addr & MMU_PAGEOFFSET; /* offset */

	if (rp->regspec_size == 0) {
#ifdef  DDI_MAP_DEBUG
		ddi_map_debug("rootnex_map_regspec: zero regspec_size\n");
#endif  DDI_MAP_DEBUG
		return (DDI_ME_INVAL);
	}

	if (mp->map_flags & DDI_MF_DEVICE_MAPPING)
		*vaddrp = (caddr_t)mmu_btop(base);
	else {
		npages = mmu_btopr(rp->regspec_size + pgoffset);

#ifdef	DDI_MAP_DEBUG
		ddi_map_debug("rootnex_map_regspec: Mapping %d \
pages physical %x ",
		    npages, base);
#endif	DDI_MAP_DEBUG

		a = rmalloc(kernelmap, (long)npages);
		if (a == NULL) {
			return (DDI_ME_NORESOURCES);
		}
		cvaddr = (u_long)kmxtob(a);

		/*
		 * Now map in the pages we've allocated...
		 */

		segkmem_mapin(&kvseg, (caddr_t)cvaddr,
		    (u_int)mmu_ptob(npages), mp->map_prot, mmu_btop(base), 0);

		*vaddrp = (caddr_t)(kmxtob(a) + pgoffset);
	}
#ifdef	DDI_MAP_DEBUG
	ddi_map_debug("at virtual 0x%x\n", *vaddrp);
#endif	DDI_MAP_DEBUG
	return (DDI_SUCCESS);
}

static int
rootnex_unmap_regspec(ddi_map_req_t *mp, caddr_t *vaddrp)
{
	extern struct seg kvseg;
	caddr_t addr = (caddr_t)*vaddrp;
	u_int npages, pgoffset;
	register struct regspec *rp;
	long a;

	if (mp->map_flags & DDI_MF_DEVICE_MAPPING)
		return (0);

	rp = mp->map_obj.rp;
	pgoffset = (u_int)addr & MMU_PAGEOFFSET;

	if (rp->regspec_size == 0) {
#ifdef  DDI_MAP_DEBUG
		ddi_map_debug("rootnex_unmap_regspec: zero regspec_size\n");
#endif  DDI_MAP_DEBUG
		return (DDI_ME_INVAL);
	}

	npages = mmu_btopr(rp->regspec_size + pgoffset);
	segkmem_mapout(&kvseg, (caddr_t)((int)addr & (~MMU_PAGEOFFSET)),
	    (u_int)mmu_ptob(npages));
	a = btokmx(addr);
	rmfree(kernelmap, (long)npages, (u_long)a);

	/*
	 * Destroy the pointer - the mapping has logically gone
	 */
	*vaddrp = (caddr_t)0;

	return (DDI_SUCCESS);
}

static int
rootnex_map_handle(ddi_map_req_t *mp)
{
	ddi_acc_hdl_t *hp;
	u_long base;
	u_int pgoffset;
	u_int flags;
	register struct regspec *rp;

	/*
	 * Set up the hat_flags for the mapping.
	 */
	hp = mp->map_handlep;

	hp->ah_hat_flags = 0;
	flags = hp->ah_acc.devacc_attr_endian_flags &
				(DDI_NEVERSWAP_ACC| DDI_STRUCTURE_LE_ACC|
					DDI_STRUCTURE_BE_ACC);
	switch (flags) {
	case DDI_NEVERSWAP_ACC:
		break;
	case DDI_STRUCTURE_LE_ACC:
		hp->ah_hat_flags |= HAT_STRUCTURE_LE;
		break;
	case DDI_STRUCTURE_BE_ACC:
		return (DDI_FAILURE);
	default:
		return (DDI_REGS_ACC_CONFLICT);
	}

	flags = hp->ah_acc.devacc_attr_dataorder;
	if (flags & DDI_UNORDERED_OK_ACC)
		hp->ah_hat_flags |= HAT_UNORDERED_OK;
	if (flags & DDI_MERGING_OK_ACC)
		hp->ah_hat_flags |= HAT_MERGING_OK;
	if (flags & DDI_LOADCACHING_OK_ACC)
		hp->ah_hat_flags |= HAT_LOADCACHING_OK;
	if (flags & DDI_STORECACHING_OK_ACC)
		hp->ah_hat_flags |= HAT_LOADCACHING_OK;

	rp = mp->map_obj.rp;
	base = (u_long)rp->regspec_addr & (~MMU_PAGEOFFSET); /* base addr */
	pgoffset = (u_long)rp->regspec_addr & MMU_PAGEOFFSET; /* offset */

	if (rp->regspec_size == 0)
		return (DDI_ME_INVAL);

	hp->ah_pfn = mmu_btop(base);
	hp->ah_pnum = mmu_btopr(rp->regspec_size + pgoffset);

	return (DDI_SUCCESS);
}

static int
rootnex_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
	off_t offset, off_t len, caddr_t *vaddrp)
{
	struct regspec *rp, tmp_reg;
	ddi_map_req_t mr = *mp;		/* Get private copy of request */
	int error;

	mp = &mr;

	switch (mp->map_op)  {
	case DDI_MO_MAP_LOCKED:
	case DDI_MO_UNMAP:
	case DDI_MO_MAP_HANDLE:
		break;
	default:
#ifdef	DDI_MAP_DEBUG
		cmn_err(CE_WARN, "rootnex_map: unimplemented map op %d.",
		    mp->map_op);
#endif	DDI_MAP_DEBUG
		return (DDI_ME_UNIMPLEMENTED);
	}

	if (mp->map_flags & DDI_MF_USER_MAPPING)  {
#ifdef	DDI_MAP_DEBUG
		cmn_err(CE_WARN, "rootnex_map: unimplemented map type: user.");
#endif	DDI_MAP_DEBUG
		return (DDI_ME_UNIMPLEMENTED);
	}

	/*
	 * First, if given an rnumber, convert it to a regspec...
	 * (Presumably, this is on behalf of a child of the root node?)
	 */

	if (mp->map_type == DDI_MT_RNUMBER)  {

		int rnumber = mp->map_obj.rnumber;
#ifdef	DDI_MAP_DEBUG
		static char *out_of_range =
		    "rootnex_map: Out of range rnumber <%d>, device <%s>";
#endif	DDI_MAP_DEBUG

		rp = i_ddi_rnumber_to_regspec(rdip, rnumber);
		if (rp == (struct regspec *)0)  {
#ifdef	DDI_MAP_DEBUG
			cmn_err(CE_WARN, out_of_range, rnumber,
			    ddi_get_name(rdip));
#endif	DDI_MAP_DEBUG
			return (DDI_ME_RNUMBER_RANGE);
		}

		/*
		 * Convert the given ddi_map_req_t from rnumber to regspec...
		 */

		mp->map_type = DDI_MT_REGSPEC;
		mp->map_obj.rp = rp;
	}

	/*
	 * Adjust offset and length correspnding to called values...
	 * XXX: A non-zero length means override the one in the regspec
	 * XXX: (regardless of what's in the parent's range?)
	 */

	tmp_reg = *(mp->map_obj.rp);		/* Preserve underlying data */
	rp = mp->map_obj.rp = &tmp_reg;		/* Use tmp_reg in request */

	rp->regspec_addr += (u_int)offset;
	if (len != 0)
		rp->regspec_size = (u_int)len;

	/*
	 * Apply any parent ranges at this level, if applicable.
	 * (This is where nexus specific regspec translation takes place.
	 * Use of this function is implicit agreement that translation is
	 * provided via ddi_apply_range.)
	 */

#ifdef	DDI_MAP_DEBUG
	ddi_map_debug("applying range of parent <%s> to child <%s>...\n",
	    ddi_get_name(dip), ddi_get_name(rdip));
#endif	DDI_MAP_DEBUG

	if ((error = i_ddi_apply_range(dip, rdip, mp->map_obj.rp)) != 0)
		return (error);

	switch (mp->map_op)  {
	case DDI_MO_MAP_LOCKED:

		/*
		 * Set up the locked down kernel mapping to the regspec...
		 */

		return (rootnex_map_regspec(mp, vaddrp));

	case DDI_MO_UNMAP:

		/*
		 * Release mapping...
		 */

		return (rootnex_unmap_regspec(mp, vaddrp));

	case DDI_MO_MAP_HANDLE:

		return (rootnex_map_handle(mp));
	}

	return (DDI_ME_UNIMPLEMENTED);
}

/*
 * rootnex_get_intrspec: rootnex convert an interrupt number to an interrupt
 *			specification. The interrupt number determines
 *			which interrupt spec will be returned if more than
 *			one exists. Look into the parent private data
 *			area of the dev_info structure to find the interrupt
 *			specification.  First check to make sure there is
 *			one that matchs "inumber" and then return a pointer
 *			to it.  Return NULL if one could not be found.
 */
static ddi_intrspec_t
rootnex_get_intrspec(dev_info_t *dip, dev_info_t *rdip, u_int inumber)
{
	struct ddi_parent_private_data *ppdptr;

#ifdef	lint
	dip = dip;
#endif

	/*
	 * convert the parent private data pointer in the childs dev_info
	 * structure to a pointer to a sunddi_compat_hack structure
	 * to get at the interrupt specifications.
	 */
	ppdptr = (struct ddi_parent_private_data *)
	    (DEVI(rdip))->devi_parent_data;

	/*
	 * validate the interrupt number.
	 */
	if (inumber >= ppdptr->par_nintr) {
		return (NULL);
	}

	/*
	 * return the interrupt structure pointer.
	 */
	return ((ddi_intrspec_t)&ppdptr->par_intr[inumber]);
}

/*
 * rootnex_add_intrspec:
 *
 *	Add an interrupt specification.
 */
static int
rootnex_add_intrspec(dev_info_t *dip, dev_info_t *rdip,
	ddi_intrspec_t intrspec, ddi_iblock_cookie_t *iblock_cookiep,
	ddi_idevice_cookie_t *idevice_cookiep,
	u_int (*int_handler)(caddr_t int_handler_arg),
	caddr_t int_handler_arg, int kind)
{
	register struct intrspec *ispec;
	register u_int pri;
	register int soft = 0;
	extern int (*psm_translate_irq)(dev_info_t *, int);

#ifdef	lint
	dip = dip;
#endif

	ispec = (struct intrspec *)intrspec;
	pri = INT_IPL(ispec->intrspec_pri);

	if (kind == IDDI_INTR_TYPE_FAST) {
#ifdef XXX_later
		if (!settrap(rdip, ispec->intrspec_pri, int_handler)) {
			return (DDI_FAILURE);
		}
		ispec->intrspec_func = (u_int (*)()) 1;
#else
		return (DDI_FAILURE);
#endif
	} else {
		struct dev_ops *dops = DEVI(rdip)->devi_ops;
		int hot;

		if (dops->devo_bus_ops) {
			hot = 1;	/* Nexus drivers MUST be MT-safe */
		} else if (dops->devo_cb_ops->cb_flag & D_MP) {
			hot = 1;	/* Most leaves are MT-safe */
		} else {
			hot = 0;	/* MT-unsafe drivers ok (for now) */
		}

		/*
		 * Convert 'soft' pri to "fit" with 4m model
		 */
		soft = (kind == IDDI_INTR_TYPE_SOFT);
		if (soft)
			ispec->intrspec_pri = pri + INTLEVEL_SOFT;
		else {
			ispec->intrspec_pri = pri;
			/*
			 * call psmi to translate the irq with the dip
			 */
			ispec->intrspec_vec = (u_int)
				(*psm_translate_irq)(rdip, ispec->intrspec_vec);
		}

		if (soft) {
			/* register soft interrupt handler */
			if (!add_avsoftintr((void *)ispec, ispec->intrspec_pri,
			    int_handler, DEVI(rdip)->devi_name, int_handler_arg,
			    (hot)? NULL : &unsafe_driver)) {
				return (DDI_FAILURE);
			}
		} else {
			/* register hardware interrupt handler */
			if (!add_avintr((void *)ispec, ispec->intrspec_pri,
			    int_handler, DEVI(rdip)->devi_name,
			    ispec->intrspec_vec, int_handler_arg,
			    (hot)? NULL : &unsafe_driver)) {
				return (DDI_FAILURE);
			}
		}
		ispec->intrspec_func = int_handler;
	}

	if (iblock_cookiep) {
		*iblock_cookiep = (ddi_iblock_cookie_t)ipltospl(pri);
	}

	if (idevice_cookiep) {
		idevice_cookiep->idev_vector = 0;
		if (kind == IDDI_INTR_TYPE_SOFT) {
			idevice_cookiep->idev_softint = (u_long)soft;
		} else {
			/*
			 * The idevice cookie should contain the priority as
			 * understood by the device itself on the bus it
			 * lives on.  Let the nexi beneath sort out the
			 * translation (if any) that's needed.
			 */
			idevice_cookiep->idev_priority = (u_short)pri;
		}
	}

	return (DDI_SUCCESS);
}

/*
 * rootnex_remove_intrspec:
 *
 *	remove an interrupt specification.
 *
 */
/*ARGSUSED*/
static void
rootnex_remove_intrspec(dev_info_t *dip, dev_info_t *rdip,
	ddi_intrspec_t intrspec, ddi_iblock_cookie_t iblock_cookie)
{
	struct intrspec *ispec = (struct intrspec *)intrspec;

	if (ispec->intrspec_func == (u_int (*)()) 0) {
		return;
#ifdef XXX_later
	} else if (ispec->intrspec_func == (u_int (*)()) 1) {
		(void) settrap(rdip, ispec->intrspec_pri, NULL);
#endif
	} else {
		rem_avintr((void *)ispec, ispec->intrspec_pri,
		    ispec->intrspec_func, ispec->intrspec_vec);
	}
	ispec->intrspec_func = (u_int (*)()) 0;
}

/*
 * rootnex_map_fault:
 *
 *	fault in mappings for requestors
 */

/*ARGSUSED*/
static int
rootnex_map_fault(dev_info_t *dip, dev_info_t *rdip,
	struct hat *hat, struct seg *seg, caddr_t addr,
	struct devpage *dp, u_int pfn, u_int prot, u_int lock)
{
	extern struct seg kvseg;
	extern struct seg_ops segdev_ops;

#ifdef	DDI_MAP_DEBUG
	ddi_map_debug("rootnex_map_fault: address <%x> pfn <%x>", addr, pfn);
	ddi_map_debug(" Seg <%s>\n",
	    seg->s_ops == &segdev_ops ? "segdev" :
	    seg == &kvseg ? "segkmem" : "NONE!");
#endif	DDI_MAP_DEBUG

	/*
	 * This is all terribly broken, but it is a start
	 *
	 * XXX	Note that this test means that segdev_ops
	 *	must be exported from seg_dev.c.
	 * XXX	What about devices with their own segment drivers?
	 */
	if (seg->s_ops == &segdev_ops) {
		register struct segdev_data *sdp =
			(struct segdev_data *)seg->s_data;

		if (hat == NULL) {
			/*
			 * This is one plausible interpretation of
			 * a null hat i.e. use the first hat on the
			 * address space hat list which by convention is
			 * the hat of the system MMU.  At alternative
			 * would be to panic .. this might well be better ..
			 */
			ASSERT(AS_READ_HELD(seg->s_as, &seg->s_as->a_lock));
			hat = seg->s_as->a_hat;
			cmn_err(CE_NOTE, "rootnex_map_fault: nil hat");
		}
		hat_devload(hat, addr, MMU_PAGESIZE, pfn, prot | sdp->hat_flags,
		    (lock ? HAT_LOAD_LOCK : HAT_LOAD));
	} else if (seg == &kvseg && dp == (struct devpage *)0) {
		segkmem_mapin(seg, (caddr_t)addr, (u_int)MMU_PAGESIZE,
		    prot, pfn, 0);
	} else
		return (DDI_FAILURE);
	return (DDI_SUCCESS);
}


/*
 * DMA routines- for all 80x86 machines.
 */

/*
 * Shorthand defines
 */

#define	MAP	0
#define	BIND	1
#define	MAX_INT_BUF	(16*MMU_PAGESIZE)
#define	AHI		dma_lim->dlim_addr_hi
#define	OBJSIZE		dmareq->dmar_object.dmao_size
#define	OBJTYPE		dmareq->dmar_object.dmao_type
#define	DIRECTION	(hp->dmai_rflags & DDI_DMA_RDWR)

/* #define	DMADEBUG */
#if defined(DEBUG) || defined(lint)
#define	DMADEBUG
static int dmadebug = 0;
#define	DMAPRINT(a)	if (dmadebug) prom_printf a
#else
#define	DMAPRINT(a)	{ }
#endif	/* DEBUG */


/*
 * allocate DMA handle
 */
static int
rootnex_dma_allochdl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_attr_t *attr,
    int (*waitfp)(caddr_t), caddr_t arg, ddi_dma_handle_t *handlep)
{
	ddi_dma_impl_t *hp;

#ifdef lint
	dip = dip;
#endif

	if (attr->dma_attr_addr_hi <= attr->dma_attr_addr_lo) {
		return (DDI_DMA_BADATTR);
	}
	if ((attr->dma_attr_seg & MMU_PAGEOFFSET) != MMU_PAGEOFFSET ||
	    MMU_PAGESIZE & (attr->dma_attr_granular - 1) ||
	    attr->dma_attr_sgllen < 0) {
		return (DDI_DMA_BADATTR);
	}
	hp = (ddi_dma_impl_t *)kmem_zalloc(sizeof (*hp),
		(waitfp == DDI_DMA_SLEEP) ? KM_SLEEP : KM_NOSLEEP);
	if (hp == NULL) {
		if (waitfp != DDI_DMA_DONTWAIT) {
			ddi_set_callback(waitfp, arg, &dvma_call_list_id);
		}
		return (DDI_DMA_NORESOURCES);
	}

	/*
	 * Save requestor's information
	 */
	hp->dmai_wins = NULL;
	hp->dmai_kaddr =
	    hp->dmai_ibufp = NULL;
	hp->dmai_ibpadr = 0;
	hp->dmai_inuse = 0;
	hp->dmai_minxfer = attr->dma_attr_minxfer;
	hp->dmai_burstsizes = attr->dma_attr_burstsizes;
	hp->dmai_minfo = NULL;
	hp->dmai_rdip = rdip;
	hp->dmai_attr = *attr;
	*handlep = (ddi_dma_handle_t)hp;

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
rootnex_dma_freehdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle)
{
	ddi_dma_impl_t *hp = (ddi_dma_impl_t *)handle;

	kmem_free(hp, sizeof (*hp));
	if (dvma_call_list_id)
		ddi_run_callback(&dvma_call_list_id);
	return (DDI_SUCCESS);
}

#define	MAP	0
#define	BIND	1

static int
rootnex_dma_bindhdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, struct ddi_dma_req *dmareq,
    ddi_dma_cookie_t *cookiep, u_int *ccountp)
{
	ddi_dma_impl_t *hp = (ddi_dma_impl_t *)handle;
	ddi_dma_attr_t *dma_attr = &hp->dmai_attr;
	ddi_dma_cookie_t *cp;
	ddi_dma_lim_t lim, *limp;
	impl_dma_segment_t *segp;
	u_int segcount = 1;
	int rval;

	/*
	 * no mutex for speed
	 */
	if (hp->dmai_inuse) {
		return (DDI_DMA_INUSE);
	}
	hp->dmai_inuse = 1;

	/*
	 * for now
	 */

	limp = &lim;
	limp->dlim_addr_lo = (u_long)dma_attr->dma_attr_addr_lo;
	limp->dlim_addr_hi = (u_long)dma_attr->dma_attr_addr_hi;
	limp->dlim_minxfer = (u_int)dma_attr->dma_attr_minxfer;
	limp->dlim_dmaspeed = 0;
	limp->dlim_cntr_max = 0;
	limp->dlim_burstsizes = 1;
	limp->dlim_version = (u_int)DMALIM_VER0;
	limp->dlim_adreg_max = (u_int)dma_attr->dma_attr_seg;
	limp->dlim_ctreg_max = (u_int)dma_attr->dma_attr_count_max;
	limp->dlim_granular = (u_int)dma_attr->dma_attr_granular;
	limp->dlim_sgllen = (short)dma_attr->dma_attr_sgllen;
	limp->dlim_reqsize = (u_int)dma_attr->dma_attr_maxxfer;

	rval =  rootnex_io_brkup(dip, rdip, dmareq, &handle, limp, BIND);
	if (rval && (rval != DDI_DMA_PARTIAL_MAP)) {
		hp->dmai_inuse = 0;
		return (rval);
	}
	hp->dmai_wins = segp = hp->dmai_hds;
	if (hp->dmai_ibufp) {
		(void) rootnex_io_wtsync(hp, BIND);
	}

	while ((segp->dmais_flags & DMAIS_WINEND) == 0) {
		segp = segp->dmais_link;
		segcount++;
	}
	*ccountp = segcount;
	cp = hp->dmai_cookie;
	ASSERT(cp);
	cookiep->dmac_notused = cp->dmac_notused;
	cookiep->dmac_type = cp->dmac_type;
	cookiep->dmac_address = cp->dmac_address;
	cookiep->dmac_size = cp->dmac_size;
	hp->dmai_cookie++;

	return (rval);
}

/*ARGSUSED*/
static int
rootnex_dma_unbindhdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle)
{
	ddi_dma_impl_t *hp = (ddi_dma_impl_t *)handle;
	int rval = DDI_SUCCESS;

	if (hp->dmai_ibufp) {
		rval = rootnex_io_rdsync(hp);
		ddi_mem_free(hp->dmai_ibufp);
	}
	if (hp->dmai_kaddr)
		rmfree(kernelmap, (long)1,
		    mmu_btop(hp->dmai_kaddr - Sysbase));
	kmem_free((caddr_t)hp->dmai_segp, hp->dmai_kmsize);
	if (dvma_call_list_id)
		ddi_run_callback(&dvma_call_list_id);
	hp->dmai_inuse = 0;
	return (rval);
}

/*ARGSUSED*/
static int
rootnex_dma_flush(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, off_t off, u_int len,
    u_int cache_flags)
{
	ddi_dma_impl_t *hp = (ddi_dma_impl_t *)handle;

	if (hp->dmai_ibufp)
		return (rootnex_io_rdsync(hp));
	else
		return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
rootnex_dma_win(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, uint_t win, off_t *offp,
    uint_t *lenp, ddi_dma_cookie_t *cookiep, uint_t *ccountp)
{
	ddi_dma_impl_t *hp = (ddi_dma_impl_t *)handle;
	impl_dma_segment_t *segp, *winp = hp->dmai_hds;
	uint_t len, segcount = 1;
	ddi_dma_cookie_t *cp;
	int i;

	/*
	 * win is in the range [0 .. dmai_nwin-1]
	 */
	if (win >= hp->dmai_nwin) {
		return (DDI_FAILURE);
	}
	if (hp->dmai_wins && hp->dmai_ibufp) {
		(void) rootnex_io_rdsync(hp);
	}
	ASSERT(winp->dmais_flags & DMAIS_WINSTRT);
	for (i = 0; i < win; i++) {
		winp = winp->_win._dmais_nex;
		ASSERT(winp);
		ASSERT(winp->dmais_flags & DMAIS_WINSTRT);
	}

	hp->dmai_wins = (impl_dma_segment_t *)winp;
	if (hp->dmai_ibufp)
		(void) rootnex_io_wtsync(hp, BIND);
	segp = winp;
	len = segp->dmais_size;
	*offp = segp->dmais_ofst;
	while ((segp->dmais_flags & DMAIS_WINEND) == 0) {
		segp = segp->dmais_link;
		len += segp->dmais_size;
		segcount++;
	}

	*lenp = len;
	*ccountp = segcount;
	cp = hp->dmai_cookie = winp->dmais_cookie;
	ASSERT(cp);
	cookiep->dmac_notused = cp->dmac_notused;
	cookiep->dmac_type = cp->dmac_type;
	cookiep->dmac_address = cp->dmac_address;
	cookiep->dmac_size = cp->dmac_size;
	hp->dmai_cookie++;
	DMAPRINT(("getwin win %x mapping %x size %x\n",
	    (u_long)winp, (u_long)cp->dmac_address, cp->dmac_size));

	return (DDI_SUCCESS);
}

static int
rootnex_dma_map(dev_info_t *dip, dev_info_t *rdip,
    struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep)
{
	register ddi_dma_lim_t *dma_lim = dmareq->dmar_limits;
	register impl_dma_segment_t *segmentp;
	register ddi_dma_impl_t *hp;
	struct page **pplist;
	struct as *asp;
	caddr_t vadr;
	paddr_t padr;
	page_t *pp;
	u_int offset;
	int mapinfo;
	int sizehandle;

#ifdef lint
	dip = dip;
#endif

	DMAPRINT(("dma_map: %s (%s) reqp %x ", (handlep)? "alloc" : "advisory",
	    ddi_get_name(rdip), (u_long)dmareq));

#ifdef	DMADEBUG
	/*
	 * Validate range checks on DMA limits
	 */
	if ((dma_lim->dlim_adreg_max & MMU_PAGEOFFSET) != MMU_PAGEOFFSET ||
	    dma_lim->dlim_granular > MMU_PAGESIZE ||
	    dma_lim->dlim_sgllen <= 0) {
		DMAPRINT((" bad_limits\n"));
		return (DDI_DMA_BADLIMITS);
	}
#endif
	/*
	 * check if this I/O breakup request is trivial
	 */
	vadr = (OBJTYPE == DMA_OTYP_PAGES) ?
	    (caddr_t)dmareq->dmar_object.dmao_obj.pp_obj.pp_offset :
	    dmareq->dmar_object.dmao_obj.virt_obj.v_addr;

	pplist = NULL;

	if ((((u_int)vadr & MMU_PAGEMASK) ==
	    (((u_int)vadr + OBJSIZE - 1) & MMU_PAGEMASK)) &&
	    (((u_int)vadr & (dma_lim->dlim_minxfer - 1)) == 0) &&
	    handlep) {

		switch (OBJTYPE) {
		case DMA_OTYP_PAGES:
			pp = dmareq->dmar_object.dmao_obj.pp_obj.pp_pp;
			padr =
			    (u_long)vadr + (page_pptonum(pp) << MMU_PAGESHIFT);
			mapinfo = DMAMI_PAGES;
			DMAPRINT(("pagep %x size 0x%x", pp, OBJSIZE));
			break;
		case DMA_OTYP_VADDR:
			offset = (u_long)vadr & MMU_PAGEOFFSET;
			asp = dmareq->dmar_object.dmao_obj.virt_obj.v_as;
			if (asp == NULL)
				asp = &kas;
			pplist = dmareq->dmar_object.dmao_obj.virt_obj.v_priv;
			if (pplist == NULL) {
				if (asp != &kas) {
					padr = offset +
					    (ppcmmu_getpfnum(asp, vadr) <<
						MMU_PAGESHIFT);
					mapinfo = DMAMI_UVADR;
				} else {
					padr = offset +
					    (ppcmmu_getkpfnum(vadr) <<
						MMU_PAGESHIFT);
					mapinfo = DMAMI_KVADR;
				}
			} else {
				padr = offset +
				    (page_pptonum(*pplist) << MMU_PAGESHIFT);
				if (asp != &kas) {
					mapinfo = DMAMI_UVADR;
				} else {
					mapinfo = DMAMI_KVADR;
				}
			}
			DMAPRINT(("vadr %x size 0x%x", vadr, OBJSIZE));
			break;
		case DMA_OTYP_PADDR:
		default:
			/*
			 *  Not a supported type for this implementation
			 */
			return (DDI_DMA_NOMAPPING);
		}
		if (padr < AHI) {
			sizehandle = sizeof (ddi_dma_impl_t) +
			    sizeof (impl_dma_segment_t);

			hp = (ddi_dma_impl_t *)kmem_alloc(sizehandle,
			    (dmareq->dmar_fp == DDI_DMA_SLEEP) ?
			    KM_SLEEP : KM_NOSLEEP);
			if (!hp) {
				/* let other routine do callback */
				goto breakup_req;
			}
			hp->dmai_kmsize = sizehandle;

			/*
			 * locate segments after dma_impl handle structure
			 */
			segmentp = (impl_dma_segment_t *)(hp + 1);

			/*
			 * Save requestor's information
			 */
			hp->dmai_minxfer = dma_lim->dlim_minxfer;
			hp->dmai_burstsizes = dma_lim->dlim_burstsizes;
			hp->dmai_rdip = rdip;
			hp->dmai_cookie = NULL;
			hp->dmai_wins = NULL;
			hp->dmai_kaddr = hp->dmai_ibufp = NULL;
			hp->dmai_ibpadr = 0;
			hp->dmai_hds = segmentp;
			hp->dmai_rflags = dmareq->dmar_flags & DMP_DDIFLAGS;
			hp->dmai_minfo = (void *)mapinfo;
			hp->dmai_object = dmareq->dmar_object;

			if (mapinfo == DMAMI_PAGES) {
				segmentp->_vdmu._dmais_pp = pp;
				segmentp->dmais_ofst = (u_int)vadr;
			} else {
				segmentp->_vdmu._dmais_va = vadr;
				segmentp->dmais_ofst = 0;
			}
			segmentp->_win._dmais_nex = NULL;
			segmentp->dmais_link = NULL;
			segmentp->_pdmu._dmais_pd = padr;
			segmentp->dmais_size = OBJSIZE;
			segmentp->dmais_flags = DMAIS_WINSTRT | DMAIS_WINEND;
			segmentp->dmais_hndl = hp;
			*handlep = (ddi_dma_handle_t)hp;
			DMAPRINT(("	QUICKIE handle %x\n", hp));
			return (DDI_DMA_MAPPED);
		}
	}
breakup_req:
	return (rootnex_io_brkup(dip, rdip, dmareq, handlep, dma_lim, MAP));
}

int
rootnex_io_brkup(dev_info_t *dip, dev_info_t *rdip,
    struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep,
    ddi_dma_lim_t *dma_lim, int type)
{
	register impl_dma_segment_t *segmentp;
	impl_dma_segment_t *curwinp;
	impl_dma_segment_t *previousp;
	impl_dma_segment_t *prewinp;
	ddi_dma_cookie_t *cookiep;
	ddi_dma_impl_t *hp = 0;
	page_t *pp;
	caddr_t basevadr;
	caddr_t segmentvadr;
	paddr_t segmentpadr;
	struct as *asp;
	u_int maxsegmentsize, sizesegment;
	u_int needintbuf;
	u_int offset;
	u_int residual_size;
	u_int sglistsize;
	int nsegments;
	int mapinfo;
	int reqneedintbuf;
	int rval;
	int segment_flags, win_flags;
	int sgcount;
	int wcount;
#ifdef DMADEBUG
	int numsegments;
#endif
	struct page **pplist = NULL;

#ifdef lint
	dip = dip;
#endif

	/*
	 * Validate the dma request.
	 */
#ifdef DMADEBUG
	if (dma_lim->dlim_adreg_max < MMU_PAGEOFFSET ||
	    dma_lim->dlim_ctreg_max < MMU_PAGEOFFSET ||
	    dma_lim->dlim_granular > MMU_PAGESIZE ||
	    dma_lim->dlim_reqsize < MMU_PAGESIZE) {
		DMAPRINT((" bad_limits\n"));
		return (DDI_DMA_BADLIMITS);
	}
#endif

	residual_size = OBJSIZE;
	switch (OBJTYPE) {
	case DMA_OTYP_PAGES:
		pp = dmareq->dmar_object.dmao_obj.pp_obj.pp_pp;
		basevadr = 0;
		segmentvadr =
		    (caddr_t)dmareq->dmar_object.dmao_obj.pp_obj.pp_offset;
		offset = (int)segmentvadr;
		segmentpadr =
		    (u_long)offset + (page_pptonum(pp) << MMU_PAGESHIFT);
		mapinfo = DMAMI_PAGES;
		DMAPRINT(("pagep %x size 0x%x\n", pp, residual_size));
		break;
	case DMA_OTYP_VADDR:
		segmentvadr =
		    basevadr = dmareq->dmar_object.dmao_obj.virt_obj.v_addr;
		offset = (u_long)segmentvadr & MMU_PAGEOFFSET;
		asp = dmareq->dmar_object.dmao_obj.virt_obj.v_as;
		if (asp == NULL)
			asp = &kas;
		pplist = dmareq->dmar_object.dmao_obj.virt_obj.v_priv;
		if (pplist == NULL) {
			/*
			 * The corect way would be to call hat_ but for
			 * user addresses this requires the as readers lock
			 * to be held. Now we know that the segment is locked
			 * and the as space can't go away so we can make a short
			 * cut into the hat avoiding the locking and all the
			 * MT hat stuff.
			 */
			if (asp != &kas) {
				segmentpadr = offset + (ppcmmu_getpfnum(
				    asp, segmentvadr) << MMU_PAGESHIFT);
				mapinfo = DMAMI_UVADR;
			} else {
				segmentpadr = offset + (ppcmmu_getkpfnum(
				    segmentvadr) << MMU_PAGESHIFT);
				mapinfo = DMAMI_KVADR;
			}
		} else {
			segmentpadr = offset +
			    (page_pptonum(*pplist) << MMU_PAGESHIFT);
			if (asp != &kas) {
				mapinfo = DMAMI_UVADR;
			} else {
				mapinfo = DMAMI_KVADR;
			}
		}
		DMAPRINT(("vadr %x size 0x%x\n", segmentvadr, residual_size));
		break;
	case DMA_OTYP_PADDR:
	default:
		/*
		 *  Not a supported type for this implementation
		 */
		/* mapinfo = 0; */
		DMAPRINT(("bad_mem_type %x\n", OBJTYPE));
		rval = DDI_DMA_NOMAPPING;
		goto bad;
	}
	if (dma_lim->dlim_sgllen <= 0 ||
	    (offset & (dma_lim->dlim_minxfer - 1))) {
		DMAPRINT((" bad_limits/mapping\n"));
		rval = DDI_DMA_NOMAPPING;
		goto bad;
	}

	maxsegmentsize = umin(dma_lim->dlim_adreg_max,
	    umin((dma_lim->dlim_ctreg_max + 1) * dma_lim->dlim_minxfer,
	    dma_lim->dlim_reqsize) - 1) + 1;
	if (maxsegmentsize < MMU_PAGESIZE) {
		DMAPRINT((" bad_limits, maxsegmentsize\n"));
		rval = DDI_DMA_BADLIMITS;
		goto bad;
	}

	/*
	 * Estimate the number of xfer segments into which the memory
	 * object will be broken up.  Start with the number of memory
	 * pages that the object spans.  The total is the number of
	 * segments that will be allocated.
	 */
	nsegments = (offset + residual_size + MMU_PAGEOFFSET) >> MMU_PAGESHIFT;

	if (dma_lim->dlim_sgllen > 1) {
		/* scatter/gather list or buffer chaining */
		if (offset & (dma_lim->dlim_granular - 1)) {
			/*
			 * Object is not page or sector aligned,
			 * so we may need more segments
			 */
			nsegments += (nsegments + dma_lim->dlim_sgllen - 2) /
			    (dma_lim->dlim_sgllen - 1);

			if (dma_lim->dlim_adreg_max != 0xffffffff)
				nsegments += (offset + residual_size) /
				    (dma_lim->dlim_adreg_max + 1);
			if (AHI != 0xffffffff)
				nsegments += residual_size / MAX_INT_BUF;
		}
		if (dma_lim->dlim_reqsize != 0xffffffff)
			nsegments += residual_size / dma_lim->dlim_reqsize;
	} else {
		/* discrete operations */
		if ((offset & (dma_lim->dlim_granular - 1)) == 0 &&
		    dma_lim->dlim_adreg_max != 0xffffffff) {
			/*
			 * Object is sector aligned, so no
			 * intermediate buffer will be used
			 */
			nsegments += (offset + residual_size) /
			    (dma_lim->dlim_adreg_max + 1);
		}
	}
#ifdef DMADEBUG
	numsegments = nsegments;
#endif
	ASSERT(nsegments > 0);

	if (handlep) {
		int sizehandle;

		if (type == MAP) {
			sizehandle = sizeof (ddi_dma_impl_t) +
			    (nsegments * sizeof (impl_dma_segment_t));

			hp = (ddi_dma_impl_t *)kmem_alloc(sizehandle,
				(dmareq->dmar_fp == DDI_DMA_SLEEP) ?
				KM_SLEEP : KM_NOSLEEP);
			if (!hp) {
				rval = DDI_DMA_NORESOURCES;
				goto bad;
			}
			hp->dmai_kmsize = sizehandle;

			/*
			 * locate segments after dma_impl handle structure
			 */
			segmentp = (impl_dma_segment_t *)(hp + 1);

			/*
			 * Save requestor's information
			 */
			hp->dmai_minxfer = dma_lim->dlim_minxfer;
			hp->dmai_burstsizes = dma_lim->dlim_burstsizes;
			hp->dmai_rdip = rdip;
		} else {
			/*
			 * take handle passed from alloc
			 */
			hp = (ddi_dma_impl_t *)*handlep;
			sizehandle = (nsegments * sizeof (impl_dma_segment_t) +
			    nsegments * sizeof (ddi_dma_cookie_t));

			hp->dmai_segp = kmem_zalloc(sizehandle,
			    (dmareq->dmar_fp == DDI_DMA_SLEEP) ?
			    KM_SLEEP : KM_NOSLEEP);
			if (!hp->dmai_segp) {
				rval = DDI_DMA_NORESOURCES;
				goto bad;
			}
			hp->dmai_kmsize = sizehandle;
			segmentp = (impl_dma_segment_t *)hp->dmai_segp;
			cookiep = (ddi_dma_cookie_t *)(segmentp + nsegments);
			hp->dmai_cookie = cookiep;
		}
		hp->dmai_wins = NULL;
		hp->dmai_kaddr = hp->dmai_ibufp = NULL;
		hp->dmai_ibpadr = 0;
		hp->dmai_hds = prewinp = segmentp;
		hp->dmai_rflags = dmareq->dmar_flags & DMP_DDIFLAGS;
		hp->dmai_minfo = (void *)mapinfo;
		hp->dmai_object = dmareq->dmar_object;
	} else {
		/*
		 * Advisory call
		 * Just verify that the memory object is accessible
		 * by the DMA engine.
		 */
		if (nsegments > 1 || segmentpadr > AHI) {
			return (DDI_DMA_NOMAPPING);
		}
		return (DDI_DMA_MAPOK);
	}

	/*
	 * Breakup the memory object
	 * and build an i/o segment at each boundary condition
	 */
	curwinp = 0;
	needintbuf = 0;
	previousp = 0;
	reqneedintbuf = 0;
	sglistsize = 0;
	wcount = 0;
	sgcount = 1;
	do {
		sizesegment = umin((MMU_PAGESIZE - offset),
		    umin(residual_size, maxsegmentsize));
		segment_flags = (segmentpadr > AHI) ? DMAIS_NEEDINTBUF : 0;

		if (dma_lim->dlim_sgllen == 1) {
			/*
			 * _no_ scatter/gather capability,
			 * so ensure that size of each segment is a
			 * multiple of dlim_granular (== sector size)
			 */
			if ((segmentpadr & (dma_lim->dlim_granular - 1)) &&
			    residual_size != sizesegment) {
				/*
				 * this segment needs an intermediate buffer
				 */
				sizesegment = umin(MMU_PAGESIZE,
				    umin(residual_size, maxsegmentsize));
				segment_flags |= DMAIS_NEEDINTBUF;
			}
		}

		if (previousp &&
		    (previousp->_pdmu._dmais_pd + previousp->dmais_size) ==
		    segmentpadr &&
		    (previousp->dmais_flags &
		    (DMAIS_NEEDINTBUF | DMAIS_COMPLEMENT)) == 0 &&
		    (segment_flags & DMAIS_NEEDINTBUF) == 0 &&
		    (previousp->dmais_size + sizesegment) <= maxsegmentsize &&
		    (segmentpadr & dma_lim->dlim_adreg_max) &&
		    (sglistsize + sizesegment) <= dma_lim->dlim_reqsize) {
			/*
			 * combine new segment with previous segment
			 */
			previousp->dmais_flags |= segment_flags;
			previousp->dmais_size += sizesegment;
			if (type == BIND) {
				previousp->dmais_cookie->dmac_size +=
					sizesegment;
			}
			if ((sglistsize += sizesegment) ==
			    dma_lim->dlim_reqsize)
				/*
				 * force end of scatter/gather list
				 */
				sgcount = dma_lim->dlim_sgllen + 1;
		} else {
			/*
			 * add new segment to linked list
			 */
			if (previousp) {
				previousp->dmais_link = segmentp;
			}
			if (type == BIND) {
				segmentp->dmais_cookie = cookiep;
			}

			segmentp->dmais_hndl = hp;
			if (curwinp == 0) {
				prewinp->_win._dmais_nex =
				    curwinp = segmentp;
				segment_flags |= DMAIS_WINSTRT;
				win_flags = segment_flags;
				wcount++;
			} else {
				segmentp->_win._dmais_cur = curwinp;
				win_flags |= segment_flags;
			}
			segmentp->dmais_ofst = segmentvadr - basevadr;
			if (mapinfo == DMAMI_PAGES) {
				segmentp->_vdmu._dmais_pp = pp;
			} else {
				segmentp->_vdmu._dmais_va = segmentvadr;
			}
			segmentp->_pdmu._dmais_pd = segmentpadr;
			segmentp->dmais_flags = (ushort_t)segment_flags;

			if (dma_lim->dlim_sgllen > 1) {
				if (segment_flags & DMAIS_NEEDINTBUF) {
					needintbuf += sizesegment;
					if (needintbuf >= MAX_INT_BUF) {
						/*
						 * limit size of intermediate
						 * buffer
						 */
						sizesegment -=
						    (needintbuf - MAX_INT_BUF);
						reqneedintbuf = MAX_INT_BUF;
						needintbuf = 0;
						/*
						 * end of current window
						 */
						segmentp->dmais_flags |=
						    DMAIS_WINEND;
						prewinp = curwinp;
						curwinp->dmais_flags |=
						    DMAIS_WINUIB;
						curwinp = NULL;
						/*
						 * force end of scatter/gather
						 * list
						 */
						sgcount = dma_lim->dlim_sgllen;
					}
				}
				sglistsize += sizesegment;
				if (sglistsize >= dma_lim->dlim_reqsize) {
					/*
					 * limit size of xfer
					 */
					sizesegment -= (sglistsize -
					    dma_lim->dlim_reqsize);
					sglistsize = dma_lim->dlim_reqsize;
					sgcount = dma_lim->dlim_sgllen;
				}
				sgcount++;
			} else {
				/*
				 * _no_ scatter/gather capability,
				 */
				if (segment_flags & DMAIS_NEEDINTBUF) {
					/*
					 * end of window
					 */
					needintbuf =
					    max(sizesegment, needintbuf);
					segmentp->dmais_flags |= DMAIS_WINEND;
					prewinp = curwinp;
					curwinp->dmais_flags |= DMAIS_WINUIB;
					curwinp = NULL;
				}
			}
			segmentp->dmais_size = sizesegment;
			if (type == BIND) {
				cookiep->dmac_address = segmentpadr;
				cookiep->dmac_notused = 0;
				cookiep->dmac_type = (ulong_t)segmentp;
				cookiep->dmac_size = sizesegment;
				cookiep++;
			}
			previousp = segmentp++;
			--nsegments;
		}

		if (sgcount > dma_lim->dlim_sgllen) {
			/*
			 * end of a scatter/gather list!
			 * ensure that total length of list is a
			 * multiple of granular (sector size)
			 */
			if (sizesegment != residual_size) {
				u_int trim;

				trim = sglistsize &
				    (dma_lim->dlim_granular - 1);
				if (trim >= sizesegment) {
					cmn_err(CE_WARN,
					    "unable to reduce segment size\n");
					rval = DDI_DMA_NOMAPPING;
					goto bad;
				}
				previousp->dmais_size -= trim;
				sizesegment -= trim;
				/* start new scatter/gather list */
				sgcount = 1;
				sglistsize = 0;
			}
			previousp->dmais_flags |= DMAIS_COMPLEMENT;
		}
		if (residual_size -= sizesegment) {
			segmentvadr += sizesegment;
			switch (mapinfo) {
			case DMAMI_PAGES:
				offset += sizesegment;
				if (offset >= MMU_PAGESIZE) {
					offset &= MMU_PAGEOFFSET;
					pp = pp->p_next;
				}
				segmentpadr = offset +
				    (page_pptonum(pp) << MMU_PAGESHIFT);
				break;

			case DMAMI_UVADR:
				offset = (u_long)segmentvadr & MMU_PAGEOFFSET;
				if (pplist == NULL) {
					segmentpadr = (paddr_t)(offset +
					    (ppcmmu_getpfnum(asp, segmentvadr)
						<< MMU_PAGESHIFT));
				} else {
					u_long off;

					off = (u_long)basevadr & MMU_PAGEOFFSET;
					segmentpadr = (paddr_t)(offset +
					    (page_pptonum(pplist[btop(off +
					    (segmentvadr - basevadr))]) <<
						MMU_PAGESHIFT));
				}
				break;

			case DMAMI_KVADR:
				offset = (u_long)segmentvadr & MMU_PAGEOFFSET;
				if (pplist == NULL) {
					segmentpadr = (paddr_t)(offset +
					    (ppcmmu_getkpfnum(segmentvadr) <<
						MMU_PAGESHIFT));
				} else {
					u_long off;

					off = (u_long)basevadr & MMU_PAGEOFFSET;
					segmentpadr = (paddr_t)(offset +
					    (page_pptonum(pplist[btop(off +
					    (segmentvadr - basevadr))]) <<
						MMU_PAGESHIFT));
				}
				break;
			}
		}
	} while (residual_size && nsegments);
	ASSERT(residual_size == 0);

	previousp->dmais_link = NULL;
	previousp->dmais_flags |= DMAIS_WINEND;
	if (curwinp) {
		if (win_flags & DMAIS_NEEDINTBUF)
			curwinp->dmais_flags |= DMAIS_WINUIB;
		curwinp->_win._dmais_nex = NULL;
	} else
		prewinp->_win._dmais_nex = NULL;

	if ((needintbuf = max(needintbuf, reqneedintbuf)) != 0) {
		/*
		 *  allocate intermediate buffer
		 */
		if (i_ddi_mem_alloc(dip, dma_lim, needintbuf,
		    (dmareq->dmar_fp == DDI_DMA_SLEEP) ? 0x1 : 0, 1, 0,
		    &hp->dmai_ibufp, (u_int *)&hp->dmai_ibfsz,
		    NULL) != DDI_SUCCESS) {
			rval = DDI_DMA_NORESOURCES;
			goto bad;
		}
		hp->dmai_ibpadr = (hat_getkpfnum(hp->dmai_ibufp) <<
		    MMU_PAGESHIFT) + ((u_long)hp->dmai_ibufp & MMU_PAGEOFFSET);

		if (mapinfo != DMAMI_KVADR) {
			long akva;

			/*
			 * allocate kernel virtual space for remapping.
			 */
			mutex_enter(&maplock(kernelmap));
			/* XXX check (dmareq->dmar_fp == DDI_DMA_SLEEP) */
			while ((akva = (long)rmalloc_locked(kernelmap,
			    1)) == 0) {
				mapwant(kernelmap) = 1;
				cv_wait(&map_cv(kernelmap),
				    &maplock(kernelmap));
			}
			mutex_exit(&maplock(kernelmap));
			hp->dmai_kaddr = kmxtob(akva);
		}
	}

	/*
	 * return success
	 */
	if (type == MAP) {
#ifdef DMADEBUG
		DMAPRINT(("dma_brkup: handle %x nsegments %x \n",
		    hp, numsegments - nsegments));
#endif
		hp->dmai_cookie = NULL;
		*handlep = (ddi_dma_handle_t)hp;
		return (DDI_DMA_MAPPED);
	} else {
		ASSERT(wcount > 0);
		if (wcount == 1) {
			hp->dmai_rflags &= ~DDI_DMA_PARTIAL;
			rval = DDI_DMA_MAPPED;
		} else if (hp->dmai_rflags & DDI_DMA_PARTIAL) {
			rval = DDI_DMA_PARTIAL_MAP;
		} else {
			if (hp->dmai_segp)
				kmem_free((caddr_t)hp->dmai_segp,
				    hp->dmai_kmsize);
			return (DDI_DMA_TOOBIG);
		}
		hp->dmai_nwin = wcount;
		return (rval);
	}
bad:
	if (type == MAP) {
		if (hp)
			kmem_free((caddr_t)hp, hp->dmai_kmsize);
	} else {
		hp->dmai_cookie = NULL;
		if (hp->dmai_segp)
			kmem_free((caddr_t)hp->dmai_segp, hp->dmai_kmsize);
	}
	if (rval == DDI_DMA_NORESOURCES && dmareq->dmar_fp != DDI_DMA_DONTWAIT)
		ddi_set_callback(dmareq->dmar_fp, dmareq->dmar_arg,
		    &dvma_call_list_id);
	return (rval);
}


int
rootnex_io_wtsync(ddi_dma_impl_t *hp, int type)
{
	impl_dma_segment_t *sp = hp->dmai_wins;
	caddr_t	kviradr;
	caddr_t vsrc;
	u_long iboffset = 0;
	u_long segoffset, vsoffset;
	int cpycnt;

	if ((sp->dmais_flags & DMAIS_WINUIB) == 0)
		return (DDI_SUCCESS);

	switch ((int)hp->dmai_minfo) {

	case DMAMI_KVADR:
		do if (sp->dmais_flags & DMAIS_NEEDINTBUF) {
			/*
			 * for each segment that requires an intermediate buffer
			 * allocate it now
			 */
			ASSERT((hp->dmai_ibfsz - iboffset) >= sp->dmais_size);

			if (DIRECTION == DDI_DMA_WRITE)
				/*
				 *  copy from segment to buffer
				 */
				bcopy(sp->_vdmu._dmais_va,
				    hp->dmai_ibufp + iboffset,
				    sp->dmais_size);
			/*
			 * save phys addr of intemediate buffer
			 */
			sp->_pdmu._dmais_pd = hp->dmai_ibpadr + iboffset;
			if (type == BIND) {
				sp->dmais_cookie->dmac_address =
					sp->_pdmu._dmais_pd;
			}
			iboffset += sp->dmais_size;
		} while (!(sp->dmais_flags & DMAIS_WINEND) &&
		    (sp = sp->dmais_link));
		break;

	case DMAMI_PAGES:
		do if (sp->dmais_flags & DMAIS_NEEDINTBUF) {
			/*
			 * for each segment that requires an intermediate buffer
			 * allocate it now
			 */
			ASSERT((hp->dmai_ibfsz - iboffset) >= sp->dmais_size);

			if (DIRECTION == DDI_DMA_WRITE) {
				/*
				 * need to mapin page so we can have a
				 * virtual address to do copying
				 */
				ppc_pp_map(sp->_vdmu._dmais_pp, hp->dmai_kaddr);
				/*
				 *  copy from segment to buffer
				 */
				bcopy(hp->dmai_kaddr +
				    (sp->dmais_ofst & MMU_PAGEOFFSET),
				    hp->dmai_ibufp + iboffset, sp->dmais_size);
				/*
				 *  need to mapout page
				 */
				hat_unload(kas.a_hat, hp->dmai_kaddr,
						MMU_PAGESIZE, HAT_UNLOAD);
			}
			/*
			 * save phys addr of intemediate buffer
			 */
			sp->_pdmu._dmais_pd = hp->dmai_ibpadr + iboffset;
			if (type == BIND) {
				sp->dmais_cookie->dmac_address =
					sp->_pdmu._dmais_pd;
			}
			iboffset += sp->dmais_size;
		} while (!(sp->dmais_flags & DMAIS_WINEND) &&
		    (sp = sp->dmais_link));
		break;

	case DMAMI_UVADR:
		do if (sp->dmais_flags & DMAIS_NEEDINTBUF) {
			/*
			 * for each segment that requires an intermediate buffer
			 * allocate it now
			 */
			ASSERT((hp->dmai_ibfsz - iboffset) >= sp->dmais_size);

			if (DIRECTION == DDI_DMA_WRITE) {
				struct page **pplist;
				segoffset = 0;
				do {
					/*
					 * need to mapin page so we can have a
					 * virtual address to do copying
					 */
					vsrc = sp->_vdmu._dmais_va + segoffset;
					vsoffset =
					    (u_long)vsrc & MMU_PAGEOFFSET;
					pplist = hp->dmai_object.dmao_obj.
							virt_obj.v_priv;
					/*
					 * check if we have to use the
					 * shadow list or the CPU mapping.
					 */
					if (pplist != NULL) {
						u_long base, off;

						base = (u_long)hp->dmai_object.
						    dmao_obj.virt_obj.v_addr;
						off = (base & MMU_PAGEOFFSET) +
							(u_long)vsrc - base;
						ppc_pp_map(pplist[btop(off)],
							hp->dmai_kaddr);
					} else {
						ppc_va_map(vsrc,
						    hp->dmai_object.dmao_obj.
							virt_obj.v_as,
						    hp->dmai_kaddr);
					}
					kviradr = hp->dmai_kaddr + vsoffset;
					cpycnt = sp->dmais_size - segoffset;
					if (vsoffset + cpycnt > MMU_PAGESIZE)
						cpycnt = MMU_PAGESIZE -
						    vsoffset;
					/*
					 *  copy from segment to buffer
					 */
					bcopy(kviradr,
					    hp->dmai_ibufp + iboffset +
						segoffset,
					    cpycnt);
					/*
					 *  need to mapout page
					 */
					hat_unload(kas.a_hat, hp->dmai_kaddr,
					    MMU_PAGESIZE, HAT_UNLOAD);
					segoffset += cpycnt;
				} while (segoffset < sp->dmais_size);
			}
			/*
			 * save phys addr of intermediate buffer
			 */
			sp->_pdmu._dmais_pd = hp->dmai_ibpadr + iboffset;
			if (type == BIND) {
				sp->dmais_cookie->dmac_address =
					sp->_pdmu._dmais_pd;
			}
			iboffset += sp->dmais_size;
		} while (!(sp->dmais_flags & DMAIS_WINEND) &&
		    (sp = sp->dmais_link));
		break;

	default:
		cmn_err(CE_WARN, "Invalid dma handle/map info\n");
	}

	/*
	 * We need to guarantee that all stores are visible to devices.
	 * eieio() defines a "store barrier" such that all stores done
	 * prior to eieio() are visible to those done afterwards.
	 */
	eieio();

	return (DDI_SUCCESS);
}

int
rootnex_io_rdsync(ddi_dma_impl_t *hp)
{
	impl_dma_segment_t *sp = hp->dmai_wins;
	caddr_t	kviradr;
	caddr_t vdest;
	u_long irdoffset = 0;
	u_long segoffset, vdoffset;
	int cpycnt;

	if (!(sp->dmais_flags & DMAIS_WINUIB) || DIRECTION != DDI_DMA_READ)
		return (DDI_SUCCESS);

	switch ((int)hp->dmai_minfo) {

	case DMAMI_KVADR:
		do if (sp->dmais_flags & DMAIS_NEEDINTBUF) {
			/*
			 * we already have a kernel address
			 */
			ASSERT(irdoffset == sp->_pdmu._dmais_pd -
			    hp->dmai_ibpadr);
			/*
			 *  copy from buffer to segment
			 */
			bcopy(hp->dmai_ibufp + irdoffset, sp->_vdmu._dmais_va,
			    sp->dmais_size);
			irdoffset += sp->dmais_size;
		} while (!(sp->dmais_flags & DMAIS_WINEND) &&
		    (sp = sp->dmais_link));
		break;

	case DMAMI_PAGES:
		do if (sp->dmais_flags & DMAIS_NEEDINTBUF) {
			ASSERT(irdoffset == sp->_pdmu._dmais_pd -
			    hp->dmai_ibpadr);
			/*
			 * need to mapin page
			 */
			ppc_pp_map(sp->_vdmu._dmais_pp, hp->dmai_kaddr);
			/*
			 *  copy from buffer to segment
			 */
			bcopy(hp->dmai_ibufp + irdoffset,
			    (hp->dmai_kaddr +
				(sp->dmais_ofst & MMU_PAGEOFFSET)),
			    sp->dmais_size);

			/*
			 *  need to mapout page
			 */
			hat_unload(kas.a_hat, hp->dmai_kaddr, MMU_PAGESIZE,
			    HAT_UNLOAD);
			irdoffset += sp->dmais_size;
		} while (!(sp->dmais_flags & DMAIS_WINEND) &&
		    (sp = sp->dmais_link));
		break;

	case DMAMI_UVADR:
		do if (sp->dmais_flags & DMAIS_NEEDINTBUF) {
			struct page **pplist;
			segoffset = 0;
			ASSERT(irdoffset == sp->_pdmu._dmais_pd -
			    hp->dmai_ibpadr);
			do {
				/*
				 * need to map_in user virtual address
				 */
				vdest = sp->_vdmu._dmais_va + segoffset;
				vdoffset = (u_long)vdest & MMU_PAGEOFFSET;
				pplist = hp->dmai_object.dmao_obj.
						virt_obj.v_priv;
				/*
				 * check if we have to use the
				 * shadow list or the CPU mapping.
				 */
				if (pplist != NULL) {
					u_long base, off;

					base = (u_long)hp->dmai_object.
						dmao_obj.virt_obj.v_addr;
					off = (base & MMU_PAGEOFFSET) +
						(u_long)vdest - base;
					ppc_pp_map(pplist[btop(off)],
						hp->dmai_kaddr);
				} else {
					ppc_va_map(vdest,
					    hp->dmai_object.dmao_obj.
						virt_obj.v_as,
					    hp->dmai_kaddr);
				}
				kviradr = hp->dmai_kaddr + vdoffset;
				cpycnt = sp->dmais_size - segoffset;
				if (vdoffset + cpycnt > MMU_PAGESIZE)
					cpycnt = MMU_PAGESIZE - vdoffset;
				/*
				 *  copy from buffer to segment
				 */
				bcopy(hp->dmai_ibufp + irdoffset + segoffset,
				    kviradr, cpycnt);
				/*
				 *  need to map_out page
				 */
				hat_unload(kas.a_hat, hp->dmai_kaddr,
						MMU_PAGESIZE, HAT_UNLOAD);
				segoffset += cpycnt;
			} while (segoffset < sp->dmais_size);
			irdoffset += sp->dmais_size;
		} while (!(sp->dmais_flags & DMAIS_WINEND) &&
		    (sp = sp->dmais_link));
		break;

	default:
		cmn_err(CE_WARN, "Invalid dma handle/map info\n");
	}
	return (DDI_SUCCESS);
}

static int
rootnex_dma_mctl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, enum ddi_dma_ctlops request,
    off_t *offp, u_int *lenp,
    caddr_t *objpp, u_int cache_flags)
{
	register ddi_dma_impl_t *hp = (ddi_dma_impl_t *)handle;
	impl_dma_segment_t *sp = (impl_dma_segment_t *)lenp;
	impl_dma_segment_t *wp = (impl_dma_segment_t *)offp;
	ddi_dma_cookie_t *cp;
	int rval = DDI_SUCCESS;

#ifdef lint
	dip = dip;
	rdip = rdip;
#endif

	DMAPRINT(("io_mctl: handle %x ", hp));

	switch (request) {

	case DDI_DMA_SEGTOC:
		/* return device specific dma cookie for segment */
		sp = (impl_dma_segment_t *)cache_flags;
		if (!sp) {
			DMAPRINT(("stoc segment %x end\n", (u_long)sp));
			return (DDI_FAILURE);
		}
		cp = (ddi_dma_cookie_t *)objpp;

		cp->dmac_address = sp->_pdmu._dmais_pd;

		DMAPRINT(("stoc segment %x mapping %x size %x\n",
		    (u_long)sp, (u_long)sp->_vdmu._dmais_va, sp->dmais_size));

		cp->dmac_notused = 0;
		cp->dmac_type = (ulong_t)sp;
		*lenp = cp->dmac_size = sp->dmais_size;
		*offp = sp->dmais_ofst;
		return (DDI_SUCCESS);

	case DDI_DMA_NEXTSEG:	/* get next DMA segment	*/
		ASSERT(wp->dmais_flags & DMAIS_WINSTRT);
		if (wp != hp->dmai_wins) {
			DMAPRINT(("nxseg: not current window %x\n",
			    (u_long)wp));
			return (DDI_DMA_STALE);
		}
		if (!sp) {
			/*
			 * reset to first segment in current window
			 */
			*objpp = (caddr_t)wp;
		} else {
			if (sp->dmais_flags & DMAIS_WINEND) {
				DMAPRINT(("nxseg: seg %x eow\n", (u_long)sp));
				return (DDI_DMA_DONE);
			}
			/* check if segment is really in window */
			ASSERT((sp->dmais_flags & DMAIS_WINSTRT) && sp == wp ||
			    !(sp->dmais_flags & DMAIS_WINSTRT) &&
			    sp->_win._dmais_cur == wp);
			*objpp = (caddr_t)sp->dmais_link;
		}
		DMAPRINT(("nxseg: new seg %x\n", (u_long)*objpp));
		return (DDI_SUCCESS);

	case DDI_DMA_NEXTWIN:	/* get next DMA window	*/
		if (hp->dmai_wins && hp->dmai_ibufp)
			/*
			 * do implied sync on current window
			 */
			(void) rootnex_io_rdsync(hp);
		if (!wp) {
			/*
			 * reset to (first segment of) first window
			 */
			*objpp = (caddr_t)hp->dmai_hds;
			DMAPRINT(("nxwin: first win %x\n", (u_long)*objpp));
		} else {
			ASSERT(wp->dmais_flags & DMAIS_WINSTRT);
			if (wp != hp->dmai_wins) {
				DMAPRINT(("nxwin: win %x not current\n",
				    (u_long)wp));
				return (DDI_DMA_STALE);
			}
			if (wp->_win._dmais_nex == 0) {
				DMAPRINT(("nxwin: win %x end\n", (u_long)wp));
				return (DDI_DMA_DONE);
			}
			*objpp = (caddr_t)wp->_win._dmais_nex;
			DMAPRINT(("nxwin: new win %x\n", (u_long)*objpp));
		}
		hp->dmai_wins = (impl_dma_segment_t *)*objpp;
		if (hp->dmai_ibufp)
			return (rootnex_io_wtsync(hp, MAP));
		return (DDI_SUCCESS);

	case DDI_DMA_FREE:
		DMAPRINT(("free handle\n"));
		if (hp->dmai_ibufp) {
			rval = rootnex_io_rdsync(hp);
			ddi_mem_free(hp->dmai_ibufp);
		}
		if (hp->dmai_kaddr)
			rmfree(kernelmap, (long)1, btokmx(hp->dmai_kaddr));
		kmem_free((caddr_t)hp, hp->dmai_kmsize);
		if (dvma_call_list_id)
			ddi_run_callback(&dvma_call_list_id);
		break;

	case DDI_DMA_IOPB_ALLOC:	/* get contiguous DMA-able memory */
		DMAPRINT(("iopb alloc\n"));
		rval = i_ddi_mem_alloc(rdip, (ddi_dma_lim_t *)offp, (u_int)lenp,
		    0, 0, 0, objpp, (u_int *)0, NULL);
		break;

	case DDI_DMA_SMEM_ALLOC:	/* get contiguous DMA-able memory */
		DMAPRINT(("mem alloc\n"));
		rval = i_ddi_mem_alloc(rdip, (ddi_dma_lim_t *)offp, (u_int)lenp,
		    cache_flags, 1, 0, objpp, (u_int *)handle, NULL);
		break;

	case DDI_DMA_SYNC:
		DMAPRINT(("sync\n"));
		if (hp->dmai_ibufp) {
			if (cache_flags == DDI_DMA_SYNC_FORDEV)
				rval = rootnex_io_wtsync(hp, MAP);
			else
				rval = rootnex_io_rdsync(hp);
		}
		break;

	case DDI_DMA_KVADDR:
		DMAPRINT(("kvaddr of phys mapping\n"));
		return (DDI_FAILURE);

	case DDI_DMA_GETERR:
		DMAPRINT(("geterr\n"));
		rval = DDI_FAILURE;
		break;

	case DDI_DMA_COFF:
		DMAPRINT(("coff off %x mapping %x size %x\n",
		    (u_long)*objpp, hp->dmai_wins->_pdmu._dmais_pd,
		    hp->dmai_wins->dmais_size));
		rval = DDI_FAILURE;
		break;

	default:
		DMAPRINT(("unknown 0%x\n", request));
		return (DDI_FAILURE);
	}
	return (rval);
}

/*
 * Root nexus ctl functions
 */

static int
rootnex_ctl_reportdev(dev_info_t *dev)
{
	register int i, n;

	cmn_err(CE_CONT, "?%s%d at root", DEVI(dev)->devi_name,
	    DEVI(dev)->devi_instance);

	for (i = 0; i < sparc_pd_getnreg(dev); i++) {

		register struct regspec *rp = sparc_pd_getreg(dev, i);

		if (i == 0)
			cmn_err(CE_CONT, "?: ");
		else
			cmn_err(CE_CONT, "? and ");

#ifdef XXXPPC_later
		switch (rp->regspec_bustype) {

		case BTEISA:
			name = DEVI_EISA_NEXNAME;
			break;

		case BTISA:
			name = DEVI_ISA_NEXNAME;
			break;

		default:
			cmn_err(CE_CONT, "?space %x offset %x",
			    rp->regspec_bustype, rp->regspec_addr);
			continue;
		}
		cmn_err(CE_CONT, "?%s 0x%x", name, rp->regspec_addr);
#else
		cmn_err(CE_CONT, "?space %x offset %x",
		    rp->regspec_bustype, rp->regspec_addr);
#endif
	}
	for (i = 0, n = sparc_pd_getnintr(dev); i < n; i++) {
		register int pri;

		if (i == 0)
			cmn_err(CE_CONT, "? ");
		else
			cmn_err(CE_CONT, "?, ");
		pri = INT_IPL(sparc_pd_getintr(dev, i)->intrspec_pri);
		cmn_err(CE_CONT, "?sparc ipl %d", pri);
	}
	cmn_err(CE_CONT, "?\n");
	return (DDI_SUCCESS);
}

/*
 * For the prep rootnexus, we're prepared to claim that the interrupt string
 * is in the form of a list of <ipl,vec> specifications.
 */
#define	VEC_MIN	1
#define	VEC_MAX	255
static int
rootnex_xlate_intrs(dev_info_t *dip, dev_info_t *rdip, int *in,
	struct ddi_parent_private_data *pdptr)
{
	register size_t size;
	register int n;
	register struct intrspec *new;

	static char bad_intr_fmt[] =
	    "rootnex: bad interrupt spec from %s%d - ipl %d, irq %d\n";

#ifdef	lint
	dip = dip;
#endif

	/*
	 * The list consists of <ipl,vec> elements
	 */
	if ((n = (*in++ >> 1)) < 1)
		return (DDI_FAILURE);

	pdptr->par_nintr = n;
	size = n * sizeof (struct intrspec);
	new = pdptr->par_intr = kmem_zalloc(size, KM_SLEEP);

	while (n--) {
		register int level = *in++;
		register int vec = *in++;

		if (level < 1 || level > MAXIPL ||
		    vec < VEC_MIN || vec > VEC_MAX) {
			cmn_err(CE_CONT, bad_intr_fmt,
			    DEVI(rdip)->devi_name,
			    DEVI(rdip)->devi_instance, level, vec);
			goto broken;
			/*NOTREACHED*/
		}
		new->intrspec_pri = level;
		if (vec != 2)
			new->intrspec_vec = vec;
		else
			/*
			 * irq 2 on the PC bus is tied to irq 9
			 * on ISA, EISA and MicroChannel
			 */
			new->intrspec_vec = 9;
		new++;
	}

	return (DDI_SUCCESS);
	/*NOTREACHED*/

broken:
	kmem_free(pdptr->par_intr, size);
	pdptr->par_intr = (void *)0;
	pdptr->par_nintr = 0;
	return (DDI_FAILURE);
}

static int
rootnex_merge_pseudo(dev_info_t *parent, dev_info_t *child)
{
	dev_info_t *och;
	int found_hw_node;
	int need_init;
	major_t major;
	extern struct devnames *devnamesp;

	/*
	 * Find all previously defined h/w children with the same name
	 * and same parent and copy the property lists from the
	 * prototype node into the h/w nodes and re-inititialize them.
	 */
	major = ddi_name_to_major(ddi_get_name(child));
	if (major == (major_t)-1)
		return (DDI_FAILURE);

	found_hw_node = 0;
	for (och = devnamesp[major].dn_head; (och != NULL) && (och != child);
		och = ddi_get_next(och)) {

		if ((ddi_get_parent(och) != parent) ||
			(ddi_get_nodeid(och) == DEVI_PSEUDO_NODEID) ||
			DEVI(och)->devi_sys_prop_ptr ||
			DEVI(och)->devi_drv_prop_ptr || DDI_CF2(och))
			continue;

		found_hw_node++;
		need_init = DDI_CF1(och) ? 1 : 0;
		if (need_init)
			(void) ddi_uninitchild(och);
		copy_prop(DEVI(child)->devi_drv_prop_ptr,
		    &(DEVI(och)->devi_drv_prop_ptr));
		copy_prop(DEVI(child)->devi_sys_prop_ptr,
		    &(DEVI(och)->devi_sys_prop_ptr));
		if (need_init)
			(void) ddi_initchild(parent, och);
	}

	if (found_hw_node) {
		impl_ddi_sunbus_removechild(child);
		return (DDI_SUCCESS);
	} else
		return (DDI_FAILURE);
}

/*ARGSUSED*/
static int
rootnex_ctl_children(dev_info_t *dip, dev_info_t *rdip, ddi_ctl_enum_t ctlop,
    dev_info_t *child)
{
	extern int impl_ddi_sunbus_initchild(dev_info_t *);
	extern void impl_ddi_sunbus_removechild(dev_info_t *);

	switch (ctlop)  {

	case DDI_CTLOPS_INITCHILD:
		if (ddi_get_nodeid(child) == DEVI_PSEUDO_NODEID) {
			if (rootnex_merge_pseudo(dip, child) == DDI_SUCCESS) {
				return (DDI_NOT_WELL_FORMED);
			}
		}
		return (impl_ddi_sunbus_initchild(child));

	case DDI_CTLOPS_UNINITCHILD:
		impl_ddi_sunbus_removechild(child);
		return (DDI_SUCCESS);
	}

	return (DDI_FAILURE);
}


static int
rootnex_ctlops(dev_info_t *dip, dev_info_t *rdip,
    ddi_ctl_enum_t ctlop, void *arg, void *result)
{
	int n, *ptr;
	struct ddi_parent_private_data *pdp;

	switch (ctlop) {
	case DDI_CTLOPS_DMAPMAPC:
		/*
		 * Return 'partial' to indicate that dma mapping
		 * has to be done in the main MMU.
		 */
		return (DDI_DMA_PARTIAL);

	case DDI_CTLOPS_BTOP:
		/*
		 * Convert byte count input to physical page units.
		 * (byte counts that are not a page-size multiple
		 * are rounded down)
		 */
		*(u_long *)result = btop(*(u_long *)arg);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_PTOB:
		/*
		 * Convert size in physical pages to bytes
		 */
		*(u_long *)result = ptob(*(u_long *)arg);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_BTOPR:
		/*
		 * Convert byte count input to physical page units
		 * (byte counts that are not a page-size multiple
		 * are rounded up)
		 */
		*(u_long *)result = btopr(*(u_long *)arg);
		return (DDI_SUCCESS);

	/*
	 * XXX	This pokefault_mutex clutter needs to be done differently.
	 *	Note that i_ddi_poke() calls this routine in the order
	 *	INIT then optionally FLUSH then always FINI.
	 */
	case DDI_CTLOPS_POKE_INIT:
		mutex_enter(&pokefault_mutex);
		pokefault = -1;
		return (DDI_SUCCESS);

	case DDI_CTLOPS_POKE_FLUSH:
		return (DDI_FAILURE);

	case DDI_CTLOPS_POKE_FINI:
		pokefault = 0;
		mutex_exit(&pokefault_mutex);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_INITCHILD:
	case DDI_CTLOPS_UNINITCHILD:
		return (rootnex_ctl_children(dip, rdip, ctlop, arg));

	case DDI_CTLOPS_REPORTDEV:
		return (rootnex_ctl_reportdev(rdip));

	case DDI_CTLOPS_IOMIN:
		/*
		 * Nothing to do here but reflect back..
		 */
		return (DDI_SUCCESS);

	case DDI_CTLOPS_REGSIZE:
	case DDI_CTLOPS_NREGS:
	case DDI_CTLOPS_NINTRS:
		break;

	case DDI_CTLOPS_SIDDEV:
		/*
		 * Oh, a hack...
		 */
		if (ddi_get_nodeid(rdip) != DEVI_PSEUDO_NODEID)
			return (DDI_SUCCESS);
		else
			return (DDI_FAILURE);

	case DDI_CTLOPS_INTR_HILEVEL:
		/*
		 * Indicate whether the interrupt specified is to be handled
		 * above lock level.  In other words, above the level that
		 * cv_signal and default type mutexes can be used.
		 */
		*(int *)result =
		    (INT_IPL(((struct intrspec *)arg)->intrspec_pri)
		    > LOCK_LEVEL);
		return (DDI_SUCCESS);

	case DDI_CTLOPS_XLATE_INTRS:
		return (rootnex_xlate_intrs(dip, rdip, arg, result));

	case DDI_CTLOPS_POWER:
	{
		int		(*pwr_fn)(power_req *);
		int		ret;
		power_req	*req = (power_req *)arg;

		if ((ret = ddi_prop_get_int(DDI_DEV_T_ANY, dip,
		    DDI_PROP_DONTPASS, "platform-pm", -1)) == -1) {
			return (DDI_FAILURE);
		}
		pwr_fn = (int (*)())ret;
		return ((*pwr_fn)(req));
	}

	default:
		return (DDI_FAILURE);
	}
	/*
	 * The rest are for "hardware" properties
	 */

	pdp = (struct ddi_parent_private_data *)
	    (DEVI(rdip))->devi_parent_data;

	if (!pdp) {
		return (DDI_FAILURE);
	} else if (ctlop == DDI_CTLOPS_NREGS) {
		ptr = (int *)result;
		*ptr = pdp->par_nreg;
	} else if (ctlop == DDI_CTLOPS_NINTRS) {
		ptr = (int *)result;
		*ptr = pdp->par_nintr;
	} else {
		off_t *size = (off_t *)result;

		ptr = (int *)arg;
		n = *ptr;
		if (n > pdp->par_nreg) {
			return (DDI_FAILURE);
		}
		*size = (off_t)pdp->par_reg[n].regspec_size;
	}
	return (DDI_SUCCESS);
}
