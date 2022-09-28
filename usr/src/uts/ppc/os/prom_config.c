/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_config.c	1.4	96/07/02 SMI"

/*
 * Operations to deal with PROM properties
 */

/*
 * indicate that this is the implementation code.
 */
#define	SUNDDI_IMPL

#include <sys/types.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/thread.h>
#include <sys/byteorder.h>
#include <sys/bootconf.h>
#include <sys/promif.h>
#include <sys/prom_config.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

static prom_node_t *top_prom_node;

/*
 * Contains the instance-to-path translations for entries under the
 * /chosen node.  We have to save these away before releasing the
 * prom as there is no way to convert an instance to a pathname
 * after the prom is gone.
 */
static struct i2path {
	ihandle_t instance;
	char *path;
	struct i2path *next;
} *i2path_list = NULL;

static int prom_conf_stdin_is_keyboard;
static int prom_conf_stdout_is_framebuffer;
static char *prom_conf_prom_version_name;
static char *prom_conf_stdinpath;
static char *prom_conf_stdoutpath;

/*
 * hash lookups
 */
#define	BUCKETS		16
#define	HASH(X)	((((uint_t)(X)) >> 4) & (BUCKETS - 1))
static prom_node_t *prom_config_hash_table[BUCKETS];

/*
 * internal function prototypes
 */
static int copy_prom_node(prom_node_t *pnp);
static void prom_walk(prom_node_t *pnp, int (*f)());
static void create_prom_prop(prom_node_t *pnp, char *propname, char *propval,
    int proplen);
static ddi_prop_t *get_prom_prop(prom_node_t *pnp, char *name);
static dnode_t prom_find(prom_node_t *pnp, char *path);
static prom_node_t *find_node_by_name(prom_node_t *pnp, char *dname,
	char *daddr);
static void init_ihandle_to_path(void);
static char *init_prom_version_name();

/*
 * Allocate a new prom_node with the given nodeid. Add it to the hash
 * chain.
 */
static prom_node_t *
new_prom_node(dnode_t nodeid)
{
	prom_node_t *nodep;
	u_int	slot;
	int		len;
	char	scratch[OBP_MAXPATHLEN];
	char	*devname;
	char	*devaddr;

	nodep = kmem_zalloc(sizeof (prom_node_t), KM_SLEEP);
	nodep->pn_nodeid = nodeid;
	sprintf(scratch, "prom node %x lock", nodeid);
	mutex_init(&nodep->pn_lock, scratch, MUTEX_DEFAULT, NULL);
	sprintf(scratch, "prom node %x cv", nodeid);
	cv_init(&nodep->pn_cv, scratch, CV_DEFAULT, NULL);
	nodep->pn_owner = NULL;
	scratch[0] = '\0';
	len = prom_phandle_to_path((phandle_t)nodeid, scratch,
			OBP_MAXPATHLEN);
	/*
	 * Get the address part of the device name.
	 */
	if (len != 0) {
		devname = strrchr(scratch, '/');
		devname++;
		devaddr = strchr(devname, '@');
		if (devaddr != NULL) {
			devaddr++;
			nodep->pn_addr = kmem_zalloc(strlen(devaddr) + 1,
			    KM_SLEEP);
			(void) strcpy(nodep->pn_addr, devaddr);
		}
	}

	/* insert into hash table */
	slot = HASH(nodeid);
	nodep->pn_hashlink = prom_config_hash_table[slot];
	prom_config_hash_table[slot] = nodep;

	return (nodep);
}

/*
 * Return a pointer to the prom_node structure corresponding to
 * the given nodeid.
 */
static prom_node_t *
find_node_by_nodeid(dnode_t nodeid)
{
	u_int slot;
	prom_node_t *nodep;

	slot = HASH(nodeid);
	for (nodep = prom_config_hash_table[slot]; nodep != NULL;
	    nodep = nodep->pn_hashlink) {
		if (nodep->pn_nodeid == nodeid)
			return (nodep);
	}
	return (NULL);
}

/*
 * Initialize PROM configuarion locks.
 * Copy PROM device tree into memory.
 * Save away prom static data
 */
void
prom_config_setup(void)
{
	top_prom_node = new_prom_node(prom_nextnode((dnode_t)0));
	prom_walk(top_prom_node, copy_prom_node);
	init_ihandle_to_path();
	prom_conf_prom_version_name = init_prom_version_name();
	prom_conf_stdin_is_keyboard = prom_stdin_is_keyboard();
	prom_conf_stdout_is_framebuffer = prom_stdout_is_framebuffer();
	prom_conf_stdinpath = prom_stdinpath();
	prom_conf_stdoutpath = prom_stdoutpath();
}


static void
prom_walk(prom_node_t *pnp, int (*f)())
{
	(*f)(pnp);
	if (pnp) {
		prom_walk(pnp->pn_child, f);
		prom_walk(pnp->pn_sibling, f);
	}
}

/*
 * Fill in a node of the in-kernel PROM tree. Make copies of all the
 * PROM properties for the passed-in node, and create necessary
 * children and siblings.
 */
/*ARGSUSED1*/
static int
copy_prom_node(prom_node_t *pnp)
{
	int	proplen;
	char propname[OBP_MAXPROPNAME+1], nextprop[OBP_MAXPROPNAME+1];
	char *propval;
	char *next;
	dnode_t	nid;
	dnode_t snid, cnid;

	if (pnp == NULL)
		return (DDI_WALK_CONTINUE);
	nid = pnp->pn_nodeid;
	propname[0] = '\0';
	for (;;) {
		/*
		 * For obp, at least, we have to look at the returned
		 * value of prom_getprop().
		 */
		next = prom_nextprop(nid, propname, nextprop);
		if (next == NULL || next[0] == '\0')
			break;
		(void) strcpy(propname, next);
		proplen = prom_getproplen(nid, propname);
		/*
		 * The PROM really shouldn't tell us a property is there, but
		 * then fail prom_getproplen().
		 */
		if (proplen == -1) {
			cmn_err(CE_WARN,
			    "ignoring malformed property \"%s\" on "
			    "PROM node (id 0x%x).",
			    propname, nid);
			continue;
		}
		if (proplen != 0) {
			propval = kmem_alloc(proplen, KM_SLEEP);
			prom_getprop(nid, propname, propval);
		} else {
			propval = NULL;
		}
		create_prom_prop(pnp, propname, propval, proplen);
		if (propval != NULL)
			kmem_free(propval, proplen);
	}

	snid = prom_nextnode(nid);
	if (snid && (snid != (dnode_t)-1)) {
		/* Make a sibling */
		pnp->pn_sibling = new_prom_node(snid);
	}

	cnid = prom_childnode(nid);
	if (cnid && (cnid != (dnode_t)-1)) {
		/* Make a child */
		pnp->pn_child = new_prom_node(cnid);
	}

	return (DDI_WALK_CONTINUE);
}

/*
 * Called when a client (like a property search, or the openprom driver)
 * wants to start looking at configuration data from the PROM. Returns a
 * 'prom_config_handle' which is used in subsequent requests.
 */
int
prom_config_begin(prom_config_handle_t *handlep, dnode_t nodeid)
{
	prom_node_t *pnp;

	if (top_prom_node == NULL) {
		/* not yet copied tree */
		*handlep = (prom_config_handle_t *)nodeid;
		return (DDI_SUCCESS);
	}
	if (nodeid != 0) {
		/*
		 * Find the node. This also validates the node id, as we
		 * do not just believe what the client passed in.
		 */
		pnp = find_node_by_nodeid(nodeid);
		if (pnp == NULL)
			return (DDI_FAILURE);
	} else {
		/* node id 0 means the top */
		pnp = top_prom_node;
	}
	*handlep = (prom_config_handle_t *)pnp;

	/* single-thread access to prom config data */
	ASSERT(pnp->pn_owner != curthread);
	mutex_enter(&pnp->pn_lock);
	while (pnp->pn_owner != NULL) {
		cv_wait(&pnp->pn_cv, &pnp->pn_lock);
	}
	pnp->pn_owner = curthread;
	mutex_exit(&pnp->pn_lock);

	return (DDI_SUCCESS);
}


/*
 * Called when the client is done looking at PROM configuration data.
 */
void
prom_config_end(prom_config_handle_t handle)
{
	prom_node_t *pnp = (prom_node_t *)handle;

	if (top_prom_node == NULL) {
		/* nothing to do */
		return;
	}
	ASSERT(pnp->pn_owner == curthread);
	mutex_enter(&pnp->pn_lock);
	pnp->pn_owner = NULL;
	cv_signal(&pnp->pn_cv);
	mutex_exit(&pnp->pn_lock);
}

/*
 * PROM config routines. These pretty much match their promif counterparts.
 */

int
prom_config_getproplen(prom_config_handle_t handle, char *name)
{
	ddi_prop_t *propp;
	prom_node_t *pnp;
	int	len;

	if (top_prom_node == NULL)
		return (prom_getproplen((dnode_t)handle, name));
	pnp = (prom_node_t *)handle;
	propp = get_prom_prop(pnp, name);
	if (propp == NULL) {
		return (-1);
	}
	len = propp->prop_len;
	return (len);
}

int
prom_config_getprop(prom_config_handle_t handle, char *name, char *buffer)
{
	ddi_prop_t *propp;
	prom_node_t *pnp;
	int	len;

	if (top_prom_node == NULL)
		return (prom_getprop((dnode_t)handle, name, buffer));

	pnp = (prom_node_t *)handle;
	propp = get_prom_prop(pnp, name);
	if (propp == NULL) {
		return (-1);
	}
	len = propp->prop_len;
	bcopy(propp->prop_val, buffer, len);
	return (len);
}

/*
 * Update a PROM property.
 * Most PROM properties never change, but the openeepr driver might
 * change the 'options' properties because of an eeprom(1M) command.
 */
int
prom_config_setprop(prom_config_handle_t handle, char *propname,
    char *propval, int proplen)
{
	prom_node_t *pnp = (prom_node_t *)handle;
	ddi_prop_t *propp;

	if (top_prom_node == NULL) {
		return (prom_setprop((dnode_t)handle, propname, propval,
		    proplen));
	}

	propp = get_prom_prop(pnp, propname);
	if (propp != NULL) {
		/* update existing property */
		if (propp->prop_len != 0)
			kmem_free(propp->prop_val, propp->prop_len);
		if (proplen != 0) {
			propp->prop_val = kmem_alloc(proplen, KM_SLEEP);
			bcopy(propval, propp->prop_val, proplen);
		} else
			propp->prop_val = NULL;
		propp->prop_len = proplen;
		return (proplen);
	}
	/* new one */
	create_prom_prop(pnp, propname, propval, proplen);
	return (proplen);
}
int
prom_config_version_name(char *buf, int buflen)
{
	prom_config_handle_t pch;
	int len;

	if (prom_config_begin(&pch, 0) != DDI_SUCCESS) {
		return (-1);
	}
	*buf = '\0';
	*(buf + buflen - 1) = '\0';

	if (top_prom_node == NULL) {
		len = prom_version_name(buf, buflen);
	} else if (prom_conf_prom_version_name == NULL) {
		len = -1;
	} else {
		(void) strncpy(buf, prom_conf_prom_version_name, buflen - 1);
		len = strlen(prom_conf_prom_version_name);
	}
	prom_config_end(pch);
	return (len);
}
char *
prom_config_nextprop(prom_config_handle_t handle, char *previous, char *next)
{
	ddi_prop_t *propp;
	prom_node_t *pnp = (prom_node_t *)handle;

	if (top_prom_node == NULL) {
		return (prom_nextprop((dnode_t)handle, previous, next));
	}

	*next = '\0';
	if (*previous == '\0') {
		propp = pnp->pn_propp;
		if (propp != NULL)
			(void) strcpy(next, propp->prop_name);
		return (next);
	}
	propp = get_prom_prop(pnp, previous);
	if (propp == NULL) {
		return (next);
	}
	if (propp->prop_next != NULL) {
		(void) strcpy(next, propp->prop_next->prop_name);
	}
	return (next);
}

dnode_t
prom_config_childnode(prom_config_handle_t handle)
{
	prom_node_t *pnp = (prom_node_t *)handle;

	if (top_prom_node == NULL) {
		return (prom_childnode((dnode_t)handle));
	}
	if (pnp == NULL)
		return (0);
	if (pnp->pn_child == NULL)
		return (0);
	return (pnp->pn_child->pn_nodeid);
}

dnode_t
prom_config_nextnode(prom_config_handle_t handle)
{
	prom_node_t *pnp = (prom_node_t *)handle;

	if (top_prom_node == NULL) {
		return (prom_nextnode((dnode_t)handle));
	}
	if (pnp == NULL)
		return (0);
	if (pnp->pn_sibling == NULL)
		return (0);
	return (pnp->pn_sibling->pn_nodeid);
}

/*ARGSUSED*/
dnode_t
prom_config_topnode(prom_config_handle_t handle)
{
	return (top_prom_node->pn_nodeid);
}

/*
 * Create a PROM property on the in-kernel PROM device tree.
 * We cannot use the normal routines because they now encode, and
 * we don't want that to happen. They also stack properties, and
 * we really should maintain the PROM's order.
 */
static void
create_prom_prop(prom_node_t *pnp, char *propname, char *propval, int proplen)
{
	int namelen;
	ddi_prop_t *next, *current;

	current = kmem_alloc(sizeof (ddi_prop_t), KM_SLEEP);
	current->prop_next = NULL;
	current->prop_dev = DDI_DEV_T_NONE;
	current->prop_flags = 0;
	namelen = strlen(propname);
	current->prop_name = kmem_alloc(namelen + 1, KM_SLEEP);
	(void) strcpy(current->prop_name, propname);
	if (proplen != 0) {
		current->prop_val = kmem_alloc(proplen, KM_SLEEP);
		bcopy(propval, current->prop_val, proplen);
	} else
		current->prop_val = NULL;
	current->prop_len = proplen;
	if (pnp->pn_propp == NULL) {
		pnp->pn_propp = current;
	} else {
		/* preserve PROM order */
		for (next = pnp->pn_propp; next->prop_next != NULL;
		    next = next->prop_next) {
			/* NULL */
		}
		next->prop_next = current;
	}
}

/*
 * Retrieve a PROM property
 */
static ddi_prop_t *
get_prom_prop(prom_node_t *pnp, char *name)
{
	ddi_prop_t	*propp;

	for (propp = pnp->pn_propp; propp != NULL; propp = propp->prop_next) {
		if (strcmp(propp->prop_name, name) == 0)
			break;
	}
	return (propp);
}

char *
prom_config_stdinpath(void)
{
	prom_config_handle_t handle;
	char *ret;

	if (prom_config_begin(&handle, 0) != DDI_SUCCESS) {
		return (NULL);
	}
	if (top_prom_node == NULL) {
		ret = prom_stdinpath();
	} else {
		ret = prom_conf_stdinpath;
	}
	prom_config_end(handle);
	return (ret);
}
char *
prom_config_stdoutpath(void)
{
	prom_config_handle_t handle;
	char *ret;

	if (prom_config_begin(&handle, 0) != DDI_SUCCESS) {
		return (NULL);
	}
	if (top_prom_node == NULL) {
		ret = prom_stdoutpath();
	} else {
		ret = prom_conf_stdoutpath;
	}
	prom_config_end(handle);
	return (ret);
}
int
prom_config_stdout_is_framebuffer(void)
{
	prom_config_handle_t handle;
	int ret;

	if (prom_config_begin(&handle, 0) != DDI_SUCCESS) {
		return (0);
	}
	if (top_prom_node == NULL) {
		ret = prom_stdout_is_framebuffer();
	} else {
		ret = prom_conf_stdout_is_framebuffer;
	}
	prom_config_end(handle);
	return (ret);
}
int
prom_config_stdin_is_keyboard(void)
{
	prom_config_handle_t handle;
	int ret;

	if (prom_config_begin(&handle, 0) != DDI_SUCCESS) {
		return (0);
	}
	if (top_prom_node == NULL) {
		ret = prom_stdin_is_keyboard();
	} else {
		ret = prom_conf_stdin_is_keyboard;
	}
	prom_config_end(handle);
	return (ret);
}

dnode_t
prom_config_finddevice(char *name)
{
	char *path;
	prom_config_handle_t handle;
	dnode_t ret;
	prom_node_t *pnp;
	size_t len;

	if ((name == NULL) || (*name == '\0') ||
	    (*name != '/')) {
		return (OBP_BADNODE);
	}

	if (prom_config_begin(&handle, 0) != DDI_SUCCESS) {
		return (OBP_BADNODE);
	}
	pnp = (prom_node_t *)handle;

	if (top_prom_node == NULL) {
		ret = prom_finddevice(name);
	} else {
		len = strlen(name) + 1;
		if ((path = kmem_alloc(len, KM_NOSLEEP)) == NULL) {
			prom_config_end(handle);
			return (OBP_BADNODE);
		}
		(void) strcpy(path, name);
		ret = prom_find(pnp, path);
		kmem_free(path, len);
	}
	prom_config_end(handle);
	return (ret);
}

/*
 * note: this private routine assumes it can mangle the content of 'path'
 */
static dnode_t
prom_find(prom_node_t *pnp, char *path)
{
	char *dname;
	char *daddr;

	ASSERT((path != NULL) && (*path == '/'));

	while (path != NULL) {
		path++;
		dname = path;
		if ((path = strchr(path, '/')) != NULL) {
			*path = '\0';
		}
		daddr = strchr(dname, '@');
		if (daddr != NULL) {
			*daddr = '\0';
			daddr++;
		}
		if ((pnp = find_node_by_name(pnp->pn_child, dname, daddr))
		    == (prom_node_t *)OBP_BADNODE) {
			return (OBP_BADNODE);
		}
	}
	return (pnp->pn_nodeid);
}

/*
 * search through a list of siblings looking for a matching name and
 * unit address.
 */
static prom_node_t *
find_node_by_name(prom_node_t *pnp, char *dname, char *daddr)
{

	ddi_prop_t *p;
	prom_node_t *possible_match = NULL;

	if (dname == NULL) {
		return ((prom_node_t *)OBP_BADNODE);
	}

	while (pnp != NULL) {
		p = get_prom_prop(pnp, "name");
		if (p == NULL) {
			return ((prom_node_t *)OBP_BADNODE);
		}
		if (strcmp(p->prop_val, dname) != 0) {
			/*
			 * not a match - move on to next sibling
			 */
			pnp = pnp->pn_sibling;
			continue;
		}

		/*
		 * we may be passed a name that is not fully qualified.
		 * If so and we find only one sibling with the same node
		 * name, we will return a match.
		 */
		if (daddr == NULL && pnp->pn_addr != NULL) {
			if (possible_match == NULL) {
				possible_match = pnp;
			} else {
				return ((prom_node_t *)OBP_BADNODE);
			}
		} else if (daddr != NULL && pnp->pn_addr == NULL) {
			return ((prom_node_t *)OBP_BADNODE);
		} else if ((daddr == NULL && pnp->pn_addr == NULL) ||
		    (strcmp(pnp->pn_addr, daddr) == 0)) {
			/*
			 * we found a match
			 */
			return (pnp);
		}
		pnp = pnp->pn_sibling;
	}
	if (possible_match != NULL) {
		return (possible_match);
	} else {
		return ((prom_node_t *)OBP_BADNODE);
	}
}

/*
 * convert an instance to a path - use the information that was
 * saved away by init_ihandle_to_path() earlier
 */
int
prom_config_ihandle_to_path(ihandle_t ihandle, char *buf, u_int len)
{
	struct i2path *entry;
	prom_config_handle_t handle;

	if (prom_config_begin(&handle, 0) != DDI_SUCCESS) {
		return (-1);
	}

	if (top_prom_node == NULL) {
		return (prom_ihandle_to_path(ihandle, buf, len));
	}

	for (entry = i2path_list; entry != NULL;
	    entry = entry->next) {
		if (entry->instance == ihandle) {
			if (entry->path == NULL) {
				return (-1);
			}
			(void) strncpy(buf, entry->path, len);
			return (strlen(entry->path));
		}
	}
	prom_config_end(handle);
	return (-1);
}

static char *
init_prom_version_name()
{
	char buf[MAXNAMELEN];
	int vers_size;
	char *vers;

	vers_size = prom_version_name(buf, MAXNAMELEN);

	if (vers_size < 0) {
		cmn_err(CE_WARN, "prom_config: unable to read PROM version.");
		return (NULL);
	}

	vers = kmem_alloc(vers_size, KM_SLEEP);

	(void) strncpy(vers, buf, vers_size);
	return (vers);
}

/*
 * must be called before the prom is released.
 * saves away the instance-to-path conversion information for
 * the chosen node.
 */
static void
init_ihandle_to_path(void)
{
	char *chosen_list[] = {"stdin", "stdout", "memory", "mmu", NULL};
	dnode_t chosen;
	int i;
	int len;
	struct i2path *entry;
	char path[OBP_MAXPATHLEN];
	ihandle_t buf;

	/* get the chosen node */
	chosen = prom_chosennode();

	if ((chosen == OBP_NONODE) || (chosen == OBP_BADNODE)) {
		return;
	}

	/*
	 * loop thru the list of properties under the /chosen
	 * node to be saved, and save each one.
	 */
	for (i = 0; chosen_list[i] != NULL; i++) {
		len = prom_getproplen(chosen, chosen_list[i]);
		if ((len == -1) || (len == 0)) {
			continue;
		}
		ASSERT(len == sizeof (ihandle_t));

		/* allocate a new i2path struct */
		entry = kmem_alloc(sizeof (struct i2path), KM_SLEEP);

		/* look up an entry in the chosen node */
		(void) prom_getprop(chosen, chosen_list[i], (char *)&buf);

		/* save the instance number */
		entry->instance = (ihandle_t)htonl((unsigned long)buf);

		/* convert to a path and save if successful */
		if (prom_ihandle_to_path(entry->instance, path,
		    OBP_MAXPATHLEN - 1) > 0) {
			entry->path = kmem_alloc((strlen(path) + 1), KM_SLEEP);
			(void) strcpy(entry->path, path);
		} else {
			entry->path = NULL;
		}
		/* link into list */
		entry->next = i2path_list;
		i2path_list = entry;
	}
}
