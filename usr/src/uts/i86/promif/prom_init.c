/*
 * Copyright (c) 1991-1996 by Sun Microsystems, Inc.
 */

#pragma ident "@(#)prom_init.c	1.12	96/06/19 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/prom_emul.h>
#include <sys/bootconf.h>
#include <sys/obpdefs.h>
#include <sys/kmem.h>

extern int prom_is_p1275(void);

int	promif_debug = 0;	/* debug */
int emul_1275 = 0;
prom_node_t *top_prom_node;

/*
 *  Every standalone that wants to use this library must call
 *  prom_init() before any of the other routines can be called.
 *  Copy PROM device tree into memory.
 */
/*ARGSUSED*/
void
prom_init(char *pgmname, void *cookie)
{
#if !defined(KADB) && !defined(I386BOOT)
	/*
	 * Look for the 1275 property 'bootpath' here. If it exists
	 * and has a non-NULL value we need to assimilate the device
	 * tree bootconf has constructed.  Otherwise we do things the
	 * old way.
	 */
	emul_1275 = (BOP_GETPROPLEN(bootops, "bootpath") > 1);
#endif
}

#if !defined(KADB) && !defined(I386BOOT)

static int copy_prom_node(prom_node_t *pnp);
static void prom_walk(prom_node_t *pnp, int (*f)());

void
prom_setup()
{
	phandle_t ph;

	if (!prom_is_p1275())
		return;
	top_prom_node = (prom_node_t *)kmem_zalloc(sizeof (struct prom_node),
		KM_SLEEP);
	ph = BOP1275_PEER(bootops, NULL);
	top_prom_node->pn_propp = (struct prom_prop *)ph;
	prom_walk(top_prom_node, copy_prom_node);
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
 * Create a PROM property on the in-kernel PROM device tree.
 * We cannot use the normal routines because they now encode, and
 * we don't want that to happen. They also stack properties, and
 * we really should maintain the PROM's order.
 */
static void
create_prom_prop(prom_node_t *pnp, char *name, char *val, int len)
{
	int namelen;
	struct prom_prop *next, *current;

	current = (struct prom_prop *)kmem_zalloc(sizeof (struct prom_prop),
		KM_SLEEP);
	current->pp_next = NULL;
	namelen = prom_strlen(name);
	current->pp_name = (char *)kmem_zalloc(namelen + 1, KM_SLEEP);
	(void) prom_strcpy(current->pp_name, name);
	if (len != 0)
		current->pp_val = val;
	else
		current->pp_val = NULL;
	current->pp_len = len;
	if (pnp->pn_propp == NULL) {
		pnp->pn_propp = current;
	} else {
		/* preserve PROM order */
		for (next = pnp->pn_propp; next->pp_next != NULL;
		    next = next->pp_next) {
			/* NULL */
		}
		next->pp_next = current;
	}
}

/*
 * Fill in a node of the in-kernel PROM tree. Make copies of all the
 * PROM properties for the passed-in node, and create necessary
 * children and siblings.
 */
static int
copy_prom_node(prom_node_t *pnp)
{
	int	len;
	char name[OBP_MAXPROPNAME+1];
	char *prvname;
	char *val;
	prom_node_t *new_node;
	phandle_t ph, nph;

	if (pnp == NULL)
		return (0);
	ph = (phandle_t)pnp->pn_propp;
	pnp->pn_propp = (struct prom_prop *)0;
	name[0] = '\0';
	prvname = NULL;
	for (prvname = NULL; ; prvname = name) {
		if (BOP1275_NEXTPROP(bootops, ph, prvname, name) <= 0)
			break;
		if ((len = BOP1275_GETPROPLEN(bootops, ph, name)) == -1)
			continue;
		if (len != 0) {
			val = kmem_zalloc(len, KM_SLEEP);
			BOP1275_GETPROP(bootops, ph, name, val, len);
		} else {
			val = NULL;
		}
		create_prom_prop(pnp, name, val, len);
	}

	nph = BOP1275_PEER(bootops, ph);
	if (nph != (phandle_t)0 &&
	    BOP1275_GETPROP(bootops, nph, "name", name, OBP_MAXPROPNAME) > 0) {
		new_node = (prom_node_t *)kmem_zalloc(sizeof (struct prom_node),
			KM_SLEEP);
		new_node->pn_propp = (struct prom_prop *)nph;
		pnp->pn_sibling = new_node;
	}

	nph = BOP1275_CHILD(bootops, ph);
	if (nph != (phandle_t)0 &&
	    BOP1275_GETPROP(bootops, nph, "name", name, OBP_MAXPROPNAME) > 0) {
		new_node = (prom_node_t *)kmem_zalloc(sizeof (struct prom_node),
			KM_SLEEP);
		new_node->pn_propp = (struct prom_prop *)nph;
		pnp->pn_child = new_node;
	}

	return (0);
}

/*
 * Fatal promif internal error, not an external interface
 */

/*ARGSUSED*/
void
prom_fatal_error(const char *errormsg)
{

	volatile int	zero = 0;
	volatile int	i = 1;

	/*
	 * No prom interface, try to cause a trap by dividing by zero.
	 */

	i = i / zero;
	/*NOTREACHED*/
}
#endif /* !defined(KADB) && !defined(I386BOOT) */
