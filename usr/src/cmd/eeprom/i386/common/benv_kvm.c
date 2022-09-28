/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident   "@(#)benv_kvm.c 1.3 96/02/18 SMI"

#include "benv.h"
#include <kvm.h>
#include <nlist.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>

#ifdef	DPRINT
#define	min(x, y)	(((x) < (y)) ? (x) : (y))
#define	dprintf	if (vdebug_flag) printf
static	char vdebug_flag = 1;

static void
dprint_dev_info(caddr_t kaddr, struct dev_info *dp)
{
	dprintf("Devinfo node at kaddr <0x%x>:\n", kaddr);
	dprintf("\tdevi_parent <0x%x>\n", DEVI(dp)->devi_parent);
	dprintf("\tdevi_child <0x%x>\n", DEVI(dp)->devi_child);
	dprintf("\tdevi_sibling <0x%x>\n", DEVI(dp)->devi_sibling);
	dprintf("\tdevi_name <0x%x>\n", DEVI(dp)->devi_name);
	dprintf("\tdevi_nodeid <0x%x>\n", DEVI(dp)->devi_nodeid);
	dprintf("\tdevi_instance <0x%x>\n", DEVI(dp)->devi_instance);
	dprintf("\tdevi_ops <0x%x>\n", DEVI(dp)->devi_ops);
	dprintf("\tdevi_drv_prop_ptr <0x%x>\n", DEVI(dp)->devi_drv_prop_ptr);
	dprintf("\tdevi_sys_prop_ptr <0x%x>\n", DEVI(dp)->devi_sys_prop_ptr);
	dprintf("\tdevi_hw_prop_ptr <0x%x>\n", DEVI(dp)->devi_hw_prop_ptr);
}

static void
indent_to_level(int ilev)
{
	register i;

	for (i = 0; i < ilev; i++)
		(void) printf("    ");
}

static void
dprint_prop_list(char *name, ddi_prop_t **list_head, int ilev)
{
	int i;
	ddi_prop_t *propp;

	if (!vdebug_flag)
		return;

	if (*list_head != 0)  {
		indent_to_level(ilev);
		printf("%s software properties:\n", name);
	}

	for (propp = *list_head; propp != 0; propp = propp->prop_next)  {

		indent_to_level(ilev +1);
		printf("name <%s> length <%d>",
			propp->prop_name, propp->prop_len);

		if (propp->prop_flags & DDI_PROP_UNDEF_IT)  {
			printf(" -- Undefined.\n");
			continue;
		}

		if (propp->prop_len == 0)  {
			printf(" -- <no value>.\n");
			continue;
		}

		putchar('\n');
		indent_to_level(ilev +1);
		printf("    value <0x");
		for (i = 0; i < min(propp->prop_len, 64); ++i)  {
			unsigned byte;
			byte = (unsigned)((unsigned char)*(propp->prop_val+i));
			printf("%2.2x", byte);
		}
		printf(">.\n");
	}
}

static void
dprint_devs(struct dev_info *dp, int ilev)
{
	if (!vdebug_flag)
		return;

	dprint_prop_list("Hardware", &(DEVI(dp)->devi_hw_prop_ptr), ilev);
}
#endif	DPRINT

#define	DSIZE	(sizeof (struct dev_info))
#define	P_DSIZE	(sizeof (dev_info_t *))
#define	KNAMEBUFSIZE 256

static kvm_t *kd;
static char *mfail = "malloc";
static struct dev_info root_node;

static char *
get_kname(char *kaddr)
{
	auto char buf[KNAMEBUFSIZE], *rv;
	register i = 0;
	char c;

#ifdef	DPRINT
	dprintf("get_kname: kaddr %x\n", (int)kaddr);
#endif	DPRINT

	if (kaddr == (char *)0) {
		(void) strcpy(buf, "<null>");
		i = 7;
	} else {
		while (i < KNAMEBUFSIZE) {
			if (kvm_read(kd, (u_long)kaddr++, (char *)&c, 1) != 1) {
				exit(_error(PERROR,
					    "kvm_read of name string"));
			}
			if ((buf[i++] = c) == (char)0)
				break;
		}
		buf[i] = 0;
	}
	if ((rv = malloc((unsigned)i)) == 0) {
		exit(_error(PERROR, mfail));
	}
	strncpy(rv, buf, i);

#ifdef	DPRINT
	dprintf("get_kname: got string <%s>\n", rv);
#endif	DPRINT

	return (rv);
}

static void
get_kpropdata(ddi_prop_t *propp)
{
	char *p;

	propp->prop_name = get_kname(propp->prop_name); /* XXX */

	if (propp->prop_len != 0)  {
		p = malloc((unsigned)propp->prop_len);

		if (p == 0)
			exit(_error(PERROR, mfail));

		if (kvm_read(kd, (u_long)propp->prop_val, (char *)p,
		    (unsigned)propp->prop_len) != (int)propp->prop_len)
			exit(_error(PERROR, "kvm_read of property data"));

		propp->prop_val = (caddr_t)p;
	}
}

static void
get_kproplist(ddi_prop_t **list_head)
{
	ddi_prop_t *kpropp, *npropp, *prevpropp;


#ifdef	DPRINT
	dprintf("get_kproplist: Reading property list at kaddr <0x%x>\n",
	    *list_head);
#endif	DPRINT

	prevpropp = 0;
	npropp = 0;
	for (kpropp = *list_head; kpropp != 0; kpropp = npropp->prop_next)  {

#ifdef	DPRINT
		dprintf("get_kproplist: Reading property data size <%d>",
		    sizeof (ddi_prop_t));
		dprintf(" at kadr <0x%x>\n", kpropp);
#endif	DPRINT

		npropp = (ddi_prop_t *)malloc(sizeof (ddi_prop_t));
		if (npropp == 0)
			exit(_error(PERROR, mfail));

		if (kvm_read(kd, (u_long)kpropp, (char *)npropp,
		    sizeof (ddi_prop_t)) != sizeof (ddi_prop_t))
			exit(_error(PERROR, "kvm_read of property data"));

		if (prevpropp == 0)
			*list_head = npropp;
		else
			prevpropp->prop_next = npropp;

		prevpropp = npropp;
		get_kpropdata(npropp);
	}
}

#define	NCCNT	4
struct node_cache {
	char *name;
	struct dev_info *node;
} node_cache[NCCNT];
int nccnt;

static struct dev_info *
get_cache_node(char *name)
{
	int i;

	for (i = 0; i < NCCNT; i++)
		if (node_cache[i].name == NULL)
			break;
		else if (strcmp(name, node_cache[i].name) == 0)
			return (node_cache[i].node);
	return (NULL);
}

static void
set_cache_node(char *name, struct dev_info *node)
{
	node_cache[nccnt].name = name;
	node_cache[nccnt].node = node;
	nccnt = (nccnt + 1) % NCCNT;
}

ddi_prop_t *
get_proplist(char *name)
{
	char *tptr;
	caddr_t kdp;
	struct dev_info *dp;
	struct dev_info *node;

	if ((node = get_cache_node(name)) != NULL)
		return (node->devi_hw_prop_ptr);

	for (dp = root_node.devi_child; dp != NULL; dp = dp->devi_sibling) {

		if ((tptr = malloc(DSIZE)) == NULL)
			exit(_error(PERROR, mfail));

		if (kvm_read(kd, (u_long)dp, tptr, DSIZE) != DSIZE)
			exit(_error(PERROR, "kvm_read of devi_child"));

		kdp = (caddr_t)dp;
		dp = (struct dev_info *)tptr;
		if (dp->devi_name) {
			dp->devi_name = get_kname(dp->devi_name);

#ifdef	DPRINT
			dprintf("copied nodename is <%s>\n", dp->devi_name ?
				dp->devi_name : "<none>");
#endif	DPRINT
			if (strcmp(dp->devi_name, name) == 0) {
				node = dp;
				get_kproplist(&(DEVI(dp)->devi_hw_prop_ptr));
#ifdef	DPRINT
				dprint_dev_info(kdp, dp);
				dprint_devs(dp, 1);
#endif	DPRINT
					break;
			}
		}
	}

	set_cache_node(name, node);
	return ((node == NULL) ? NULL : node->devi_hw_prop_ptr);
}

caddr_t
get_propval(char *name, char *node)
{
	ddi_prop_t *prop, *plist;

	if ((plist = get_proplist(node)) == NULL)
		return (NULL);

	for (prop = plist; prop != NULL; prop = prop->prop_next)
		if (strcmp(prop->prop_name, name) == 0)
			return (prop->prop_val);

	return (NULL);
}

void
get_kbenv(void)
{
	dev_info_t *rnodep;
	static struct nlist list[] = {
		{"top_devinfo"},
		""
	};
	struct nlist *top_devinfo_nlp = &list[0];
	int i;

	if ((kd = kvm_open((char *)0, (char *)0, (char *)0, O_RDONLY, progname))
	    == NULL) {
		exit(_error(PERROR, "kvm_open failed"));
	} else if ((kvm_nlist(kd, &list[0])) != 0) {
		(void) _error(NO_PERROR,
		    "%s not available on kernel architecture %s (yet).",
		    progname, uts_buf.machine);
		exit(1);
	}

	/*
	 * Build the root node...
	 */
#ifdef	DPRINT
	dprintf("Building root node DSIZE %d, P_DSIZE %d...",
	    DSIZE, P_DSIZE);
#endif	DPRINT

	if (kvm_read(kd, top_devinfo_nlp->n_value, (char *)&rnodep, P_DSIZE)
	    != P_DSIZE) {
		exit(_error(PERROR, "kvm_read of root node pointer fails"));
	}

	if (kvm_read(kd, (u_long)rnodep, (char *)&root_node, DSIZE) != DSIZE) {
		exit(_error(PERROR, "kvm_read of root node fails"));
	}

#ifdef	DPRINT
	dprintf("got root node.\n");
	dprint_dev_info((caddr_t)rnodep, &root_node);
#endif	DPRINT
}

void
close_kbenv(void)
{
	(void) kvm_close(kd);
}
