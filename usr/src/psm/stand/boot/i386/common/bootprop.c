/*
 * Copyright (c) 1992-1996 by Sun Microsystems, Inc.  All Rights Reserved.
 */

#ident "@(#)bootprop.c	1.26	96/07/30 SMI"

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/machine.h>
#include <sys/salib.h>
#include <values.h>
#include "devtree.h"

#define	__ctype _ctype		/* Incredibly stupid hack used by	*/
#include <ctype.h>    		/* ".../stand/lib/i386/subr_i386.c"	*/

extern struct memlist *vfreelistp, *vinstalledp, *pfreelistp, *pinstalledp;
extern struct memlist *bphyslistp, *bvirtlistp;
extern struct dnode *active_node;

extern char *backfs_dev, *backfs_fstype;
extern char *frontfs_dev, *frontfs_fstype;

extern struct dnode *find_node();
extern int install_memlistptrs();
extern int get_end();

static int get_memlist();
static int get_string(), get_word();
extern int chario_get_dev(), chario_put_dev();
extern int chario_get_mode(), chario_put_mode();
extern char *build_path_name(struct dnode *dnp, char *buf, int len);
extern void *memcpy(void *s1, void *s2, size_t n);

extern struct bootops bootops;

int	boldgetproplen(struct bootops *bop, char *name);
int	boldgetprop(struct bootops *bop, char *name, void *value);
int	boldsetprop(struct bootops *bop, char *name, char *value);
char	*boldnextprop(struct bootops *bop, char *prevprop);
int	bgetproplen(struct bootops *, char *, phandle_t);
int	bgetprop(struct bootops *, char *, caddr_t, int, phandle_t);
int	bsetprop(struct bootops *, char *, caddr_t, int, phandle_t);
int	bnextprop(struct bootops *, char *, char *, phandle_t);

/*
 *  "global" pseudo-property values used by older kernels:
 */

char *mfg_name = "i86pc";
static int zero = 0; /* File descriptor for stdin/stdout */

extern int bootops_extensions;
extern int vac, cache_state;
extern char *impl_arch_name, *module_path, *kernname, *systype;

struct dprop *
prop_alloc(int size)
{

	return ((struct dprop *)bkmem_alloc(size));
}

void
prop_free(struct dprop *dpp, int size)
{
	/*
	 *  Use free routine appropriate to the property node's memory
	 *  location.  The size of each node is encoded within the
	 *  "dprop" struct.
	 */
	extern void rm_free(caddr_t, u_int);

	if ((unsigned)dpp >= USER_START)
		bkmem_free((char *)dpp, size);
	else
		rm_free((caddr_t)dpp, size);
}

/*
 *  AVL tree routines:
 *
 *    Technically, I didn't need to separate out the "rotate" routine, but
 *    the code can be shared with an AVL delete routine if there becomes a
 *    need for one.
 */

#define	root_prop(p)	/* Dummy dprop node at root of property tree!	*/ \
	(struct dprop *)((char *)&(p)->dn_proplist - \
	(int)((struct dprop *)0)->dp_link)

static int
compare(struct dnode *dnp, char *name, struct dprop *dpp)
{
	/*
	 *  AVL Comparison routine:
	 *
	 *  Returns a balance flag based on the result of a string
	 *  comparison of the two property names.  Note that the root
	 *  node (which has no name) compares greater than everything else.
	 */
	int x;

	return ((dpp == root_prop(dnp)) ? 0 :
	    (!(x = strcmp(name, dp_name(dpp))) ? -1 : (x > 0)));
}

static void
rotate(struct dprop *np, int sub, int bal)
{
	/*
	 *  Rotate a Subtree:
	 *
	 *    This routine is called after adding a property node that
	 *    unbalances the AVL tree.  It rebalances the offending
	 *    "sub"tree of the base node ("np") by performing one of two
	 *    possible rotations.  The "bal"ance flag specifies which
	 *    side of the unbalanced subtree is heavier.
	 *
	 *    A tri-state return code reflects the state of the tree after the
	 *    rotation is performed:
	 *
	 *    0  --  Single rotation, tree height unchanged
	 *    1  --  Single rotation, tree height changed
	 *    2  --  Double rotation
	 *
	 *    Note that a zero return is only possible after node deletion,
	 *    which isn't supported (but easily could be).
	 */
	int j, k, x = bal ^ 1;
	struct dprop *pivot = addr(np->dp_link[sub]);
	struct dprop *px, *pp, *pr = addr(pivot->dp_link[bal]);

	if (x == (j = dp_getflag(pr))) {
		/* BEGIN CSTYLED */
		/*
		 *  Double rotation;  Graphically, the transformation looks
		 *  like this ...
		 *
		 *  *   [] <-pivot                         pp-> []
		 *  *  /  \                                    /  \
		 *  *  |   [] <-pr                   pivot-> []   [] <-pr
		 *  *  |  /  \              =>              /  \ /  \
		 *  *     |   [] <-pp                       |  | |  |
		 *  *     |  /  \                           |  New  |
		 *  *        |  |
		 *  *         New
		 *
		 *  The "New" node was inserted into one of "pp"s original
		 *  subtrees; the "k" register tells us which one.
		 */
		/* END CSTYLED */
		pp = addr(pr->dp_link[x]);
		k = dp_getflag(pp);

		pr->dp_link[x].address = px = addr(pp->dp_link[bal]);
		if (px) px->dp_parent = pr;

		pp->dp_link[bal].address = pr;
		pr->dp_parent = pp;

		pivot->dp_link[bal].address = px = addr(pp->dp_link[x]);
		if (px) px->dp_parent = pivot;

		pp->dp_link[x].address = pivot;
		pivot->dp_parent = pp;

		dp_setflag_nohvy(pp);

		if (k == bal) {
			/* Tree is now balanced below "pr" */
			dp_setflag(pivot, x);
			dp_setflag_nohvy(pr);
		} else {
			/*
			 * Tree may still be unbalanced, but by no more
			 * than 1 node.
			 */
			k = ((k >= 0) ? bal : -1);
			dp_setflag_nohvy(pivot);
			dp_setflag(pr, k);
		}

		k = 2;		/* Let caller know what we just did! */

	} else {
		/* BEGIN CSTYLED */
		/*
		 *  Single rotation; The basic transformation looks like
		 *  this ...
		 *
		 *  *   [] <- pivot                       pr -> [] <- pp
		 *  *  /  \                                    /  \
		 *  *  |   [] <- pr                  pivot-> []   |
		 *  *  |  /  \              =>              /  \  |
		 *  *     |  |                              |  |  New
		 *  *     |  |                              |  |
		 *  *       New
		 *
		 *  Normally, subtrees will grow (or shrink) as a result of this
		 *  transformation, but delete processing can leave subtree
		 *  depth unchanged after a single rotation.  "k" register will
		 *  be zero in this case.
		 */
		/* END CSTYLED */
		pivot->dp_link[bal].address = px = addr(pr->dp_link[x]);
		if (px) px->dp_parent = pivot;

		pr->dp_link[x].address = pivot;
		pivot->dp_parent = pp = pr;

		if ((k = (j >= 0)) != 0) {
			/* We just rebalanced the tree.  Fix balance flags */
			dp_setflag_nohvy(pivot);
			dp_setflag_nohvy(pr);
		} else {
			/* Tree remains slightly unbalanced (delete's only!) */
			dp_setflag(pivot, bal);
			dp_setflag(pr, bal ^ 1);
		}
	}

	/*
	 *  Set new rotation point in parent node.  "x" register preserves
	 *  the balance flag while we dink with other parts of the link.
	 */

	x = np->dp_link[sub].heavy;
	np->dp_link[sub].address = pp;
	np->dp_link[sub].heavy = x;
	pp->dp_parent = np;
}

struct dprop *
find_prop_node(struct dnode *dnp, char *name, struct dprop *np)
{
	/*
	 *  Find a property node:
	 *
	 *    This routine searches the property list associated with the device
	 *    node at "dp" looking for a property with the given "name" and re-
	 *    turns a pointer to it if found.  If the target property does not
	 *    exist (and "np" is non-null) we add the new node at "np" (which we
	 *    assume has the given name) and return null.
	 */
	int j, k;
	struct dprop *pp, *pq, *ps;
	struct dprop *pr = root_prop(dnp);

	for (ps = pq = addr(pr->dp_link[left]); pq; ) {
		/*
		 *  Walk the tree looking for the node in question.  When
		 *  (and if) we get to a leaf, the "pr" register will point
		 *  to the root of the subtree requiring a rebalance (if
		 *  there is one).
		 */
		pp = pq;

		if ((j = compare(dnp, name, pp)) < 0) {
			/*
			 *  This is the node we're looking for.
			 *  Return its address.
			 */
			return (pp);
		}

		if (((pq = addr(pp->dp_link[j])) != 0) &&
		    (dp_getflag(pq) >= 0)) {
			/*
			 *  If next node ("pq") isn't balanced, its parent
			 *  ("pp") becomes the rotation point for any
			 *  rebalancing operation we may undertake later.
			 */
			pr = pp;
			ps = pq;
		}
	}

	if (np != (struct dprop *)0) {
		/*
		 *  Caller wants to add a node.  Make sure its "dp_link" fields
		 *  are clear before going any further.
		 */

		np->dp_link[left].address = np->dp_link[rite].address = 0;

		if (ps != (struct dprop *)0) {
			/*
			 *  Tree is non-empty.  The "pp" register points to
			 *  the new node's parent and the "j" register
			 *  specifies which subtree we're to hang it off of.
			 */
			pp->dp_link[j].address = np;
			np->dp_parent = pp;

			k = compare(dnp, name, ps);
			pp = addr(ps->dp_link[k]);

			while ((j = compare(dnp, name, pp)) >= 0) {
				/*
				 *  Adjust all balance flags between our base
				 *  ancestor ("ps" register) and the new node
				 *  to reflect the fact that the tree may
				 *  have grown a bit taller.
				 */
				dp_setflag(pp, j);
				pp = addr(pp->dp_link[j]);
			}

			if ((j = dp_getflag(ps)) == k) {
				/*
				 * Tree has become unbalanced;
				 * rotate to fix things.
				 */
				rotate(pr, compare(dnp, name, pr), j);
			} else {
				/* Tree has grown, but remains balanced */
				if (j >= 0) k = -1;
				dp_setflag(ps, k);
			}

		} else {
			/*
			 *  Tree is empty.  Place the new node on the left side
			 *  of the dummy root node ("pr" register) and mark
			 *  this side heavy.
			 */
			pr->dp_link[left].address = np;
			pr->dp_link[left].heavy = 1;
			np->dp_parent = pr;
		}
	}

	return ((struct dprop *)0);
}

static struct dprop *
descend(struct dprop *dpp)
{
	/*
	 *  Descend left-hand subtree:
	 *
	 *    This routine is used to locate the node to the extreme left of
	 *    the given node.  It is used by the "bnextprop" routine to walk
	 *    the AVL tree.
	 */

	struct dprop *dxp = dpp;

	while (dxp) dxp = addr((dpp = dxp)->dp_link[left]);
	return (dpp);
}

/*
 * Recursively free all props hung off this property
 */
void
free_props(struct dprop *dpp)
{
	if (addr(dpp->dp_link[left])) {
		free_props(addr(dpp->dp_link[left]));
	}

	if (addr(dpp->dp_link[rite])) {
		free_props(addr(dpp->dp_link[rite]));
	}

	prop_free(dpp, dpp->dp_size);
}


/*
 * Pseudo properties:
 *
 * The properties listed in the table below are special in that they
 * are not stored in contiguous memory location.  Hence a simple "memcpy"
 * is not sufficient to extract the corresponding values.  Instead, we
 * use a special-purpose "get" routine, whose address is recorded in the
 * table.  A corresponding "put" routine may be used to set the value
 * of a pseduo-property.
 */

struct pprop pseudo_props[] =
{
	/* BEGIN CSTYLED */
	/*
	 * NOTE:  This table must be maintained in order of property name
	 * within node address (so we use a binary search).  The
	 * partial ordering for nodes is:
	 *
	 * root  >  boot   >  bootmem  >  alias   >  chosen  >
	 * mem   >  mmu    >  prom     >  option  >  package
	 */
	/* END CSTYLED */

	{&bootmem_node, "reg", get_memlist, (void *)&bphyslistp, 0, 0},
	{&bootmem_node, "virtual", get_memlist, (void *)&bvirtlistp, 0, 0},
	{&chosen_node, "backfs-fstype", get_string, (void *)&backfs_fstype,
	    0, 0},
	{&chosen_node, "backfs-path", get_string, (void *)&backfs_dev, 0, 0},
	{&chosen_node, "boot-end", get_end, 0, 0, 0},
	{&chosen_node, "bootops-extensions", get_word,
	    (void *)&bootops_extensions, 0, 0},
	{&chosen_node, "cache-on?", get_word, (void *)&cache_state, 0, 0},
	{&chosen_node, "default-name", get_string, (void *)&kernname, 0, 0},
	{&chosen_node, "frontfs-fstype", get_string, (void *)&frontfs_fstype,
	    0, 0},
	{&chosen_node, "frontfs-path", get_string, (void *)&frontfs_dev, 0, 0},
	{&chosen_node, "fstype", get_string, (void *)&systype, 0, 0},
	{&chosen_node, "impl-arch-name", get_string, (void *)&impl_arch_name,
	    0, 0},
	{&chosen_node, "memory-update", install_memlistptrs, 0, 0, 0},
	{&chosen_node, "mfg-name", get_string, (void *)&mfg_name, 0, 0},
	{&chosen_node, "phys-avail", get_memlist, (void *)&pfreelistp, 0, 0},
	{&chosen_node, "phys-installed", get_memlist, (void *)&pinstalledp,
	    0, 0},
	{&chosen_node, "stdin", get_word, (void *)&zero, 0, 0},
	{&chosen_node, "stdout", get_word, (void *)&zero, 0, 0},
	{&chosen_node, "vac", get_word, (void *)&vac, 0, 0},
	{&chosen_node, "virt-avail", get_memlist, (void *)&vfreelistp, 0, 0},
	{&mem_node, "available", get_memlist, (void *)&pfreelistp, 0, 0},
	{&mem_node, "reg", get_memlist, (void *)&pinstalledp, 0, 0},
	{&mmu_node, "available", get_memlist, (void *)&vfreelistp, 0, 0},
	{&mmu_node, "existing", get_memlist, (void *)&vinstalledp, 0, 0},
	{&option_node, "input-device", chario_get_dev, (void *)"input-device",
	    chario_put_dev, (void *)"input-device"},
	{&option_node, "module-path", get_string, (void *)&module_path, 0, 0},
	{&option_node, "output-device", chario_get_dev, (void *)"output-device",
	    chario_put_dev, (void *)"output-device"},
	{&option_node, "ttya-mode", chario_get_mode, (void *)"ttya-mode",
	    chario_put_mode, (void *)"ttya-mode"},
	{&option_node, "ttyb-mode", chario_get_mode, (void *)"ttyb-mode",
	    chario_put_mode, (void *)"ttyb-mode"},
};

int pseudo_prop_count = sizeof (pseudo_props)/sizeof (struct pprop);

static struct pprop *
find_special(struct dnode *dnp, char *name)
{
	/*
	 * Search special properties list:
	 *
	 * This routine returns a non-zero value (i.e, a pointer to an
	 * appropriate "pseudo_props" table entry) if the specified
	 * property of the given node is in some way special.
	 */
	struct pprop *sbot = pseudo_props;
	struct pprop *stop = &pseudo_props[pseudo_prop_count];

	while (stop > sbot) {
		/*
		 *  Perform a binary search of the special properties table.
		 *  Comparisons consist of two parts:  Node addresses (the
		 *  fast compare) and property names (the slow compares).
		 *  Obviously, we skip the latter when the former yields
		 *  not-equal!
		 */
		int rc;
		struct pprop *sp = sbot + ((stop - sbot) >> 1);

		if (dnp == sp->node) {
			if (!(rc = strcmp(name, sp->name))) {
				/*
				 *  This is the node we want; return
				 *  its address to the caller.
				 */
				return (sp);
			}
		} else {
			/*
			 *  We can skip the string comparison when nodes are
			 *  non-equal,  but we do have to reset the "rc"
			 *  register to tell us which way to move the search.
			 */
			rc = ((dnp > sp->node) ? 1 : -1);
		}

		if (rc > 0) sbot = sp+1;
		else stop = sp;
	}

	return ((struct pprop *)0);
}

/*ARGSUSED*/
static int
get_word(struct dnode *dnp, int *buf, int len, int *value)
{	/* Return given word as pseudo-property value 			*/

	if (!buf || (len > sizeof (int)))
		len = sizeof (int);
	if (buf)
		(void) memcpy(buf, value, len);
	return (len);
}

/*ARGSUSED*/
static int
get_string(struct dnode *dnp, char *buf, int len, char **value)
{
	/* Return given string as pseudo-property value */
	char *vp = ((*value) ? *value : "");
	int lx = strlen(vp)+1;

	if (!buf || (len > lx))
		len = lx;
	if (buf)
		(void) memcpy(buf, vp, len);
	return (len);
}

/*ARGSUSED*/
static int
get_memlist(struct dnode *dnp, char *buf, int len, void *head)
{
	/*
	 *  The "get" routine for memory lists.  These are linked lists that
	 *  we copy into the indicated "buf" (except when "buf" is null, in
	 *  which case we simply return the amount of storage required to hold
	 *  the memlist array).
	 */
	int size = 0;
	struct memlist *mlp;

	for (mlp = *((struct memlist **)head); mlp; mlp = mlp->next) {
		/*
		 *  Step thru the memlist calculating size and (if asked),
		 *  copying its contents to the output "buf"fer.
		 */
		if (buf) {
			/*
			 *  We're delivering the real value, copy the next
			 *  address/length pair into the output buffer.
			 *  There's a bit of weirdness here to deal with buffer
			 *  lengths that are not multiples of the word size
			 *  (I don't know why I bother!).
			 */
			int x, n = 2;
			u_int word = mlp->address;

			while (n-- && (len > 0)) {
				/*
				 *  There's at least one byte remaining in
				 *  the output buffer.  Copy as much of the
				 *  next memlist word as will fit and
				 *  update the length registers accordingly.
				 */
				x = ((len -= sizeof (u_int)) >= 0) ?
				    sizeof (u_int) :
				    (-len & (sizeof (u_int)-1));
				(void) memcpy(buf, (caddr_t)&word, x);
				buf += sizeof (u_int);
				word = mlp->size;
				size += x;
			}

			if (len <= 0) {
				/*
				 *  We ran out of output buffer.
				 *  Time to bail out!
				 */
				break;
			}
		} else {
			/*
			 *  Caller is just asking for the memlist size.  This
			 *  will work out to 8 bytes per memlist entry.
			 */
			size += (2 * sizeof (u_int));
		}
	}

	return (size);
}

static int
check_name(char *name, int size)
{
	/*
	 *  Validate a 1275 name:
	 *
	 *  P1275 has some rather strict rules about what constitutes a name.
	 *  This routine ensures the given "name" follows those rules.
	 *  It returns "BOOT_FAILURE" if it does not.
	 */
	int c;
	char *cp = name;

	if (size > MAX1275NAME) {
		/* Name is too long, bail out now! */

		printf("setprop: name too long\n");
		return (BOOT_FAILURE);
	}

	while (*cp) {
		switch (c = *cp++) {
		/*
		 *  Only certain characters are legal in a name, so we check
		 *  here to make sure that no disallowd characters are being
		 *  used.  Per 1275 Spec, section 3.2.2.1.1, "The property
		 *  name is a human-readable text string consisting of one
		 *  to thirty-one printable characters. Property names shall
		 *  not contain uppercase characters or the characters
		 *  "/", "\", ":", "[", "]", and "@"."
		 */
		case '/':
		case '\\':
		case ':':
		case '[':
		case ']':
		case '@':
			break;

		default:
			if (isupper(c) || !isascii(c) || !isalnum(c)) break;
		}
	}

	if ((cp - name) != (size - 1)) {
		/* Name contains an invalid character */
		printf("setprop: invalid name\n");
		return (BOOT_FAILURE);
	}

	return (size);
}

/*
 *  These routines implement the boot getprop interface.  These new & improved
 *  versions follow the semantics of the corresponding 1275 forth words.  The
 *  older (slightly different) semantics are still supported by passing in
 *  zeros for the new arguments.  the get_arg() macro determines if we were
 *  passed a zero and should therefor use a default value for that arg.
 */

#define	get_arg(name, type, defval)		\
(						\
	    (name != 0) ? (type)name : defval	\
)

int
boldgetproplen(struct bootops *bop, char *name)
{
	return (bgetproplen(bop, name, 0));
}

/*ARGSUSED*/
int
bgetproplen(struct bootops *bop, char *name, phandle_t node)
{
	/*
	 *  Return the length of the "name"d property's value.  If the "node"
	 *  argument is null (or omitted), we try both the 1275 "/options"
	 *  and "/chosen" nodes.
	 */
	struct pprop *sp;
	struct dprop *dpp;
	struct dnode *dnp;

	struct dnode *nodelist[2];
	int nnodes, i;

	nnodes = 1;
	if (node != (struct dnode *)0) {
		nodelist[0] = node;
	} else {
		nodelist[0] = &chosen_node;
		nodelist[1] = &option_node;
		nnodes++;
	}

	for (i = 0; i < nnodes; i++) {
		dnp = nodelist[i];
		if (sp = find_special(dnp, name)) {
			/*
			 * A special node; use the "get" method to
			 * fetch the length
			 */
			return ((sp->get)(dnp, 0, 0, sp->getarg));
		} else if (dpp = find_prop_node(dnp, name, 0)) {
			/*
			 * We found the property in question,
			 * return the value size
			 */
			return (dpp->dp_valsize);
		}
	}

	/* Property not found, return error code */
	return (BOOT_FAILURE);
}

int
boldgetprop(struct bootops *bop, char *name, void *value)
{
	/* old version only returns BOOT_SUCCESS or BOOT_FAILURE */
	return ((bgetprop(bop, name, value, 0, 0) == BOOT_FAILURE)
	    ? BOOT_FAILURE : BOOT_SUCCESS);
}

/*ARGSUSED*/
int
bgetprop(struct bootops *bop, char *name, caddr_t buf, int size,
	    phandle_t node)
{
	/*
	 *  Return the "name"d property's value in the specified "buf"ffer,
	 *  but don't copy more than "size" bytes.  If "size" is zero (or
	 *  omitted), the buffer is assumed to be big enough to hold the
	 *  value (i.e, caller used "getproplen" to obtain the value size).
	 *  If the "node" argument is null (or omitted), work from either
	 *  the 1275 "/options" node or the "/chosen" node.
	 */
	struct pprop *sp;
	struct dprop *dpp;
	int len = get_arg(size, int, MAXINT);
	struct dnode *dnp;

	struct dnode *nodelist[2];
	int nnodes, i;

	nnodes = 1;
	if (node != (struct dnode *)0) {
		nodelist[0] = node;
	} else {
		nodelist[0] = &chosen_node;
		nodelist[1] = &option_node;
		nnodes++;
	}

	for (i = 0; i < nnodes; i++) {
		dnp = nodelist[i];
		if (sp = find_special(dnp, name)) {
			/* A special property, use the special "get" method */

			len = (sp->get)(dnp, buf, len, sp->getarg);
			return (len);

		} else if (dpp = find_prop_node(dnp, name, 0)) {
			/* Found the property in question; return its value */

			if (len > dpp->dp_valsize) len = dpp->dp_valsize;
			(void) memcpy(buf, (caddr_t)dp_value(dpp), len);
			return (len);

		}
	}

	/* Property not found, return error code */
	return (BOOT_FAILURE);
}

char *
boldnextprop(struct bootops *bop, char *prevprop)
{
	return ((char *)bnextprop(bop, prevprop, 0, 0));
}

int
bnextprop(struct bootops *bop, char *prev, char *buf, phandle_t node)
{
	/*
	 *  Return the name of the property following the "prev"ious property
	 *  (NULL if "prev" is the last property in the list).  Does NOT return
	 *  the name of any properties starting with a dollar sign!
	 */
	int climbing = 0;
	struct dprop *dpp, *dxp = 0;
	char *np = get_arg(buf, char *, (char *)0);
	struct dnode *dnp = get_arg(node, struct dnode *, &option_node);

	if (prev && *prev) {
		/*
		 *  Caller has provided a non-null "prev"ious name pointer.
		 *  Unfortunately, this may be the name of a pseudo property.
		 *  If so, our first attempt to locate it will fail ...
		 */
		if (!(dpp = find_prop_node(dnp, prev, 0))) {
			/*
			 *  ... and we'll have to try again with a space
			 *  concatenated onto the tail end!
			 */
			char prev_name[MAX1275NAME+1];

			(void) sprintf(prev_name, "%s ", prev);
			dpp = find_prop_node(dnp, prev_name, 0);
		}

		if (dpp != (struct dprop *)0) {
			/*
			 *  Caller is working from an established property.
			 *  Use the "find_prop_node" routine to locate the
			 *  corresponding "dprop" structure, and move forward
			 *  from there.
			 */
			for (;;) {
				/*
				 *  When (and if) this loop exits, "dxp" will
				 *  point to the next property node!
				 */
				if (!climbing &&
				    (dxp = addr(dpp->dp_link[rite]))) {
					/*
					 *  If the current node has a
					 *  right-hand subtree, we haven't
					 *  processed it yet.  Descend thru
					 *  its left subtrees until we get to
					 *  a leaf.  This becomes the next
					 *  property node.
					 */
					dxp = descend(dxp);
					break;
				} else if ((dxp = dpp->dp_parent) ==
					    root_prop(dnp)) {
					/*
					 *  If parent is root of the tree,
					 *  we're done!  Return zero length
					 *  (or null name ptr) to indicate
					 *  end of list.
					 */
					return (0);
				} else if (climbing =
					    compare(dnp, dp_name(dpp), dxp)) {
					/*
					 *  If we stepped up from the right
					 *  subtree, set the current node
					 *  pointer to the parent node and try
					 *  again.
					 */
					dpp = dxp;
				} else {
					/*
					 *  If we stepped up from the left
					 *  subtree, the parent becomes the
					 *  next property node!
					 */
					break;
				}
			}
		} else {
			/*
			 *  The "prev" property was not found, this is
			 *  a programming error!
			 */
			printf("nextprop: property not found\n");
			return (BOOT_FAILURE);
		}
	} else if (!(dxp = descend(addr(dnp->dn_proplist)))) {
		/*
		 *  Property list is empty!  Return null to indicate the end of
		 *  the list.
		 */
		return (0);
	} else if (*dp_name(dxp) == '$') {
		/*
		 *  Properties with names that begin with a dollar sign are
		 *  said to be "invisible" (i.e, the kernel doesn't see them),
		 *  so we refuse to deliver them.  Since dollar signs sort
		 *  ahead of all legal 1275 name characters, we can skip this
		 *  one by recursively calling ourselves!
		 */
		return (bnextprop(bop, dp_name(dxp), buf, node));
	}

	if (np != (char *)0) {
		/*
		 *  Enhanced version copies property name into the indicated
		 *  buffer and returns the total name length (including the
		 *  null).  We have to be careful of pseudo-property names
		 *  that end in a space, however!
		 */
		int x;

		(void) memcpy(np, dp_name(dxp), x = dxp->dp_namsize);
		if (np[x-2] == ' ') np[--x - 1] = '\0';
		return (x);
	} else {
		/*
		 *  Old version just returns a pointer to the name.  Since old
		 *  kernels don't know about pseudo-properties, there's no need
		 *  to check for them here.
		 */
		return ((int)dp_name(dxp));
	}
}

int
boldsetprop(struct bootops *bop, char *name, char *value)
{
	return (bsetprop(bop, name, value, 0, 0));
}

/*ARGSUSED*/
int
bsetprop(struct bootops *bop, char *name, caddr_t value, int size,
	    phandle_t node)
{
	/*
	 *  Set a property value.  If the property doesn't exist already, we'll
	 *  create it.  Otherwise, we just change its value (which might mean
	 *  re-allocating the storage we've assigned to it.
	 *
	 *  Values are copied into dynamically allocated storage.  This allows
	 *  calling routines to pass stack-local buffers as values.
	 */
	struct pprop *sp;
	struct dprop *dpp = (struct dprop *)0;
	struct dnode *pp, *dnp = get_arg(node, struct dnode *, &option_node);

	static int recurse = 0;  /* Prevent infinite loops due to recursion */

	int len = get_arg(size, int, (node ? size : (strlen((char *)value)+1)));

	int x;
	int nam_size = check_name(name, strlen(name)+1);
	int tot_size = (sizeof (*dpp) - 2) + nam_size + len;

	if (nam_size <= 0) {
		/*
		 *  Property name is bogus, bail out now!
		 */

		return (BOOT_FAILURE);
	}

	/* begin SPECIAL CASES section */

	/*
	 * bootpath should be mirrored on both /options and /chosen.
	 * boot-path should be mirrored on both /options and /chosen.
	 * whoami should be mirrored on both /options and /chosen.
	 *
	 * boot-args and bootargs should be the same and in sync on both
	 * /options and /chosen.
	 */
	if (!recurse && (dnp == &option_node || dnp == &chosen_node)) {
		struct dnode *mnode;
		char *mname;
		int isbp, isba;

		/*
		 *  Mirror values on the node we weren't called with
		 */
		mnode = ((dnp == &option_node) ? &chosen_node : &option_node);
		if (strcmp(name, "bootpath") == 0 ||
		    strcmp(name, "boot-path") == 0 ||
		    strcmp(name, "bootargs") == 0 ||
		    strcmp(name, "boot-args") == 0 ||
		    strcmp(name, "whoami") == 0) {
			recurse = 1;
			(void) bsetprop(bop, name, value, len, mnode);
			recurse = 0;
		}

		/*
		 * bootargs and boot-args should be kept the same on both
		 * /chosen and /options
		 *
		 * We've already taken care of mirroring this guy on the
		 * mirror node, now we have to make the copy on both nodes.
		 * E.G., if we are setting "boot-args", we've already made
		 * sure above that the same value is set under both /options
		 * and /chosen.  What we'll do below is ensure that "bootargs"
		 * is set on both those nodes as well.
		 */
		mname = (char *)0;
		if (strcmp(name, "boot-args") == 0) {
			mname = "bootargs";
		} else if (strcmp(name, "bootargs") == 0) {
			mname = "boot-args";
		}

		if (mname) {
			recurse = 1;
			(void) bsetprop(bop, mname, value, len, dnp);
			(void) bsetprop(bop, mname, value, len, mnode);
			recurse = 0;
		}
	}

	/* end SPECIAL CASES section */

	if (!(x = strcmp(name, "name")) &&
	    (check_name((char *)value, size) < 0)) {
		/*
		 *  Caller is changing this node's "name" property, so we have
		 *  to perform some special checking.  The value of a "name"
		 *  property must, itself, be a valid name.
		 */
		return (BOOT_FAILURE);
	}

	if ((pp = dnp->dn_parent) != 0) {
		/*
		 *  If we're setting a property of a non-root node, we have to
		 *  make sure we're not altering the device tree sort sequence
		 *  by doing so.  Note that "x" register will be zero if we're
		 *  trying to change this node's name.
		 */
		if ((x == 0) || (strcmp(name, "$at") == 0)) {
			/*
			 *  The two properties that influence the sort
			 *  sequence are "name" and "$at" (device address).
			 *  If we're changing either of these, we have to make
			 *  sure that the new node specification doesn't
			 *  conflict with one that already exists.
			 */
			char *np = (char *)value;
			char *ap = (char *)value;

			if (x == 0) {
				/*
				 *  If we're not changing the node address,
				 *  figure out what the current node address
				 *  happens to be.  This will be the
				 *  current value of the "$at" property if
				 *  there is one, "0,0" otherwise.
				 */
				dpp = find_prop_node(dnp, "$at", 0);
				ap = (dpp ? dp_value(dpp) : "0,0");
			} else if (len > MAX1275ADDR) {
				/*
				 *  If we are changing the node address, make
				 *  sure it will fit in our internal buffers!
				 */
				printf("setprop: $at value too long\n");
				return (BOOT_FAILURE);
			} else {
				/*
				 *  If we're not changing the name field, find
				 *  the current value of the name property.
				 *  This is null if we haven't set a name yet.
				 */
				dpp = find_prop_node(dnp, "name", 0);
				np = (dpp ? dp_value(dpp) : "");
			}

			if (value && (!x || dpp)) {
				/*
				 *  Build the new pathname for this node, then
				 *  use "find_node" to see if it already
				 *  exists!
				 */
				struct dnode *dxp;
				char path[((MAX1275NAME+1) * 8) + 1];
				(void) build_path_name(pp, path, sizeof (path));
				(void) sprintf(strchr(path, 0), "/%s@%s",
						np, ap);

				if (((dxp = find_node(path, active_node))
				    != 0) &&
				    find_prop_node(dxp, "$at", 0)) {
					/*
					 *  Changing this node's name or
					 *  address will result in a duplicate
					 *  node specification.  Generate error
					 *  message and bail out.
					 */
					printf("setprop: duplicate address\n");
					return (BOOT_FAILURE);
				} else {
					/*
					 *  Changing the node specification
					 *  via its "name" or "$at" property
					 *  will force us to re-sort this
					 *  branch of the tree the next time
					 *  we proceess it.  Clearing the
					 *  parent node's max child name
					 *  length will remind us to do so.
					 */
					pp->dn_maxchildname = 0;
				}
			} else {
				/*
				 *  Nodes must have a "name" property before
				 *  they can acquire a "$at" property (and
				 *  both the name and the address must be
				 *  non-empty).
				 */
				printf("setprop: name must set name "
				    "before $at \n");
				return (BOOT_FAILURE);
			}
		}
	}

	if (sp = find_special(dnp, name)) {
		/*
		 *  A special property.  If it has a "put" method, use that to
		 *  set the new value.  Otherwise, generate an error.
		 */
		if (sp->put) {
			/* Property has a "put" method, use it */
			return ((sp->put)(dnp, value, len, sp->putarg));
		} else {
			/* Caller is trying to modify a read-only property */
			printf("setprop: read-only property\n");
			return (BOOT_FAILURE);
		}
	} else if (!(dpp = find_prop_node(dnp, name, 0))) {
		/*
		 *  Property node doesn't exist, create a new one.  This means
		 *  allocating enough memory to hold the new "dprop" node,
		 *  setting the appropriate fields, then re-calling
		 *  "find_prop_node" to add the new node to the AVL tree.
		 */
		if (!(dpp = prop_alloc(tot_size))) {
			/*
			 *  Can't get memory for the new property node.
			 *  Go to common error handling code, below.
			 */
			goto xit;
		}

		dpp->dp_size = tot_size;
		dpp->dp_namsize = nam_size;
		(void) strcpy(dp_name(dpp), name);

		(void) find_prop_node(dnp, name, dpp);

	} else if (dpp->dp_valsize < len) {
		/*
		 *  Property node exists, but it's not big enough to hold the
		 *  value we're trying to store there.  We have to do what
		 *  amounts to a "kmem_realloc" to obtain a bigger buffer,
		 *  after which we have to patch the "dp_link" in our parent
		 *  and "dp_parent" links in both our descendents.
		 */
		struct dprop *dxp;
		int j = compare(dnp, dp_name(dpp), dpp->dp_parent);

		if (!(dxp = prop_alloc(tot_size))) {
			/*
			 *  Can't get enough memory for the new property
			 *  value.  Deliver appropriate error.
			 */
xit:
			printf("setprop: no memory\n");
			return (BOOT_FAILURE);
		}

		/*
		 *  Make a copy of the node and then free the old buffer
		 */
		(void) memcpy(dxp, dpp, dpp->dp_size);
		prop_free(dpp, dpp->dp_size);

		dpp = dxp;
		dpp->dp_size = tot_size;

		/*
		 *  Fix parent node's "dp_link" field.  The "j"
		 *  register tells which one it is.
		 */
		dxp = dpp->dp_parent;
		dxp->dp_link[j].address = dpp;
		dxp->dp_link[j].heavy = 1;

		if ((dxp = addr(dpp->dp_link[left])) != 0)
			dxp->dp_parent = dpp;
		if ((dxp = addr(dpp->dp_link[rite])) != 0)
			dxp->dp_parent = dpp;
	}

	(void) memcpy(dp_value(dpp), value, dpp->dp_valsize = len);
	return (BOOT_SUCCESS);
}
