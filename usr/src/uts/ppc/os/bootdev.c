/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bootdev.c	1.4	96/05/22 SMI"

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/modctl.h>
#include <sys/promif.h>
#include <sys/debug.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/esunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/pathname.h>
#include <sys/autoconf.h>

/*
 * internal functions
 */
static void parse_name(char *, char **, char **, char **);
static int validate_dip(dev_info_t *dip, char *addr, int hold);
static dev_info_t *srch_child_addrs(dev_info_t *parent_dip, char *addr,
	major_t maj);
static dev_info_t *srch_child_names(dev_info_t *parent_dip, char *nodename,
	char *addr);
static void clone_dev_fix(dev_info_t *dip, char *curpath);
static void handle_naming_exceptions(char *component);
static major_t path_to_major(char *path, char *leaf_name);
static void lock_driver_list(major_t);
static void unlock_driver_list(major_t);

/* External function prototypes */
extern char *i_binding_to_drv_name(char *);

static struct modlmisc modlmisc = {
	&mod_miscops, "bootdev misc module"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

_init()
{

	return (mod_install(&modlinkage));
}

_fini()
{
	/*
	 * misc modules are not safely unloadable: 1170668
	 */
	return (EBUSY);
}

_info(modinfop)
	struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * break device@a,b:minor into components
 */
static void
parse_name(char *name, char **drvname, char **addrname, char **minorname)
{
	register char *cp, ch;
	static char nulladdrname[] = ":\0";

	cp = *drvname = name;
	*addrname = *minorname = NULL;
	while ((ch = *cp) != '\0') {
		if (ch == '@')
			*addrname = ++cp;
		else if (ch == ':')
			*minorname = ++cp;
		++cp;
	}
	if (!*addrname)
		*addrname = &nulladdrname[1];
	*((*addrname)-1) = '\0';
	if (*minorname)
		*((*minorname)-1) = '\0';
}

/*
 * deal with two explicit conversions which are also
 * performed by the boot program before passing the
 * bootpath to the kernel:
 *	COMS,3C509 ---> elx@0,0:elx0
 *	ide ---> ata
 */
static void
handle_naming_exceptions(char *component)
{
	char *promver1 = "COMS,3C509@";
	char *promver2 = "ide@";
	char *devfsver1 = "elx@0,0:elx0";
	char *devfsver2 = "ata@";

	if (component == NULL) {
		return;
	}

	if (strncmp(component, promver1, strlen(promver1)) == 0) {
		strcpy(component, devfsver1);
		return;
	}
	if (strncmp(component, promver2, strlen(promver2)) == 0) {
		while (*devfsver2 != '\0') {
			*component = *devfsver2;
			component++;
			devfsver2++;
		}
		while (*component != '\0') {
			component++;
			*component = *(component + 1);
		}
		strcat(component, ",0");
		return;
	}
}

/*
 * convert a prom device path to an equivalent path in /devices
 * Does not deal with aliases.  Does deal with pathnames which
 * are not fully qualified.
 */
int
i_promname_to_devname(char *prom_name, char *ret_buf)
{
	dev_info_t *parent_dip, *child_dip;
	char *sub_path;
	char *prom_sub_path;
	struct pathname pn;
	char *ptr;
	char *ua;
	char *component;
	char *unit_address;
	char *minorname;
	char *nodename;
	major_t child_maj, parent_maj;

	parent_maj = (major_t)-1;

	if (prom_name == NULL) {
		return (EINVAL);
	}
	if (ret_buf == NULL) {
		return (EINVAL);
	}
	if (strlen(prom_name) >= MAXPATHLEN) {
		return (EINVAL);
	}

	if (*prom_name != '/') {
		return (EINVAL);
	}

	ptr = strchr(prom_name, ':');
	if ((ptr != NULL) && (strchr(ptr, '/'))) {
		return (EINVAL);
	}

	/* allocate some buffers */
	sub_path = kmem_alloc(MAXPATHLEN, KM_SLEEP);

	prom_sub_path = kmem_alloc(MAXPATHLEN, KM_SLEEP);

	component = kmem_alloc(MAXNAMELEN, KM_SLEEP);

	ua = kmem_alloc(MAXNAMELEN, KM_SLEEP);

	/* time to walk to tree */
	if (pn_get(prom_name, UIO_SYSSPACE, &pn)) {
		kmem_free(sub_path, MAXPATHLEN);
		kmem_free(prom_sub_path, MAXPATHLEN);
		kmem_free(component, MAXNAMELEN);
		kmem_free(ua, MAXNAMELEN);
		return (EIO);
	}

	pn_skipslash(&pn);
	parent_dip = ddi_root_node();
	sub_path[0] = '\0';
	prom_sub_path[0] = '\0';

	/*
	 * walk the device tree attempting to find a node in the
	 * tree for each component in the path
	 */
	while (pn_pathleft(&pn)) {
		(void) pn_getcomponent(&pn, component);
		handle_naming_exceptions(component);
		strcat(prom_sub_path, "/");
		strcat(prom_sub_path, component);
		parse_name(component, &nodename, &unit_address, &minorname);

		if ((unit_address == NULL) || (*unit_address == '\0')) {
			/*
			 * if the path component has no unit address info,
			 * i.e. a name that is not fully qualified, we
			 * search through the list of children comparing
			 * names.
			 * srch_child_names fills in the correct unit address
			 * info for us if it exists.
			 */
			unit_address = ua;
			child_dip = srch_child_names(parent_dip, nodename,
			    unit_address);
		} else {
			/*
			 * we have the unit address information so we can
			 * just search through the children looking for a
			 * matching address.
			 */
			if ((child_maj = path_to_major(prom_sub_path, nodename))
			    != (major_t)-1) {
				child_dip = srch_child_addrs(parent_dip,
				    unit_address, child_maj);
			} else {
				child_dip = NULL;
			}
		}
		/* did we find a corresponding dip for the path component? */
		if (child_dip == NULL) {
			if (parent_maj != (major_t)-1) {
				ddi_rele_driver(parent_maj);
			}
			kmem_free(sub_path, MAXPATHLEN);
			kmem_free(prom_sub_path, MAXPATHLEN);
			kmem_free(component, MAXNAMELEN);
			kmem_free(ua, MAXNAMELEN);
			return (EINVAL);
		}

		child_maj = ddi_name_to_major(ddi_binding_name(child_dip));
		ASSERT(child_maj != (major_t)-1);

		/* continue building the devfs path */
		(void) strcat(sub_path, "/");
		(void) strcat(sub_path, ddi_node_name(child_dip));
		if ((unit_address != NULL) && (*unit_address != '\0')) {
			(void) strcat(sub_path, "@");
			(void) strcat(sub_path, unit_address);
		}
		if ((minorname != NULL) && (*minorname != '\0')) {
			(void) strcat(sub_path, ":");
			(void) strcat(sub_path, minorname);
		}
		/* move on to the next component */
		if (parent_maj != (major_t)-1) {
			ddi_rele_driver(parent_maj);
		}
		parent_maj = child_maj;
		parent_dip = child_dip;
		pn_skipslash(&pn);
	}
	/*
	 * if we get here, every component in the path matched a node
	 * in the devinfo tree.  So we have a valid path.
	 *
	 * if we matched to a clone device, modify the path to reflect the
	 * correct clone device node and finish up.
	 */
	clone_dev_fix(child_dip, sub_path);
	ddi_rele_driver(child_maj);
	strcpy(ret_buf, sub_path);
	kmem_free(sub_path, MAXPATHLEN);
	kmem_free(prom_sub_path, MAXPATHLEN);
	kmem_free(component, MAXNAMELEN);
	kmem_free(ua, MAXNAMELEN);
	return (0);
}

/*
 * take a dip which does not have any external representation in
 * /devices because it is using the clone driver and convert it
 * to the proper clone device name.
 *
 * dip must be held.
 */
static void
clone_dev_fix(dev_info_t *dip, char *curpath)
{
	struct ddi_minor_data *dmn;
	struct ddi_minor_data *save_dmn = NULL;
	dev_info_t *clone_dip;
	major_t clone_major;
	char *clone_path;

	mutex_enter(&(DEVI(dip)->devi_lock));
	/*
	 * we will return if this dip does have some sort of external
	 * pathname representation in /devices. If we find only
	 * minor nodes of type, DDM_ALIAS, we have a clone device and
	 * will convert the name.
	 */
	for (dmn = DEVI(dip)->devi_minor; dmn; dmn = dmn->next) {
		if ((dmn->type == DDM_MINOR) ||
		    (dmn->type == DDM_DEFAULT)) {
			mutex_exit(&(DEVI(dip)->devi_lock));
			return;
		}
		if (dmn->type == DDM_ALIAS) {
			save_dmn = dmn;
		}
	}
	mutex_exit(&(DEVI(dip)->devi_lock));

	if (save_dmn == NULL) {
		return;
	}
	/* convert to the clone device representation */
	clone_major = ddi_name_to_major("clone");
	if (clone_major == (major_t)-1) {
		return;
	}
	if (ddi_hold_installed_driver(clone_major) == NULL) {
		return;
	}
	/* get the clone dip */
	clone_dip = ddi_find_devinfo("clone", -1, 1);
	if (clone_dip == NULL) {
		ddi_rele_driver(clone_major);
		return;
	}

	clone_path = kmem_alloc(MAXPATHLEN, KM_SLEEP);

	mutex_enter(&(DEVI(dip)->devi_lock));

	for (dmn = DEVI(dip)->devi_minor; dmn; dmn = dmn->next) {
		if (dmn == save_dmn) {
			break;
		}
	}

	if (dmn == NULL) {
		mutex_exit(&(DEVI(dip)->devi_lock));
		ddi_rele_driver(clone_major);
		kmem_free(clone_path, MAXPATHLEN);
		return;
	}

	/* build the pathname for the clone and append minor name info */
	if (ddi_pathname(clone_dip, clone_path) != NULL) {
		sprintf(curpath, "%s:%s", clone_path, save_dmn->ddm_aname);
	}
	mutex_exit(&(DEVI(dip)->devi_lock));
	ddi_rele_driver(clone_major);
	kmem_free(clone_path, MAXPATHLEN);
}

/*
 * search for 'name' in all of the children of 'dip'.
 * also return the correct address information in 'addr'
 *
 * The assumption here is that we have do not have an address to
 * compare with...so if we find more than one child with nodename,
 * we fail, since there is no way to determine which path we
 * should follow.
 *
 * since we are called in order to convert from a prom name
 * to a devfs name, we have to handle some special cases here.
 *
 * the parent dip, dip, must be held by the caller.
 *
 * the dip returned is held.
 */
static dev_info_t *
srch_child_names(dev_info_t *parent_dip, char *nodename, char *addr)
{
	char *ptr;
	dev_info_t *child_dip;
	dev_info_t *save_dip = NULL;
	int unique = 1;

	/*
	 * look for a straight match... comparing against node name
	 */
	rw_enter(&(devinfo_tree_lock), RW_READER);
	for (child_dip = ddi_get_child(parent_dip);
	    child_dip != NULL;
	    child_dip = ddi_get_next_sibling(child_dip)) {

		if (strcmp(ddi_node_name(child_dip), nodename) == 0) {
			/* not unique, then fail */
			if (save_dip != NULL) {
				unique = 0;
				break;
			} else {
				save_dip = child_dip;
				unique = 1;
			}
		}
	}
	rw_exit(&(devinfo_tree_lock));

	/*
	 * we exited the loop for one of 3 reasons:
	 * 1. we found multiple instances of the same name - there is no
	 * we can accurately determine which of the instances is the
	 * correct one so we fail.
	 * 2. We found a single node that matches
	 * 3. We found no nodes that match
	 */

	if (!unique) {
		/* couldn't find a *unique* instance of the name */
		return (NULL);
	}
	if (save_dip != NULL) {
		/*
		 * if we found a single node that matched nodename.
		 * We might have found...
		 *
		 * 1. the exact match (the dip is valid)
		 * 2. a generic name(disk) that is specific in devfs(sd)
		 * (the dip we found is not valid(disk), but there may be
		 * an equivalent dip that was created from a .conf file).
		 * 3. an alias(SUNW,ssd) that is the driver name in devfs(ssd)
		 * (the dip we found is not valid(disk), but there may be
		 * an equivalent dip that was created from a .conf file).
		 */
		if (validate_dip(save_dip, addr, 1)) {
			/* a exact match */
			return (save_dip);
		}
		/*
		 * get the driver name and look for a single node with the
		 * driver name as the node name.
		 * for cases 2) and 3) above
		 */
		ptr = i_binding_to_drv_name(ddi_binding_name(save_dip));
	} else {
		/*
		 * no name match...could still be a valid name though...
		 * the prom may not have *needed* to create a node for this
		 * device if we did not boot from it.  If it is a device
		 * such as SUNW,ssd that has its devinfo nodes created from
		 * a .conf file (and prom node is discarded), we should check
		 * for the existence of a node with the driver name (ssd).
		 */
		ptr = i_binding_to_drv_name(nodename);
	}
	if (ptr == NULL)
		return (NULL);
	unique = 0;
	rw_enter(&(devinfo_tree_lock), RW_READER);
	for (child_dip = ddi_get_child(parent_dip);
	    child_dip != NULL;
	    child_dip = ddi_get_next_sibling(child_dip)) {
		if (strcmp(ddi_node_name(child_dip), ptr) == 0) {
			if (save_dip != NULL) {
				unique = 0;
				break;
			} else {
				unique = 1;
				save_dip = child_dip;
			}
		}
	}
	rw_exit(&(devinfo_tree_lock));
	/* could not find unique instance */
	if (!unique) {
		return (NULL);
	}
	/* nothing matched - quit */
	if (save_dip == NULL)
		return (NULL);

	/* we found something - let's see if its valid. */
	if (validate_dip(save_dip, addr, 1))
		return	(save_dip);

	return (NULL);
}

/*
 * valid means:
 *	- a driver can be loaded and attached to the instance of the
 * 		device this dip represents.
 * we also optionally return the address information for the node in addr
 * we are either passed a dip which is held and locked (hold = 0)
 * or we are asked to hold the driver and lock the dip (hold = 1)
 *
 * regardless, if successful, the driver for the dip will be returned held.
 */
static int
validate_dip(dev_info_t *dip, char *addr, int hold)
{
	major_t major_no;
	int ret = 0;
	char *ptr;

	if (hold) {
		major_no = ddi_name_to_major(ddi_binding_name(dip));
		if (major_no == (major_t)-1) {
			return (0);
		}
		if (ddi_hold_installed_driver(major_no) == NULL) {
			return (0);
		}
		(void) e_ddi_deferred_attach(major_no, NODEV);

		lock_driver_list(major_no);
	}
	if (DDI_CF2(dip)) {
		ret = 1;
		if (addr != NULL) {
			ptr = ddi_get_name_addr(dip);
			if (ptr == NULL) {
				*addr = '\0';
			} else {
				(void) strcpy(addr, ptr);
			}
		}
	}
	if (hold) {
		unlock_driver_list(major_no);
		if (ret == 0) {
			ddi_rele_driver(major_no);
		}
	}
	return (ret);
}


/*
 * search thru the children of 'dip' looking for an address that
 * matches 'addr'.
 * dip (parent dip) must be held.
 * major is the major number of the child we are looking for
 * addr is the address of the node we are looking for.
 *
 * the dip returned is held.
 */
static dev_info_t *
srch_child_addrs(dev_info_t *parent_dip, char *addr, major_t major)
{
	dev_info_t *child_dip;
	char *ptr;

	if (major == (major_t)-1) {
		return (NULL);
	}
	if (ddi_hold_installed_driver(major) == NULL)  {
		return (NULL);
	}
	(void) e_ddi_deferred_attach(major, NODEV);
	lock_driver_list(major);

	for (child_dip = devnamesp[major].dn_head; child_dip != NULL;
	    child_dip = ddi_get_next(child_dip))  {
		if (ddi_get_parent(child_dip) != parent_dip) {
			continue;
		}
		if (validate_dip(child_dip, NULL, 0) == 0) {
			continue;
		}
		ptr = ddi_get_name_addr(child_dip);
		if ((ptr != NULL) && (strcmp(ptr, addr) == 0)) {
			break;
		}
	}
	unlock_driver_list(major);
	if (child_dip == NULL) {
		ddi_rele_driver(major);
	}
	return (child_dip);
}
/*
 * attempt to convert a path to a major number
 */
static major_t
path_to_major(char *path, char *leaf_name)
{
	extern char *i_path_to_drv(char *pathname);
	char *binding;

	if ((binding = i_path_to_drv(path)) == NULL) {
		binding = leaf_name;
	}
	return (ddi_name_to_major(binding));
}

static void
lock_driver_list(major_t maj)
{
	struct devnames *dnp = &devnamesp[maj];

	ASSERT(DEV_OPS_HELD(devopsp[maj]));

	LOCK_DEV_OPS(&(dnp->dn_lock));
	while (DN_BUSY_CHANGING(dnp->dn_flags))
		cv_wait(&(dnp->dn_wait), &(dnp->dn_lock));
	dnp->dn_flags |= DN_BUSY_LOADING;
	dnp->dn_busy_thread = curthread;
	UNLOCK_DEV_OPS(&(dnp->dn_lock));
}
static void
unlock_driver_list(major_t maj)
{
	struct devnames *dnp = &devnamesp[maj];

	ASSERT(DEV_OPS_HELD(devopsp[maj]));

	LOCK_DEV_OPS(&(dnp->dn_lock));
	dnp->dn_flags &= ~(DN_BUSY_LOADING);
	dnp->dn_busy_thread = NULL;
	cv_broadcast(&(dnp->dn_wait));
	UNLOCK_DEV_OPS(&(dnp->dn_lock));
}
