/*
 * Copyrighted as an unpublished work.
 * (c) Copyright 1987,1988 Sun Microsystems, Inc.
 * All rights reserved.
 *
 * RESTRICTED RIGHTS
 *
 * These programs are supplied under a license.  They may be used,
 * disclosed, and/or copied only as permitted under such license
 * agreement.  Any copy must contain the above copyright notice and
 * this restricted rights notice.  Use, copying, and/or disclosure
 * of the programs is strictly prohibited unless otherwise provided
 * in the license agreement.
 */

#pragma ident "@(#)vdi.c	1.11	96/06/24 SMI"

#ifdef _VPIX

/*  IBM PC AT VP/ix Device Interface */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/cred.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/tss.h>
#include <sys/inline.h>
#include <sys/v86.h>
#include <sys/vdi.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/obpdefs.h>
#include <vm/hat_i86.h>

#define	ENABLE		0	/* add an entry */
#define	DISABLE		1	/* delete an entry */
#define	CHECK		2	/* check the existance of an entry */

/* indicates which devices are currently in use */
dev_t vdi_dev_used[VDMAXVDEVS] =
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

#ifdef VDCHECK
/* indicates which memory areas are currently in use */
struct vdi_used vdi_io_used[VDUSELEN];

/* indicates which io registers are currently in use */
struct vdi_used vdi_mem_used[VDUSELEN];
#endif /* VDCHECK */

/*
 * State of each DDA device.
 */

typedef struct {
	kthread_id_t	vdi_owner;	/* ptr to v86 thread controlling this */
					/* device */
	kthread_id_t	vdi_stash;
	int		vdi_inum;	/* interrupt vector number */
	int		vdi_ipri;	/* interrupt priority level */
	dev_info_t 	vdi_devi;	/* devinfo handle */
	int 		vdi_pseudonum;	/* psuedo interrupt mask to be sent */
	ushort		vdi_flag;	/* flags for this device */
	ddi_iblock_cookie_t vdi_iblock;	/* devinfo pointer */
	kmutex_t	vdi_lock;	/* mutex lock */
} vdi_devstate_t;

/*
 * An opaque handle where our set of DDA devices lives.
 */
static void *vdi_state;

static kmutex_t vdi_lock;

static int vdi_open(dev_t *devp, int flag, int otyp, cred_t *cred);
static int vdi_close(dev_t dev, int flag, int otyp, cred_t *cred);
static int vdi_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
    void **result);
static int vdi_identify(dev_info_t *devi);
static int vdi_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);
static int vdi_ioctl(dev_t, int, int, int, cred_t *, int *);
static u_int vdi_intr(caddr_t);

extern nodev(), nulldev();

struct cb_ops	vdi_cb_ops = {
	vdi_open,		/* open */
	vdi_close,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	vdi_ioctl,		/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* cb_prop_op */
	0,			/* streamtab  */
	D_NEW | D_MTSAFE	/* Driver compatibility flag */
};

struct dev_ops	vdi_ops = {

	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	vdi_getinfo,		/* get_dev_info */
	vdi_identify,		/* identify */
	nulldev,		/* probe */
	vdi_attach,		/* attach */
	nulldev,		/* detach */
	nulldev,		/* reset */
	&vdi_cb_ops,		/* driver operations */
	(struct bus_ops *)0	/* bus operations */

};

/*
 * Module linkage information for the kernel.
 */

#include <sys/modctl.h>

extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This one is a pseudo driver */
	"VPIX Driver for Direct Device Access 'dda'",
	&vdi_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};

int
_init(void)
{
	int e;

	if ((e = ddi_soft_state_init(&vdi_state,
			sizeof (vdi_devstate_t), 1)) != 0) {
		return (e);
	}
	if ((e = mod_install(&modlinkage)) != 0)  {
		ddi_soft_state_fini(&vdi_state);
	}
	mutex_init(&vdi_lock, "VDI global mutex", MUTEX_DRIVER, 0);
	return (e);
} 


int
_fini(void)
{
	int e;

	if ((e = mod_remove(&modlinkage)) != 0)  {
		return (e);
	}
	ddi_soft_state_fini(&vdi_state);
	return (e);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
vdi_identify(dev_info_t *devi)
{
	if (strcmp(ddi_get_name(devi), "vdi") == 0)
		return (DDI_IDENTIFIED);
	else
		return (DDI_NOT_IDENTIFIED);
}

/*ARGSUSED*/
static int
vdi_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	int i;
	vdi_devstate_t *vsp;
	char name[12];
	int instance;

	instance = ddi_get_instance(devi);
	if (ddi_soft_state_zalloc(vdi_state, instance) != DDI_SUCCESS) {
		cmn_err(CE_CONT, "%s%d: can't allocate state\n",
		    ddi_get_name(devi), instance);
		return (DDI_FAILURE);
	} else
		vsp = ddi_get_soft_state(vdi_state, instance);

	/* create a minor node */
	sprintf(name, "%d", instance);
	if (ddi_create_minor_node(devi, name, S_IFCHR, instance, NULL, NULL)) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}
	vsp->vdi_devi = devi;
	vsp->vdi_inum = sparc_pd_getintr(devi, 0)->intrspec_vec;
	mutex_init(&vsp->vdi_lock, "VDI resource mutex", MUTEX_DRIVER, 0);
	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
vdi_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	register int error = DDI_FAILURE;
	vdi_devstate_t *vsp;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if ((vsp = ddi_get_soft_state(vdi_state,
				getminor((dev_t)arg))) != NULL) {
			*result = vsp->vdi_devi;
			error = DDI_SUCCESS;
		}
		else
			*result = NULL;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)getminor((dev_t) arg);
		error = DDI_SUCCESS;
		break;
	}
	return (error);
}

/*
 * Open the generic device. All opens are done exclusively
 */
static int
vdi_open(devp, flag, typ, cred)
	dev_t *devp;
	int flag;
	int typ;
	struct cred *cred;
{
	register int		i;
	vdi_devstate_t *vsp;

	/* only v86 processes can open a generic device */
	if (curthread->t_v86data == (caddr_t)NULL)
		return (EACCES);

	if ((vsp = ddi_get_soft_state(vdi_state, getminor(*devp))) == NULL)
		return (ENXIO);

	if ((i = getminor(*devp)) >= VDMAXVDEVS)
		return (ENXIO);

	mutex_enter(&vdi_lock);
	/* make sure device is not already opened */
	if (vdi_dev_used[i] != -1) {
		mutex_exit(&vdi_lock);
		return (ENXIO);
	}
	vdi_dev_used[i] = *devp;
	vsp->vdi_owner = curthread;
	mutex_exit(&vdi_lock);

	return (0);
}

/*
 * close the generic device
 */
static int
vdi_close(dev, flag, typ, cred)
	dev_t dev;
	int flag;
	int typ;
	struct cred *cred;
{
	register int	vdindex;  /* relative level of infection */
	register int	i;
	vdi_devstate_t	*vsp;

	/* only v86 processes can open a generic device */
	if (curthread->t_v86data == (caddr_t)NULL)
		return (EACCES);

	vdindex = getminor(dev);
	if ((vsp = ddi_get_soft_state(vdi_state, vdindex)) == NULL)
		return (ENXIO);

	if (vdi_dev_used[vdindex] == -1)
		return (EINVAL);

	mutex_enter(&vdi_lock);
	/*
	 * empty out the info structure so we no longer give pseudorupts
	 * to the process that's closed the device
	 */
	if (vsp->vdi_owner == curthread) {
		mutex_enter(&vsp->vdi_lock);
		if (vsp->vdi_stash) {
			/* Remove the interrupt handler from ISR list */
			ddi_remove_intr(vsp->vdi_devi, 0, vsp->vdi_iblock);
			vsp->vdi_stash = 0;
			vsp->vdi_pseudonum = -1;
			vsp->vdi_flag = 0;
		}
		vsp->vdi_owner = NULL;
		mutex_exit(&vsp->vdi_lock);
	} else {
		mutex_exit(&vdi_lock);
		return (EINVAL);
	}

#ifdef VDCHECK
	/* get rid of entries in the used table */
	delete_entries(vdi_io_used, vdindex);
	delete_entries(vdi_mem_used, vdindex);
#endif /* VDCHECK */
	/* indicate that the device is no longer used */
	vdi_dev_used[vdindex] = -1;
	mutex_exit(&vdi_lock);
}

static int
vdi_ioctl(dev_t dev, int cmd, int arg, int flag, cred_t *cred, int *rvalp)
{
	struct vdev	vd;
	register int	vdintr = -1;
	vdi_devstate_t	*vsp;

	if (curthread->t_v86data == (caddr_t)NULL)
		return (EACCES);

	if ((getminor(dev) >= VDMAXVDEVS) ||
	    ((vsp = ddi_get_soft_state(vdi_state, getminor(dev))) == NULL))
		return (ENXIO);

	mutex_enter(&vdi_lock);
	if (vsp->vdi_owner != curthread) {
		mutex_exit(&vdi_lock);
		return (ENXIO);
	}
	switch (cmd) {
	case VDI_SET:
		/* get the arguments */
		if (copyin((caddr_t)arg, (caddr_t)&vd, sizeof (vd)) == -1) {
			mutex_exit(&vdi_lock);
			return (EFAULT);
		}
		/* make sure all the arguments are in range */
		if ((vdi_validate_params(&vd, dev, vsp, ENABLE)) == -1) {
			mutex_exit(&vdi_lock);
			return (EFAULT);
		}
		/* this gets done only for devices with interrupts */
		vdintr = vd.vdi_intnum;
		if (vdintr != -1) {
			mutex_enter(&vsp->vdi_lock);
			/* Install the interrupt handler */
			if (ddi_add_intr(vsp->vdi_devi, 0, &vsp->vdi_iblock, 0,
			    vdi_intr, (caddr_t)vsp)) {
				cmn_err(CE_WARN,
					"vdi_ioctl: cannot add intr");
				mutex_exit(&vsp->vdi_lock);
				mutex_exit(&vdi_lock);
				return (EFAULT);
			}
			vsp->vdi_stash = curthread;

			/* set up the pseudorupt if specified */
			if (vd.vdi_pseudomask)
				vsp->vdi_pseudonum = vd.vdi_pseudomask;
			mutex_exit(&vsp->vdi_lock);
		}

		/* now map in the memory if specified */
		if (vd.vdi_memlen) {
			vdi_unmapmem(vd.vdi_vmembase, vd.vdi_memlen);
			if (vdi_mapmem(vd.vdi_vmembase, vd.vdi_memlen,
						vd.vdi_pmembase)) {
				mutex_exit(&vdi_lock);
				return (EFAULT);
			}
		}
		break;

	case VDI_UNSET:

		/* get the arguments */
		if (copyin((caddr_t)arg, (caddr_t)&vd, sizeof (vd)) == -1) {
			mutex_exit(&vdi_lock);
			return (EFAULT);
		}

		/* make sure all the arguments are in range */
		if ((vdi_validate_params(&vd, dev, DISABLE)) == -1) {
			mutex_exit(&vdi_lock);
			return (EFAULT);
		}

		/* disable interrupts if specified */
		vdintr = vd.vdi_intnum;
		if (vdintr != -1) {
			mutex_enter(&vsp->vdi_lock);
			if (vsp->vdi_stash == NULL) {
				mutex_exit(&vsp->vdi_lock);
				mutex_exit(&vdi_lock);
				return (EINVAL);
			}
			/* Remove the interrupt handler from ISR list */
			ddi_remove_intr(vsp->vdi_devi, 0, vsp->vdi_iblock);
			vsp->vdi_stash = 0;
			vsp->vdi_pseudonum = -1;
			vsp->vdi_flag = 0;
			mutex_exit(&vsp->vdi_lock);
		}

		if (vd.vdi_memlen) {
			vdi_unmapmem(vd.vdi_vmembase, vd.vdi_memlen);
		}
		break;
	}
	mutex_exit(&vdi_lock);
	return (0);
}

/*
 * generic interrupt routine simply sends the pseudorupt and returns
 * (Note: Since the interrupt lines are shared in Solaris system and
 *	  this driver has no knowledge about whether this DDA device
 *	  is actually interrupting or not, it simply sends a pseudorupt
 *	  to the VPIX and lets the DOS driver figure out what to do
 *	  with it.)
 */
static u_int
vdi_intr(caddr_t arg)
{
	vdi_devstate_t *vsp = (vdi_devstate_t *)arg;

	mutex_enter(&vsp->vdi_lock);
	/* send the psuedorupt */
	if (vsp->vdi_stash) {
		v86setpseudo(vsp->vdi_stash, vsp->vdi_pseudonum);
		mutex_exit(&vsp->vdi_lock);
		return (DDI_INTR_UNCLAIMED); /* safe thing to do */
	}
	mutex_exit(&vsp->vdi_lock);

	return (DDI_INTR_UNCLAIMED);
}


/*
 * make sure the interrupt line is not reserved or already used by vdi
 */
vdi_validate_params(vd, dev, vsp, function)
register struct vdev *vd;
register dev_t dev;
vdi_devstate_t *vsp;
register int function;
{
    register int ival;

	/* check the interrupt vector specified */
    ival = vd->vdi_intnum;
    if (ival != -1) {
	switch (function) {
	    case ENABLE:
		if ((vsp->vdi_stash != NULL) || (ival != vsp->vdi_inum))
			return (-1);
		break;

	    case DISABLE:
		if ((vsp->vdi_stash == NULL) || (ival != vsp->vdi_inum))
			return (-1);
		break;
	}
    }

#ifdef VDCHECK
	/* check the io range specified */
	if (vd->vdi_iolen) {
		if (check_table(vdi_io_used, (long)vd->vdi_iobase,
				vd->vdi_iolen, getminor(dev), function) == -1)
			return (-1);
	}
	/* check the memory range specified  */
	if (vd->vdi_memlen) {
		if (check_table(vdi_mem_used, vd->vdi_pmembase,
		    vd->vdi_memlen, getminor(dev), function) == -1) {
			/* undo the above operation */
			check_table(vdi_io_used, (long)vd->vdi_iobase,
					vd->vdi_iolen, getminor(dev), DISABLE);
			return (-1);
		}
	}
#endif /* VDCHECK */

    return (1);
}


#ifdef VDCHECK
/* generic routine that add or deletes entries to used_tab structures */
check_table(used_tab, base, len, dev, function)
register struct vdi_used *used_tab;
register long base;
register int len;
register char dev;
register int function;
{
    register int i;
    register struct vdi_used *tabptr, *nexttab;

    switch (function) {
    case ENABLE:    /* insert into the table */
	/* first make sure its not there */
	tabptr = used_tab;
	i = 0;
	while (tabptr->base != 0 && i < VDUSELEN) {
	    /* make sure there is no interference */
	    if (base < tabptr->base + tabptr->len &&
	        base + len >= tabptr->base)
	      return (-1);
	    i++;
	    tabptr++;
	}
	if (i >= VDUSELEN) /* ran out of entries */
	  return (-1);

	/* go ahead and add them */
	tabptr->base = base;
	tabptr->len = len;
	tabptr->dev = dev;
	break;

     case CHECK:
	/* just make sure its not interfering */
	tabptr = used_tab;
	i = 0;
	while (tabptr->base != 0 && i < VDUSELEN) {
	    /* make sure there is no interference */
	    if (base < tabptr->base + tabptr->len &&
	        base + len >= tabptr->base)
	      return (-1);
	    i++;
	    tabptr++;
	}
	break;

     case DISABLE:        /* delete entry from table */
	tabptr = used_tab;
	i = 0;
	while (tabptr->base != 0 && i < VDUSELEN) {
	    if (base == tabptr->base && len == tabptr->len)
		break;
	    i++;
	    tabptr++;
	}
	if (i >= VDUSELEN)
	  return (-1);

	/* shift everything over one */
	nexttab = tabptr + 1;
	while (tabptr->base != 0 && i < VDUSELEN) {
	    tabptr->base = nexttab->base;
	    tabptr->len = nexttab->len;
	    tabptr->dev = nexttab->dev;
	    tabptr++;
	    nexttab++;
	    i++;
	}
	break;
     default:
	return (-1);
	break;
    }
    return (1);
}

/*
 * delete all entries for this device from the used tables
 * this is necessary if the program terminates abnormally
 */
delete_entries(used_tab, dev)
struct vdi_used *used_tab;
register char dev;
{
    register struct vdi_used *tabptr = used_tab;
    register int i = 0;

    /* look for the entries with this device number */
    while (tabptr->base != 0 && i < VDUSELEN) {
	if (tabptr->dev == dev) {	/* delete the entry */
	    check_table(tabptr, tabptr->base, tabptr->len,
		tabptr->dev, DISABLE);
	} else {
	    tabptr++;
	    i++;
	}
    }
}
#endif /* VDCHECK */

/*
 * vdi_mapmem() maps the device memory into the user specified address space.
 * Returns 0 for sucesss and -1 for failure.
 * XXX - needs review.
 */
int
vdi_mapmem(vaddr, len, paddr)
	u_long vaddr;
	int	len;
	u_long paddr;
{
	proc_t *pp = ttoproc(curthread);
	struct as *as;
	u_long addr;
	int pf;

	/* make sure that the device memory is not in system RAM */
	for (addr = paddr; addr < (paddr+len); addr += PAGESIZE) {
		if (pf_is_memory((u_int)btop(addr)))
			return (-1);
	}

	/* unload any previous mappings for the specified virtual address */
	as = pp->p_as;
	hat_unload(as, (caddr_t)vaddr, len, HAT_UNLOAD);

	/* load the mapping */
	pf = btop(paddr);
	for (addr = vaddr; addr < (vaddr+len); addr += PAGESIZE, pf++) {
		hat_devload(as->a_hat, (caddr_t)addr, PAGESIZE, pf,
		    PROT_WRITE | HAT_NOSYNC, HAT_LOAD_LOCK);
	}
	return (0);
}

/*
 * vdi_unmapmem() unmaps the device memory.
 * XXX - needs review.
 */
int
vdi_unmapmem(vaddr, len)
	caddr_t vaddr;
	int	len;
{
	proc_t *pp = ttoproc(curthread);
	struct as *as;

	as = pp->p_as;
	hat_unload(as, vaddr, len, HAT_UNLOAD);
}

#else /* !_VPIX */

/*
 * stubs for module wrapper when _VPIX is not defined.
 */

#include <sys/types.h>
#include <sys/sunddi.h>
#include <sys/modctl.h>

int
_init(void)
{
	return (DDI_FAILURE);
} 

int
_fini(void)
{
	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
_info(struct modinfo *modinfop)
{
	return (0);
}

#endif /* _VPIX */
