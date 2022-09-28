/*
 * Copyright (c) 1992-1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)autoconf.c	1.65	96/10/15 SMI"

/*
 * Setup the system to run on the current machine.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/utsname.h>
#include <sys/eisarom.h>
#include <sys/nvm.h>
#include <sys/bootconf.h>
#include <sys/ethernet.h>
#include <sys/kmem.h>
#include <sys/cpu.h>
#include <sys/mmu.h>
#include <sys/cpuvar.h>
#include <sys/debug.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/esunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/modctl.h>
#include <sys/hwconf.h>
#include <sys/avintr.h>
#include <sys/autoconf.h>
#include <sys/instance.h>
#include <sys/fp.h>
#include <sys/systeminfo.h>
#include <sys/archsystm.h>

/*
 * For eisa NVM support
 */
/*
#define	DEBUG_EISA_NVM	1
*/

#define	FALSE	0
#define	TRUE	1

#define	OBP_NAME		"name"

#ifdef KEEP_INT15
/* Start using the property from boot.  Set to 1 to use protected mode int15 */
int eisa_int15 = 0;
#endif

/* place to put the eisa_nvram */
caddr_t	eisa_nvmp;
int	eisa_nvmlength = 0;

#if !defined(SAS) && !defined(MPSAS)

static void get_boot_properties(void);

int envm_check(void);

/*
 * Local functions
 */
static int reset_leaf_device(dev_info_t *, void *);

static int getlongprop_buf(int id, char *name, char *buf, int maxlen);
static void add_root_props(dev_info_t *devi);
static int get_neighbors(dev_info_t *, void *);

static int get_hw_neighbors(dev_info_t *, void *);
static void get_hw_props(dev_info_t *di);

static void di_dfs(dev_info_t *, int (*)(), caddr_t);

static int eisa_getfunc(register regs *rp);
static int eisa_getslot(register regs *rp);
static int eisa_read_func(register int slot, register int func,
	register char *buffer);
static int eisa_getnvm(register regs *rp);

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */

dev_info_t *top_devinfo;

#endif	/* !SAS && !MPSAS */

/*
 * Machine type we are running on.
 */
short cputype;
short cacheflsh;

/*
 * Return the favoured drivers of this implementation
 * architecture.  These drivers MUST be present for
 * the system to boot at all.
 *
 * XXX - rootnex must be loaded before options because of the ddi
 *	 properties implementation.
 *
 * Used in loadrootmodules() in the swapgeneric module.
 */
char *
get_impl_module(int first)
{
	static char **p;
	static char *impl_module_list[] = {
		"rootnex",
		"options",
		"sad",
		"objmgr",
		"isa",
		"eisa",
		"mc",
		(char *)0
	};

	if (first)
		p = impl_module_list;
	if (*p != (char *)0)
		return (*p++);
	else
		return ((char *)0);
}

/*
 * i_path_to_drv:
 *
 * Return an alternate driver name binding for the leaf device
 * of the given pathname, if there is one.  The purpose of this
 * function is to deal with generic pathnames. The default action
 * for platforms that can't do this (ie: sun4c 1.x proms or x86 or
 * any platform that does not have prom_finddevice functionality,
 * which matches nodenames and unit-addresses without the drivers
 * participation) is to return NULL.
 *
 * Used in loadrootmodules() in the swapgeneric module to
 * associate a given pathname with a given leaf driver.
 *
 * Used in ddi_pathname_to_dev_t/bind_child in sunddi.c to
 * associate a given generic pathname with a given devinfo node.
 */

/*ARGSUSED*/
char *
i_path_to_drv(char *path)
{
	dnode_t pn;
	char *p = (char *)0;
	char *q = (char *)0;
	void *cp;
	int len;
	extern dnode_t prom_findnode_byname();

	/*
	 * NB: This requires an external agent supplying a device
	 * tree with hardware devinfo nodes and 'finddevice'
	 * functionality, so it can match the pathname to a given
	 * nodeid and find the device's compatible property if it
	 * exists, all without the driver being loaded, since we
	 * don't know (yet) which driver owns this device pathname
	 * XXX - The prom emulation stuff in x86 is sufficient to make this
	 * XXX - function workable as long as the same device name always
	 * XXX - maps to the same driver name.  This is true until
	 * XXX - the advent of generic names which will happen when PCMCIA
	 * XXX - support goes in.
	 *
	 * The framework uses an 'exact match' algorithm
	 * (driver-name == node-name) if this function returns 0.
	 */
	q = kmem_zalloc(strlen(path), KM_SLEEP);
	if (prom_devname_from_pathname(path, q) < 0)
		return ((char *)0);
	if ((pn = prom_findnode_byname(prom_nextnode((dnode_t)0), q)) ==
			OBP_NONODE)
		return ((char *)0);
	if ((len = prom_getproplen(pn, "compatible")) <= 0)
		return ((char *)0);
	cp = kmem_zalloc((size_t)len, KM_SLEEP);
	if ((len = prom_getprop(pn, "compatible", (caddr_t)cp)) <= 0)
		return ((char *)0);
	while ((p = prom_decode_composite_string(cp, (size_t)len, p)) != 0) {
		if (ddi_name_to_major(p) != -1) {
			if ((strcmp(p, q) != 0))
				CPRINTF2("%s bound to %s\n", path, p);
			return (p);
		}
	}
	return ((char *)0);
}

/*
 * Configure the hardware on the system.
 * Called before the rootfs is mounted
 */
void
configure(void)
{
	register int major;
	register dev_info_t *dip;
	extern int fpu_pentium_fdivbug;
	extern int fpu_ignored;

	/*
	 * Determine if an FPU is attached
	 */

#ifndef	MPSAS	/* no fpu module yet in MPSAS */
	fpu_probe();
#endif
	if (fpu_pentium_fdivbug) {
		printf("\
FP hardware exhibits Pentium floating point divide problem\n");
		if (!fpu_ignored)
			printf("\
If you wish to disable the FPU, edit /etc/system and append:\n\
\tset use_pentium_fpu_fdivbug = 0\n");
	}
	if (fpu_ignored) {
		printf("FP hardware will not be used\n");
	} else if (!fpu_exists) {
		printf("No FPU in configuration\n");
	}

	/*
	 * Initialize devices on the machine.
	 * Uses configuration tree built by the PROMs to determine what
	 * is present, and builds a tree of prototype dev_info nodes
	 * corresponding to the hardware which identified itself.
	 */
#if !defined(SAS) && !defined(MPSAS)
	/*
	 * Record that devinfos have been made for "rootnex."
	 */
	major = ddi_name_to_major("rootnex");
	devnamesp[major].dn_flags |= DN_DEVI_MADE;

	/*
	 * Create impl. specific root node properties...
	 */
	add_root_props(top_devinfo);

	/*
	 * Read in the properties from the boot.
	 */
	get_boot_properties();

	/*
	 * Set the name part of the address to make the root conform
	 * to canonical form 1.  (Eliminates special cases later).
	 */
	dip = ddi_root_node();
	if (impl_ddi_sunbus_initchild(dip) != DDI_SUCCESS)
		cmn_err(CE_PANIC, "Could not initialize root nexus");

#ifdef	DDI_PROP_DEBUG
	(void) ddi_prop_debug(1);	/* Enable property debugging */
#endif	DDI_PROP_DEBUG

#endif	/* !SAS && !MPSAS */

}

/*
 * This routine transforms either a prototype or canonical form 1 dev_info
 * node into a canonical form 2 dev_info node.  If the transformation fails,
 * the node is removed.
 */

int
impl_proto_to_cf2(dev_info_t *dip)
{
	int error, circular;
	struct dev_ops *ops;
	register major_t major;
	register struct devnames *dnp;

	if ((major = ddi_name_to_major(ddi_get_name(dip))) == -1)
		return (DDI_FAILURE);

	if ((ops = mod_hold_dev_by_major(major)) == NULL)
		return (DDI_FAILURE);

	/*
	 * Wait for or get busy/changing.  We need to stall here because
	 * of the alternate path for h/w devinfo nodes.
	 */

	dnp = &(devnamesp[major]);
	LOCK_DEV_OPS(&(dnp->dn_lock));

	/*
	 * Is this thread already installing this driver?
	 * If yes, mark it as a circular dependency and continue.
	 * If not, wait for other threads to finish with this driver.
	 */
	if (DN_BUSY_CHANGING(dnp->dn_flags) &&
	    (dnp->dn_busy_thread == curthread))  {
		dnp->dn_circular++;
	} else {
		while (DN_BUSY_CHANGING(dnp->dn_flags))
			cv_wait(&(dnp->dn_wait), &(dnp->dn_lock));
		dnp->dn_flags |= DN_BUSY_LOADING;
		dnp->dn_busy_thread = curthread;
	}
	circular = dnp->dn_circular;
	UNLOCK_DEV_OPS(&(dnp->dn_lock));

	/*
	 * If it's a prototype node, transform to CF1.
	 */
	if ((error = ddi_initchild(ddi_get_parent(dip), dip)) != DDI_SUCCESS) {
		/*
		 * Retain h/w devinfos, eliminate .conf file devinfos
		 */
		if (ddi_get_nodeid(dip) == DEVI_PSEUDO_NODEID)
			(void) ddi_remove_child(dip, 0);
		if (error == DDI_NOT_WELL_FORMED)	/* An artifact ... */
			error = DDI_FAILURE;
		ddi_rele_driver(major);
		goto out;
	}

	if (!DDI_CF2(dip)) {
		DEVI(dip)->devi_ops = ops;
		if ((error = impl_initdev(dip)) == DDI_SUCCESS) {
			LOCK_DEV_OPS(&(dnp->dn_lock));
			dnp->dn_flags |= DN_DEVS_ATTACHED;
			UNLOCK_DEV_OPS(&(dnp->dn_lock));
		}
		/*
		 * Driver Release/remove child done in impl_initdev!
		 * (for error case.)
		 */
		goto out;
	}

	/*
	 * This assert replaces some code to make sure the driver is
	 * actually attached to the dip -- it had better be at this point.
	 */
	ASSERT(ddi_get_driver(dip) == ops);

out:
	LOCK_DEV_OPS(&(dnp->dn_lock));
	if (circular)
		dnp->dn_circular--;
	else  {
		dnp->dn_flags &= ~(DN_BUSY_CHANGING_BITS);
		dnp->dn_busy_thread = NULL;
		cv_broadcast(&(dnp->dn_wait));
	}
	UNLOCK_DEV_OPS(&(dnp->dn_lock));
	return (error);
}

/*ARGSUSED*/
int
impl_check_cpu(dev_info_t *devi)
{
	return (DDI_SUCCESS);
}

/*
 * This is settable in /etc/system ... the default is currently
 * non-zero, which means that we call a driver's identify(9e)
 * entry point. The framework doesn't *need* to do this, because it
 * has other sources of binding information for device drivers.
 * However, we call identify(9e) in the unlikely (non-compliant) case
 * that there's an odd 3rd party driver out there depending on it.
 */
int identify_9e = 1;

int
impl_probe_attach_devi(dev_info_t *dev)
{
	register int r;

	if (identify_9e != 0)
		(void) devi_identify(dev);


	switch (r = devi_probe(dev)) {
	case DDI_PROBE_DONTCARE:
	case DDI_PROBE_SUCCESS:
		break;
	default:
		return (r);
	}

	return (devi_attach(dev, DDI_ATTACH));
}

/*
 * This routine transforms a canonical form 1 dev_info node into a
 * canonical form 2 dev_info node.  If the transformation fails, the
 * node is removed.
 */
int
impl_initdev(dev_info_t *dev)
{
	register struct dev_ops *ops;
	register int r;

	ops = ddi_get_driver(dev);
	ASSERT(ops);
	ASSERT(DEV_OPS_HELD(ops));

	DEVI(dev)->devi_instance = e_ddi_assign_instance(dev);

	if ((r = impl_probe_attach_devi(dev)) == DDI_SUCCESS)  {
		e_ddi_keep_instance(dev);
		return (r);
	}

	/*
	 * Partial probe or failed probe/attach...
	 * Retain leaf device driver nodes for deferred attach.
	 * (We need to retain the assigned instance number for
	 * deferred attach.  The call to e_ddi_free_instance is
	 * advisory -- it will retain the instance number if it's
	 * ever been kept before.)
	 */
	ddi_set_driver(dev, NULL);		/* dev --> CF1 */
	ddi_rele_driver(ddi_name_to_major(ddi_get_name(dev)));
	if (!NEXUS_DRV(ops))  {
		e_ddi_keep_instance(dev);
	} else {
		e_ddi_free_instance(dev);
		(void) ddi_uninitchild(dev);
		/*
		 * Retain h/w nodes in prototype form.
		 */
		if (ddi_get_nodeid(dev) == DEVI_PSEUDO_NODEID)
			(void) ddi_remove_child(dev, 0);
	}

	return (r);
}

/*
 * Reset all the pure leaf drivers on the system at halt time
 * We deliberately skip children of the 'pseudo' nexus, as they
 * don't have any hardware to reset.
 */
void
reset_leaves(void)
{
	ddi_walk_devs(top_devinfo, reset_leaf_device, 0);
}

/* ARGSUSED */
static int
reset_leaf_device(dev_info_t *dev, void *arg)
{
	struct dev_ops *ops;

#ifdef XXX
	if (DEVI(dev)->devi_nodeid == DEVI_PSEUDO_NODEID)
		return (DDI_WALK_PRUNECHILD);
#endif

	if ((ops = DEVI(dev)->devi_ops) != (struct dev_ops *)0 &&
	    ops->devo_reset != nodev) {
		CPRINTF2("resetting %s%d\n", ddi_get_name(dev),
			ddi_get_instance(dev));
		(void) devi_reset(dev, DDI_RESET_FORCE);
	}

	return (DDI_WALK_CONTINUE);
}

static int
getlongprop_buf(int id, char *name, char *buf, int maxlen)
{
	int size;

	size = prom_getproplen((dnode_t)id, name);
	if (size <= 0 || (size > maxlen - 1))
		return (-1);

	if (-1 == prom_getprop((dnode_t)id, name, buf))
		return (-1);

	if (strcmp("name", name) == 0) {
		if (buf[size - 1] != '\0') {
			buf[size] = '\0';
			size += 1;
		}
	}

	return (size);
}

/*
 * Add and remove implementation specific software defined properties
 * for a device. Can be used to override or supplement any properties
 * derived from the prom. Almost by definition this is ugly.
 *
 * MJ: Should be in a separate machine specific file.
 */

/*
 * XXX: This will need another field to handle property undefs.
 * and non-wildcarded properties.
 */

struct prop_def {
	char	*prop_name;
	int	prop_len;
	caddr_t	prop_value;
};


/*
 * Add statically defined root properties to this list...
 */

static const int pagesize = PAGESIZE;
static const int mmu_pagesize = MMU_PAGESIZE;
static const int mmu_pageoffset = MMU_PAGEOFFSET;

static struct prop_def root_props[] = {
{ "PAGESIZE",		sizeof (int),		(caddr_t)&pagesize },
{ "MMU_PAGESIZE",	sizeof (int),		(caddr_t)&mmu_pagesize},
{ "MMU_PAGEOFFSET",	sizeof (int),		(caddr_t)&mmu_pageoffset},
};

#define	NROOT_PROPS	(sizeof (root_props) / sizeof (struct prop_def))

static void
add_root_props(dev_info_t *devi)
{
	int i;
	struct prop_def *rpp;

	/*
	 * Note this for loop works because all of the root_prop
	 * properties are integers - if this changes, the for
	 * loop will have to change.
	 */
	for (i = 0, rpp = root_props; i < NROOT_PROPS; ++i, ++rpp) {
		(void) e_ddi_prop_update_int(DDI_DEV_T_NONE, devi,
		    rpp->prop_name, *((int *)rpp->prop_value));
	}

	/*
	 * Create the root node "boolean" property
	 * corresponding to addressing type supported in the root node:
	 *
	 * Choices are:
	 *	"relative-addressing" (OBP PROMS)
	 *	"generic-addressing"  (Sun4 -- pseudo OBP/DDI)
	 */

	(void) e_ddi_prop_update_int(DDI_DEV_T_NONE, devi,
	    DDI_RELATIVE_ADDRESSING, 1);

}

#if 0	/* Not currently used i86pc */
void
impl_add_dev_props(dev_info_t *dip)
{
	/*
	 * In this implementation, we only have a few
	 * root properties to deal with.
	 */
	if (dip == top_devinfo) {
		add_root_props(dip);
	}
}
#endif

void
impl_rem_dev_props(dev_info_t *dip)
{
	ddi_prop_remove_all(dip);
	e_ddi_prop_remove_all(dip);
}

void
impl_rem_hw_props(dev_info_t *dip)
{
	ndi_prop_remove_all(dip);
}

/*
 * Allow for implementation specific correction of PROM property values.
 */

/*ARGSUSED*/
void
impl_fix_props(dev_info_t *dip, dev_info_t *ch_dip, char *name, int len,
    caddr_t buffer)
{
	/*
	 * There are no adjustments needed in this implementation.
	 */
}


#define	NAME_LEN	160

/*ARGSUSED*/
static int
get_neighbors(dev_info_t *di, void *arg)
{
	register int nid, snid, cnid;
	dev_info_t *parent;
	char buf[NAME_LEN];

	if (di == NULL)
		return (DDI_WALK_CONTINUE);

	nid = ddi_get_nodeid(di);
	snid = (int)prom_nextnode((dnode_t)nid);
	cnid = (int)prom_childnode((dnode_t)nid);

	if (snid && (snid != -1) && ((parent = ddi_get_parent(di)) != NULL)) {
		/* Make a sibling */
		if (0 < getlongprop_buf(snid, OBP_NAME, buf, NAME_LEN))
			(void) ddi_add_child(parent, buf, snid, -1);
	}

	if (cnid && (cnid != -1)) {
		/* Make a child */
		if (0 < getlongprop_buf(cnid, OBP_NAME, buf, NAME_LEN))
			(void) ddi_add_child(di, buf, cnid, -1);
	}

	return (DDI_WALK_CONTINUE);
}

#define	NAME_LEN_1275 32

/*ARGSUSED1*/
static int
get_hw_neighbors(dev_info_t *di, void *arg)
{
	char name[NAME_LEN_1275];	/* name prop. is 32 bytes max. */
	dev_info_t *parent, *child;
	phandle_t ph, nph;

	if (di == NULL)
		return (DDI_WALK_CONTINUE);

	ph = (phandle_t)DEVI(di)->devi_driver_data;

	if (((nph = BOP1275_PEER(bootops, ph)) != (phandle_t)0) &&
	    (BOP1275_GETPROP(bootops, nph, "name", name, NAME_LEN_1275) > 0) &&
	    ((parent = ddi_get_parent(di)) != (dev_info_t *)0)) {
		child = ddi_add_child(parent, name, (u_int)DEVI_SID_NODEID, -1);
		if (child) {
			DEVI(child)->devi_driver_data = (caddr_t)nph;
			get_hw_props(child);
		}
	}

	if (((nph = BOP1275_CHILD(bootops, ph)) != (phandle_t)0) &&
	    (BOP1275_GETPROP(bootops, nph, "name", name, NAME_LEN_1275) > 0)) {
		child = ddi_add_child(di, name, (u_int)DEVI_SID_NODEID, -1);
		if (child) {
			DEVI(child)->devi_driver_data = (caddr_t)nph;
			get_hw_props(child);
		}
	}

	return (DDI_WALK_CONTINUE);
}

/*ARGSUSED1*/
static int
clear_driver_data(dev_info_t *di, void *arg)
{
	if (di == NULL)
		return (DDI_WALK_CONTINUE);

	DEVI(di)->devi_driver_data = NULL;
	return (DDI_WALK_CONTINUE);
}

#ifdef XXX_USED
/*
 * softlevel1 is called out of the SOINTR macro in locore.s with
 * the unsafe_driver lock held. We have to drop it here because
 * softint may call somebody who wants it.
 *
 * XXX	Do we really need this unsafe_driver stuff?
 *	Why isn't it in sun4c/sun4??
 */
/*ARGSUSED*/
static u_int
softlevel1(caddr_t arg)
{
	int driver_mutex = 0;	/* get unsafe_driver before returning */

	if (UNSAFE_DRIVER_LOCK_HELD()) {
		driver_mutex = 1;
		mutex_exit(&unsafe_driver);
	}
	softint();
	if (driver_mutex)
		mutex_enter(&unsafe_driver);
	return (1);
}
#endif

/*
 * Get node properties from boot.
 */
static void
get_hw_props(dev_info_t *di)
{
	int len, flags;
	char *prvname, name[NAME_LEN_1275];
	caddr_t val;
	phandle_t ph;

	flags = DDI_PROP_CANSLEEP | DDI_PROP_HW_DEF;

	ph = (phandle_t)DEVI(di)->devi_driver_data;

	for (prvname = NULL;
	    (BOP1275_NEXTPROP(bootops, ph, prvname, name) > 0);
	    prvname = name) {
		if ((len = BOP1275_GETPROPLEN(bootops, ph, name)) == -1)
			continue;

		if (len == 0) {
			flags |= DDI_PROP_UNDEF_IT;
			val = NULL;
		} else {
			flags &= ~DDI_PROP_UNDEF_IT;
			val = kmem_zalloc(len, KM_SLEEP);
			BOP1275_GETPROP(bootops, ph, name, val, len);
		}
		(void) ddi_prop_create(DDI_DEV_T_NONE, di, flags, name,
			val, len);
		/*
		 * Cache the value of the 'compatible' property from the prom,
		 */
		if (strcmp(name, "compatible") == 0) {
			DEVI(di)->devi_compat_names =
			    (char *)kmem_zalloc((size_t)len, KM_SLEEP);
			bcopy(val, DEVI(di)->devi_compat_names, (size_t)len);
			DEVI(di)->devi_compat_length = (size_t)len;
		}
		if (len)
			kmem_free(val, len);
	}
}

static char *rootname;		/* massaged name of root nexus */

/*
 * Create classes and major number bindings for the name of my root.
 * Called immediately before 'loadrootmodules'
 */
static void
impl_create_root_class(void)
{
	register char *cp;
	register int major, size;

	if ((major = ddi_name_to_major("rootnex")) == -1)
		cmn_err(CE_PANIC, "No major device number for 'rootnex'");

	size = (size_t)BOP_GETPROPLEN(bootops, "mfg-name");
	rootname = kmem_zalloc(size, KM_SLEEP);
	(void) BOP_GETPROP(bootops, "mfg-name", rootname);
	/*
	 * Fix conflict between OBP names and filesystem names.
	 * Substitute '_' for '/' in the name.  Ick.  This is only
	 * needed for the root node since '/' is not a legal name
	 * character in an OBP device name.
	 */
	for (cp = rootname; *cp; cp++)
		if (*cp == '/')
			*cp = '_';

	add_class(rootname, "root");
	make_mbind(rootname, major, mb_hashtab, NULL);

	/*
	 * The `platform' or `implementation architecture' name has been
	 * translated by boot to be proper for file system use.  It is
	 * the `name' of the platform actually booted.  Note the assumption
	 * is that the name will `fit' in the buffer platform (which is
	 * of size SYS_NMLN, which is far bigger than will actually ever
	 * be needed).
	 */
	(void) BOP_GETPROP(bootops, "impl-arch-name", platform);
}

/*
 * Create a tree from the PROM info
 */
static void
create_devinfo_tree(void)
{
	register int major;
	char buf[16];
	int have_hwnodes;

	top_devinfo = (dev_info_t *)
	    kmem_zalloc(sizeof (struct dev_info), KM_SLEEP);

	DEVI(top_devinfo)->devi_node_name = rootname;
	DEVI(top_devinfo)->devi_instance = -1;
	i_ddi_set_binding_name(top_devinfo, rootname);

	/*
	 * Look for the 1275 property 'bootpath' here. If it exists
	 * and has a non-NULL value we need to assimilate the device
	 * tree bootconf has constructed.  Otherwise we do things the
	 * old way.
	 */
	have_hwnodes = (BOP_GETPROPLEN(bootops, "bootpath") > 1);

	/*
	 *  X86 hardware nodes don't actually have PROM backing.
	 */
	if (have_hwnodes) {
		/*
		 *  We perform an initial PCI probe. Although we may
		 *  be about to build a device tree chock full of PCI info,
		 *  the drivers still will want to call back into PCI space,
		 *  and need the mechanism to do so.  This call will set
		 *  up the previously existing mechanisms, found in the
		 *  pci_autoconfig module.
		 */
		extern void impl_bus_initialprobe(void);

		impl_bus_initialprobe();
		DEVI(top_devinfo)->devi_nodeid = DEVI_SID_NODEID;
		DEVI(top_devinfo)->devi_driver_data =
		    (caddr_t)BOP1275_PEER(bootops, NULL);
	} else {
		DEVI(top_devinfo)->devi_nodeid = 0;	/* Was OBP_NONODE */
	}

	sprintf(buf, "di %x", (int)top_devinfo);
	mutex_init(&(DEVI(top_devinfo)->devi_lock), buf, MUTEX_DEFAULT, NULL);

	major = ddi_name_to_major("rootnex");
	devnamesp[major].dn_head = top_devinfo;

	DEVI(top_devinfo)->devi_bus_map_fault = (struct dev_info *)top_devinfo;
	DEVI(top_devinfo)->devi_bus_dma_map = (struct dev_info *)top_devinfo;
	DEVI(top_devinfo)->devi_bus_dma_ctl = (struct dev_info *)top_devinfo;
	DEVI(top_devinfo)->devi_bus_ctl = (struct dev_info *)top_devinfo;

	/*
	 * Record that devinfos have been made for "rootnex."
	 * di_dfs() is used to read the prom because it doesn't get the
	 * next sibling until the function returns, unlike ddi_walk_devs().
	 */
	if (have_hwnodes) {

		get_hw_props(top_devinfo);
		di_dfs(ddi_root_node(), get_hw_neighbors, 0);

		/*
		 * We've been using the private driver_data fields to store
		 * prom handles.  Return these to there untainted state.
		 */
		di_dfs(ddi_root_node(), clear_driver_data, 0);

	} else {
		di_dfs(ddi_root_node(), get_neighbors, 0);
	}
}

/*
 * Setup the DDI but don't necessarily init the DDI.  This will happen
 * later once /boot is released.
 */
void
setup_ddi(void)
{
	/*
	 * Initialize the instance number data base--this must be done
	 * after mod_setup and before the bootops are given up
	 */
	e_ddi_instance_init();
	impl_create_root_class();
	create_devinfo_tree();
	impl_ddi_callback_init();
	i_ndi_event_init_hashtable();
}

static void
di_dfs(dev_info_t *devi, int (*f)(), caddr_t arg)
{
	(*f)(devi, arg);
	if (devi) {
		di_dfs((dev_info_t *)DEVI(devi)->devi_child, f, arg);
		di_dfs((dev_info_t *)DEVI(devi)->devi_sibling, f, arg);
	}
}

/*
 * We set the cpu type from the idprom, if we can.
 * Note that we just read out the contents of it, for the most part.
 * Except for cputype, sigh.
 */

void
setcputype(void)
{
	cputype |= I86_PC;
}

#ifdef HWC_DEBUG

static void
di_print_sp(dev_info_t *dev, char *space)
{
	register char *c;

	if (dev) {
		printf("%s", space);
		di_print(dev);
		for (c = space; *c; c++)
		;
		*c++ = ' ';
		*c++ = ' ';
		*c++ = ' ';
	} else {
		space[strlen(space)-3] = '\0';
	}
}

static void
di_print_tree(dev_info_t *dev)
{
	char space[128] = "";
	di_print_sp(dev, space);
	di_dfs((dev_info_t)DEVI(dev)->devi_child, (int (*)())di_print_sp,
		space);
}

#endif

#ifdef DEBUG
int bootprop_debug = 0;
#endif

char *bootprop_ignore[] = {
	"memory-update",
	"virt-avail",
	"phys-avail",
	"phys-installed",
	(char *)0
};

static void
get_boot_properties(void)
{
	dev_info_t *devi;
	char *name;
	void *value;
	int length;
	char **ignore;
	extern struct bootops *bootops;
	extern char hw_provider[];
	char property_name[50], *tmp_name_ptr;

	/*
	 * Import "root" properties from the boot.
	 *
	 * We do this by invoking BOP_NEXTPROP until the list
	 * is completely copied in.
	 */

	devi = ddi_root_node();
	for (name = BOP_NEXTPROP(bootops, "");		/* get first */
	    name;					/* NULL => DONE */
	    name = BOP_NEXTPROP(bootops, name)) {	/* get next */

	/*
	 * Copy name to property_name, since name
	 * is in the low address range below kernelbase.
	 */

		{
			int i = 0;
			tmp_name_ptr = name;
			while (*tmp_name_ptr) {
				property_name[i] = *tmp_name_ptr++;
				i++;
			}
			property_name[i] = 0;
		}

		for (ignore = bootprop_ignore; *ignore; ++ignore) {
			if (strcmp(*ignore, property_name) == 0)
				break;
		}
		if (*ignore)
			continue;
		length = BOP_GETPROPLEN(bootops, property_name);
		if (length == 0)
			continue;
		/*
		 * special case for eisa nvram.  copy it to a special place
		 * Don't make it a property.
		 */
		if (strcmp(property_name, "eisa-nvram") == 0) {
			if ((value = kmem_zalloc(length, KM_NOSLEEP)) ==
								(void *)NULL)
				cmn_err(CE_PANIC,
						"no memory for EISA NVRAM");
			BOP_GETPROP(bootops, property_name, value);
			eisa_nvmp = (caddr_t)value;
			eisa_nvmlength = length;
			/* done with this.. go around again */
			continue;
		}
		if ((value = kmem_alloc(length, KM_NOSLEEP)) == (void *)NULL)
			cmn_err(CE_PANIC, "no memory for root properties");
		BOP_GETPROP(bootops, property_name, value);
#ifdef DEBUG
	if (bootprop_debug)
		if (length != 4)
			prom_printf("root property '%s' = '%s'\n",
					property_name, value);
		else
			prom_printf("root property '%s' = 0x%x\n",
					property_name, *(int *)value);
#endif
		if (strcmp(name, "si-machine") == 0) {
			(void) strncpy(utsname.machine, value, SYS_NMLN);
			utsname.machine[SYS_NMLN - 1] = (char)NULL;
		} else if (strcmp(name, "si-hw-provider") == 0) {
			(void) strncpy(hw_provider, value, SYS_NMLN);
			hw_provider[SYS_NMLN - 1] = (char)NULL;
		} else
			(void) e_ddi_prop_update_byte_array(DDI_DEV_T_NONE,
			    devi, property_name, (u_char *)value,
			    (u_int)length);
		kmem_free(value, length);
	}
#ifdef DEBUG
	if (bootprop_debug)
		int20();
#endif
}

/*
 * read slot data from eisa cmos memory
 */
eisa_read_slot(slot, buffer)
register int	slot;
register char	*buffer;
{
	regs		reg;
	register int	status;

	bzero((char *)&reg, sizeof (regs));
	reg.eax.word.ax = (unsigned short)EISA_READ_SLOT_CONFIG;
	reg.ecx.byte.cl = (unsigned char)slot;
	status = eisa_getnvm(&reg);

	/* Arranges data to match "NVM_SLOTINFO" structure. See "nvm.h". */

	*((short *)buffer) = reg.edi.word.di;
	buffer += sizeof (short);
	*((short *)buffer) = reg.esi.word.si;
	buffer += sizeof (short);
	*((short *)buffer) = reg.ebx.word.bx;
	buffer += sizeof (short);
	*buffer++ = reg.edx.byte.dh;
	*buffer++ = reg.edx.byte.dl;
	*((short *)buffer) = reg.ecx.word.cx;
	buffer += sizeof (short);
	return (status);
}

/*
 * read function data from eisa cmos memory
 */
static int
eisa_read_func(register int slot, register int func, register char *buffer)
{
	regs	reg;
	int	status;

	bzero((char *)&reg, sizeof (regs));
	reg.eax.word.ax = (unsigned short)EISA_READ_FUNC_CONFIG;
	reg.ecx.byte.cl = (unsigned char)slot;
	reg.ecx.byte.ch = (unsigned char)func;
	reg.esi.esi = (unsigned int)buffer;
	status = eisa_getnvm(&reg);

	/* Data is arranged to match "NVM_FUNCINFO" structure. See "nvm.h". */

	return (status);
}

/*
 *	"eisa_nvm" -	general-purpose function for extracting configuration
 *			data from EISA non-volatile memory.
 *
 *	Inputs:
 *		A pointer to a big buffer allocated by the caller.
 *			The size of the buffer is a finction of the number
 *			of slotp and functions expected.  Asking for the
 *			whole system configuration can blow 20k.
 *
 *		A argument mask defining the "keys" to search for.
 *
 *		A variable number of arguments following the argument mask.
 *
 *		! CAVEAT !	Arguments passed must be in the order shown in
 *				"key_mask" below.
 *				Arguments may be omitted but the ordering must
 *				be maintained.
 *		Examples:
 *
 * 		slot function (board_id mask) revision checksum type sub-type
 *
 *		slot
 *
 *		slot function
 *
 *		(board_id mask)
 *
 *		(board_id mask) type
 *
 *		(board_id mask) revision
 *
 *		type sub-type
 *
 *		slot sub-type
 *
 *	Output:
 *		The number of bytes actually copied into the caller's buffer.
 *
 *		All slot and function records that pertain to all of the "keys"
 *			in the following format:
 *
 *		short slot_number
 *
 *		1st slot record
 *
 *		    1st function record
 *			.
 *			.
 *			.
 *		    "nth" function record
 *
 *		short slot_number
 *
 *		"nth" slot record
 *
 *		etc . . .
 *
 *	Examples:
 *
 *		bytes = eisa_nvm(buffer, 0);
 *
 *		will copy all slot and function records into "buffer".
 *
 *		bytes = eisa_nvm(buffer, SLOT, 0);
 *
 *		will copy the record for slot 0 and all its function
 *		records into "buffer".
 *
 *		bytes = eisa_nvm(buffer, TYPE, "DISCO");
 *
 *		will copy any/all slot and function records that
 *		pertain to the board type "DISCO" into "buffer".
 *
 *		bytes = eisa_nvm(buffer, EISA_BOARD_ID | TYPE, 0x0140110e,
 *							0xffffff, "COM");
 *
 *		will copy any/all slot and function records that
 *		pertain to the board id xx40110e and the type "COM" into
 *		"buffer".
 *
 *		bytes = eisa_nvm(buffer, BOARD_ID | CHECKSUM | TYPE,
 *				0x0140110e, 0xffffffff, 0xABCD, "ASY");
 *
 *		will copy any/all slot and function records that pertain to
 *		the board id 0140110e, the checksum 0xABCD and the type "ASY"
 *		into "buffer".
 */

/*
	This defines the mask used to determine what arguments are passed in.
*/

/*
 * Fill the data buffer from eisa cmos memory then search for the
 * specified key.
 */
/*
 * the arguments just follow on from the key_mask.
 * the order of the arguments is as above.
 * arguments are only present if the corresponding bit is set in the key_mask
 */
int
eisa_nvm(data, key_mask)
char		*data;
KEY_MASK	key_mask;
{
	NVM_SLOTINFO	slot_info;
	NVM_FUNCINFO	func_info;
	char		*data_start = data;
	char		*new_slot;
	char		*argp = (char *)&key_mask + sizeof (key_mask);
	char		*type_arg = 0;
	char		*sub_type_arg = 0;
	int		len;
	unsigned int	val = 0;
	unsigned int	mask = 0;
	short		slot;
	short		slot_limit;
	short		function;
	short		function_arg;
	short		func_limit;
	unsigned short	revision = 0;
	unsigned short	checksum = 0;
	char		*type;	/* For operations on "type" field. */
	char		found_type;
	char		found_sub_type;


	if (envm_check() == DDI_FAILURE)
		return (0);

	/*
	 * extract the arguments
	 */

	/*
	 * If the slot bit is set get the slot number otherwise
	 * set up to get all slots
	 */
	if (key_mask.slot) {
		slot = *(short *)argp;
		slot_limit = slot + 1;
		argp += sizeof (int);
	} else {
		slot = 0;
		slot_limit = EISA_MAXSLOT;
	}
#ifdef DEBUG_EISA_NVM
	prom_printf("slot = %d  slot_limit = %d\n", slot, slot_limit);
#endif

	/*
	 * If the function bit is set get the function number otherwise
	 * set up to get all functions
	 */
	if (key_mask.function) {
		function_arg = *(short *)argp;
		argp += sizeof (int);
	}

	if (key_mask.board_id) {
		val  = *(unsigned *)argp,
		argp += sizeof (unsigned int);
		mask = *(unsigned *)argp;
		argp += sizeof (unsigned int);
#ifdef DEBUG_EISA_NVM
		prom_printf("val = %d  mask = %d\n", val, mask);
#endif
	}

	if (key_mask.revision) {
		revision = *(unsigned *)argp;
		argp += sizeof (unsigned int);
#ifdef DEBUG_EISA_NVM
		prom_printf("revision = %d\n", revision);
#endif
	}

	if (key_mask.checksum) {
		checksum = *(unsigned *)argp;
		argp += sizeof (unsigned int);
#ifdef DEBUG_EISA_NVM
		prom_printf("checksum = %d\n", checksum);
#endif
	}

	if (key_mask.type) {
		type_arg = *(char **)argp;
		argp += sizeof (char *);
#ifdef DEBUG_EISA_NVM
		prom_printf("type_arg = %s\n", type_arg);
#endif
	}

	if (key_mask.sub_type) {
		sub_type_arg = *(char **)argp;
		argp += sizeof (char *);
#ifdef DEBUG_EISA_NVM
		prom_printf("sub_type_arg = %s\n", sub_type_arg);
#endif
	}

	/* Searches through the required slots. */

	for (; slot < slot_limit; slot++) {
		new_slot = data;

		if (eisa_read_slot(slot, (char *)&slot_info) == 0) {
			/* Handles request for a specific function. */
			/* Checks the slot-related keys. */

			if (key_mask.board_id) {
				if ((*(unsigned int *)slot_info.boardid &
				    mask) != (val & mask))
					continue;
			}

			if (key_mask.revision) {
				if (slot_info.revision != revision)
					continue;
			}

			if (key_mask.checksum) {
				if (slot_info.checksum != checksum)
					continue;
			}

			/* Searches through the required functions. */
			/*
			 * If the function bit is set get the function number
			 * otherwise set up to get all functions
			 */
			if (key_mask.function) {
				function = function_arg;
				func_limit = function + 1;
			} else {
				function = 0;
				func_limit = slot_info.functions;
			}

#ifdef DEBUG_EISA_NVM
			prom_printf("function = %d  func_limit = %d\n",
			    function, func_limit);
#endif
			for (; function < func_limit; function++) {
				if (eisa_read_func(slot, function,
				    (char *)&func_info) == 0) {
					/* Checks the function-related keys. */

					type = (char *)func_info.type;
					found_type = FALSE;
					found_sub_type = FALSE;
					if (key_mask.type) {
						/* Searches for the type */
						/* string specified. */
						len = strlen(type_arg);
						while (*type &&
							*type != ';' &&
							type <
							(char *)func_info.type +
						    sizeof (func_info.type) &&
						    found_type == FALSE) {
							if (strncmp(type,
							    type_arg, len) == 0)
								found_type
								    = TRUE;
							else
								type++;
						}
						if (found_type == FALSE)
							/*
							 * Failed to match
							 * the requested type.
							 * try the next
							 * function
							 */
							continue;
					}

					/*
					 * skip over the type info
					 * skip until ';' NULL or end of
					 * string array
					 */
					while (*type && *type != ';' &&
					    type < (char *)func_info.type +
					    sizeof (func_info.type))
						type++;

					/*
					 * See if we're pointing to a ';'.
					 * If so, skip over it ... and then
					 * see if we need to check for a
					 * subtype.
					 */
					if (*type++ == ';' &&
					    key_mask.sub_type) {
						/*  Searches for the sub-type */
						/* string specified. */
						len = strlen(sub_type_arg);
						while (*type && *type != ';' &&
						    type <
							(char *)func_info.type +
						    sizeof (func_info.type) &&
						    found_sub_type == FALSE) {
							if (strncmp(type,
							    sub_type_arg,
							    len) == 0)
								found_sub_type
								    = TRUE;
							else
								type++;
						}
						if (found_sub_type == FALSE)
							/*
							 * Failed to match the
							 * requested sub_type.
							 * try the next
							 * function
							 */
							continue;
					}

					/*
					 * At this point, any/all keys have
					 * been matched so copies the slot
					 * structure (if not already copied)
					 * and function structure into the
					 * caller's data area.
					*/

					if (data == new_slot) {
						*((short *)data) = slot;
						data += sizeof (short);
						new_slot = data;
						bcopy((char *)&slot_info, data,
						    sizeof (NVM_SLOTINFO));
						((NVM_SLOTINFO *)new_slot)
						    ->functions = 0;
						data += sizeof (NVM_SLOTINFO);
					}

					bcopy((char *)&func_info, data,
					    sizeof (NVM_FUNCINFO));
					((NVM_SLOTINFO *)new_slot)->functions++;
					data += sizeof (NVM_FUNCINFO);

				}   /* end eisa_read_func */
			}   /* end for all functions */
		}   /* end eisa_read_slot */
	}   /* end for all slots */
	return ((int)(data - data_start));
}

static int
eisa_getnvm(register regs *rp)
{
	int	slot;
	int	rc;

#ifdef KEEP_INT15
/*
 * The eisa_rom_call code is not ported but exists in 5.3 code (rom_call.s)
 * There does't seem to be much point porting it it the property stuff
 * works.
 */
/*	check for support for rom bios call init 15			*/
	if (eisa_int15)
		return (eisa_rom_call(rp));

#endif
	slot = rp->ecx.byte.cl;
	if (slot >= EISA_MAXSLOT) {
		return (rp->eax.byte.ah = NVM_INVALID_SLOT);
	}

	switch (rp->eax.word.ax) {
	case EISA_READ_SLOT_CONFIG:
		rc = eisa_getslot(rp);
		break;
	case EISA_READ_FUNC_CONFIG:
		rc = eisa_getfunc(rp);
		break;
	default:
		rc = 1;
		break;
	}
	return (rc);
}

static int
eisa_getslot(register regs *rp)
{
	register struct es_slot *es_slotp = (struct es_slot *)eisa_nvmp;

	es_slotp += rp->ecx.byte.cl;
	rp->eax.word.ax = es_slotp->es_slotinfo.eax.word.ax;
	rp->ebx.word.bx = es_slotp->es_slotinfo.ebx.word.bx;
	rp->ecx.word.cx = es_slotp->es_slotinfo.ecx.word.cx;
	rp->edx.word.dx = es_slotp->es_slotinfo.edx.word.dx;
	rp->esi.word.si = es_slotp->es_slotinfo.esi.word.si;
	rp->edi.word.di = es_slotp->es_slotinfo.edi.word.di;
	return ((int)es_slotp->es_slotinfo.eax.byte.ah);

}

static int
eisa_getfunc(register regs *rp)
{
	register struct es_func *es_funcp;
	register struct es_slot *es_slotp = (struct es_slot *)eisa_nvmp;
	uint    func;

	es_slotp += rp->ecx.byte.cl;
	if (!es_slotp->es_funcoffset) {
		return (rp->eax.byte.ah = NVM_EMPTY_SLOT);
	}

	func = (uint)rp->ecx.byte.ch;
	if (func >= es_slotp->es_slotinfo.edx.byte.dh) {
		return (rp->eax.byte.ah = NVM_INVALID_FUNCTION);
	}

	es_funcp = (struct es_func *)(eisa_nvmp + es_slotp->es_funcoffset);
	es_funcp += func;
	rp->eax.word.ax = es_funcp->eax.word.ax;
	bcopy((caddr_t)es_funcp->ef_buf, (caddr_t)rp->esi.esi, EFBUFSZ);
	return ((int)es_funcp->eax.byte.ah);
}


/*
 * check if eisa machine
 */

int
envm_check(void)
{
	if (eisa_nvmlength != 0)	/* we have EISA data */
		return (DDI_SUCCESS);
	return (DDI_FAILURE);
}

#ifdef KEEP_INT15
eisa_enable_int15()
{
	eisa_int15 = 1;
}

eisa_disable_int15()
{
	eisa_int15 = 0;
}
#endif

/*
 * Referenced in common/cpr_driver.c: Power off machine.
 * Don't know how to power off i86pc.
 */
void
arch_power_down()
{
}
