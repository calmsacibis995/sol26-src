/*
 * Copyright (c) 1994-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

#ident "@(#)devtree.c	1.15	96/07/30 SMI"

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/salib.h>
#include "devtree.h"

#define	LINESIZE   80		/* Should probably come from BIOS! 	*/

extern struct bootops bootops;	    /* Ptr to bootop vector		*/
extern int bkern_mount(struct bootops *bop, char *dev, char *mpt, char *type);
extern int bgetproplen(struct bootops *, char *, phandle_t);
extern int bgetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern int bsetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern int get_bin_prop(char *cp, char **bufp, char *cmd);

/* BEGIN CSTYLED */
/*
 *  Device Tree Skeleton:
 *
 *     The "standard" device tree nodes are allocated statically.  Tree
 *     linkage is initialized statically as well, but properties are
 *     established via the "setup_devtree" routine.  The initial tree
 *     looks like this:
 *
 *			            [ root ]
 *			               |
 *			               |
 *    +-------+-------+----------+-----+-----+---------+---------+--------+
 *    |       |       |          |           |         |         |        |
 *    |       |       |          |           |         |         |        |
 *  +boot  aliases  chosen  i86pc-memory  i86pc-mmu  openprom  options  packages
 *    |
 *    |
 *  memory
 */
/* END CSTYLED */

struct dnode devtree[DEFAULT_NODES] = {

	{ (struct dnode *)0, &boot_node, (struct dnode *)0 },	/* root */

	{ &root_node, &bootmem_node,	&alias_node	   },	/* +boot */
	{ &boot_node, (struct dnode *)0, (struct dnode *)0 },	/* memory */

	{ &root_node, (struct dnode *)0, &chosen_node	   },	/* aliases */
	{ &root_node, (struct dnode *)0, &mem_node	   },	/* chosen  */
	{ &root_node, (struct dnode *)0, &mmu_node	   },	/* i86-mem */
	{ &root_node, (struct dnode *)0, &prom_node	   },	/* i86-mmu */
	{ &root_node, (struct dnode *)0, &option_node	   },	/* openprom */
	{ &root_node, (struct dnode *)0, &package_node	   },	/* options */
	{ &root_node, (struct dnode *)0, &delayed_node	   },	/* packages */
	{ &root_node, (struct dnode *)0, (struct dnode *)0 }	/* delayed */
};

static char *node_names[DEFAULT_NODES] = {
	"", "+boot", "memory", "aliases", "chosen",
	"i86pc-memory", "i86pc-mmu", "openprom",
	"options", "packages", "delayed-writes"
};

struct dnode *active_node = &option_node;

void
setup_devtree()
{
	/*
	 *  Initialize the device tree:
	 *
	 *    P1275 "standard" nodes are statically allocated and linked to-
	 *    gether in the tables above.  We name them here.
	 */
	int n;
	char **nnp = node_names;
	struct pprop *ppp = pseudo_props;

	for (n = 0; n < DEFAULT_NODES; n++, nnp++) {
		/* Set up the default node names */
		(void) bsetprop(&bootops, "name", *nnp,
			strlen(*nnp)+1, &devtree[n]);
	}

	/* Give the root node its name */
	/* TBD:  We should really be probing the bios to find mfgr/model! */
	(void) bsetprop(&bootops, "name", "i86pc", 6, &root_node);

	for (n = pseudo_prop_count; n; n--) {
		/*
		 *  Warning: Hack alert!!
		 *
		 *  Set dummy pseudo-properties at selected nodes so that the
		 *  ".properties" commands will appear to display pseudo
		 *  properties associated with these nodes.  These properties
		 *  are not actually stored in the nodes' property lists, but
		 *  are extracted from boot memory by special routines
		 *  (see "bootprop.c").
		 */
		char buf[MAX1275NAME+1];

		(void) sprintf(buf, "%s ", ppp->name);
		(void) bsetprop(&bootops, buf, 0, 0, (ppp++)->node);
	}

	/*
	 *  Set default values for some important properties
	 */
	(void) bsetprop(&bootops, "reg", "0,0", 4, &mmu_node);
	(void) bsetprop(&bootops, "reg", "0,0", 4, &package_node);

#define	kbd "keyboard"		/* Default input device	*/
#define	scr "screen"		/* Default ouput device */
#define	mod "9600,8,n,1,-"	/* Default serial modes */
#define	btree "/platform/i86pc/boot"	/* Default real mode root path */

	(void) bsetprop(&bootops, "input-device", kbd, sizeof (kbd),
		&option_node);
	(void) bsetprop(&bootops, "output-device", scr, sizeof (scr),
		&option_node);
	(void) bsetprop(&bootops, "ttya-mode", mod, sizeof (mod),
		&option_node);
	(void) bsetprop(&bootops, "ttyb-mode", mod, sizeof (mod),
		&option_node);
	(void) bsetprop(&bootops, "boottree", btree, sizeof (btree),
		&chosen_node);
}

static void
format_dev_addr(struct dnode *dnp, char *buf, int flag)
{
	/*
	 *    Format a device address:
	 *
	 *    Formats the device address associated with the node at "dnp" into
	 *    the "buf"fer provided.  If the "flag" is non-zero, nodes with no
	 *    address are formatted as "@0,0".  Otherwise, we place a null
	 *    string in the output buffer.
	 */
	extern struct dprop *find_prop_node();
	struct dprop *dpp;

	if (dpp = find_prop_node(dnp, "$at", 0)) {
		/*
		 *  If this node has an "$at" property, use the corresponding
		 *  value as the node address.
		 */
		(void) sprintf(buf, "@%s", dp_value(dpp));

	} else if ((flag != 0) || find_prop_node(dnp, "reg", 0) ||
	    find_prop_node(dnp, "reg ", 0)) {
		/*
		 *  IF node has a "reg" property (or the caller claims there's
		 *  an address we can't find) use the default address, "0,0".
		 */
		(void) strcpy(buf, "@0,0");
	} else {
		/*
		 *  Node has no address component.  Return a null string.
		 */
		*buf = '\0';
	}
}

char *
build_path_name(struct dnode *dnp, char *buf, int len)
{
	/*
	 *  Construct device path:
	 *
	 *    This routine recursively builds the device path associated with
	 *    the node at "dnp", placing the result in indicated "buf"fer which
	 *    is "len" bytes long.  If "buf" is NULL, it prints the path name
	 *    rather than copying it into the buffer.
	 */
	char *bend = &buf[len-1];
	char *cp = "/";

	if (dnp != &root_node) {
		/*
		 *  Recursively call this routine until we reach the root node.
		 *  Then copy each path component into the buffer [or write it
		 *  to stdout] as we unwind the stack frames.  Each invocation
		 *  returns the the "buf"fer location immediately behind the
		 *  node name it copies over.
		 *
		 *  Statically allocating the name buffer saves stack space.
		 */
		static char nbuf[MAX1275NAME+MAX1275ADDR];
		char *nnp = nbuf;

		cp = nnp + (dnp->dn_parent == &root_node);
		buf = build_path_name(dnp->dn_parent, buf, len);
		(void) bgetprop(&bootops, "name", nnp+1, MAX1275NAME, dnp);
		*nnp = '/';

		format_dev_addr(dnp, strchr(nnp, '\0'), 0);
	}

	if (buf != (char *)0) {
		/*
		 *  Copy this node name into the output buffer with a lead-
		 *  ing slash and a trailing null.  Also update the "buf"
		 *  pointer along the way.
		 */

		while (*cp && (buf < bend)) *buf++ = *cp++;
		*buf = '\0';

	} else {
		/*
		 *  Print the next path component to stdout!
		 */

		printf(cp);
	}

	return (buf);
}

static int
sort_nodes(struct dnode *parent)
{
	/*
	 *  Sort device tree nodes:
	 *
	 *    This routine is used by the "ls" and "show-devs" (if and when I
	 *    get around to implementing it) commands to sort a peer chain
	 *    into node name sequence.  It returns the length the longest
	 *    name encountered while doing this.
	 *
	 *    If the parent node's "maxchildname" field contains a
	 *    non-negative value, it means we've already sorted this peer
	 *    chain and can simply return the "maxchildname" value.
	 */
	int i, j, k = parent->dn_maxchildname;
	int done = (k > 0);

	struct dnode *dnp;
	char node_name[MAX1275NAME], prev_name[MAX1275NAME];
	char node_addr[MAX1275ADDR], prev_addr[MAX1275ADDR];

	while (!done) {
		/*
		 *  Main bubble sort loop.  OK, so bubble sorts are slow.
		 *  But they're easy to code (and get right) and perform
		 *  reasonably well if there's not too much data to sort
		 *  or if the data is almost sorted already. Since we re-order
		 *  the list as we sort it, the latter condition
		 *  usually holds.  The most likely reason for the list to be
		 *  out of sequence is that someone has issued a "mknod"
		 *  command, in which case the new node is at the front of the
		 *  list, and the bubble sort below is a fairly efficient way
		 *  to move it to it's proper position.
		 *
		 *  The only time this routine is likely to perform poorly is
		 *  when someone renames the node at the end of a peer list so
		 *  that it has to be moved to the front of the list.  This is
		 *  such a rare event (the kernel will never do it) that I've
		 *  decided to ignore it.
		 */
		struct dnode *dyp = (struct dnode *)0;
		struct dnode *dxp = (struct dnode *)
		    ((char *)&parent->dn_child -
		    (int)&((struct dnode *)0)->dn_peer);

		k = 0;
		done = 1;

		for (dnp = parent->dn_child; dnp; dnp = dnp->dn_peer) {
			/*
			 *  Make another pass over the input, looking for the
			 *  node with the longest name (saved to "k" register)
			 *  and verifying that the list is sorted.
			 */
			format_dev_addr(dnp, node_addr, 1);
			j = bgetprop(&bootops, "name", node_name,
			    sizeof (node_name), dnp);

			if (--j <= 0)
				node_name[j = 0] = 0;

			if ((j += strlen(node_addr)) > k)
				k = j;

			if (dyp && (((i = strcmp(node_name, prev_name)) < 0) ||
			    (!i && (strcmp(node_addr, prev_addr) < 0)))) {
				/*
				 *  This entry is out of sequence.  Swap it
				 *  with the previous node and clear the "done"
				 *  flag to ensure another pass over the data.
				 */
				dxp->dn_peer = dnp->dn_peer;
				dnp->dn_peer = dxp;
				dyp->dn_peer = dnp;
				dyp = dnp;
				dnp = dxp;
				done = 0;
			} else if (dnp->dn_peer) {
				/*
				 *  So far so good.  Move on to next node.
				 */
				(void) strcpy(prev_name, node_name);
				(void) strcpy(prev_addr, node_addr);
				dyp = dxp;
				dxp = dnp;
			}
		}
	}

	return (parent->dn_maxchildname = k);
}

struct dnode *
find_node(char *path, struct dnode *anp)
{
	/*
	 *  Convert pathname to device node:
	 *
	 *    This is the equivalent of the kernel's "namei" routine.  It
	 *    traverses the indicated pathname and returns a pointer to the
	 *    device tree node identified by that path.
	 */
	int j, k, x;
	char *cp, *xp = 0;
	struct dnode *dnp;

	if (*(cp = path) != '/') {
		/*
		 *  Path name is relative, check to see if first component is
		 *  an alias!  If there are any device args, they apply to the
		 *  device named by the alias.
		 */
		while (*cp && (*cp != '/') && (*cp != ':')) cp++;
		j = *cp; *cp = '\0';

		if (((k = bgetproplen(&bootops, path, &alias_node)) > 0) &&
		    (xp = bkmem_alloc(x = k+strlen(cp+1)+1))) {
			/*
			 *  Yes, we have an alias!  Buy a buffer large enough
			 *  to contain the alias value followed by the
			 *  remaining pathname components and load the alias
			 *  value into it.  Concatenate the remainder of the
			 *  path after restoring the termination character.
			 */
			(void) bgetprop(&bootops, path, xp, k, &alias_node);
		}

		*cp = j;
		if (xp)
			(void) strcat(path = xp, cp);
	}

	for (dnp = ((*path == '/') ? &root_node : anp); dnp; path = cp) {
		/*
		 *  The "dnp" register points to the device node corresponding
		 *  to that portion of the path that's already been traversed.
		 */
		int l;
		char *ap;

		while (*path && (*path == '/')) path++;
		for (cp = path;
		    *cp && *cp != '/' && *cp != '@' && *cp != ':';
		    cp++);

		if ((l = (cp - path)) <= 0) {
			/*
			 *  We've exhausted the input.  The current node
			 *  ("dnp") is the one the caller wants!
			 */
			break;
		}

		/*
		 *  Sort children of the current node to make device address
		 *  resolution a bit easier.  Then set "ap" to point to the
		 *  address portion of the next path component.  This may be
		 *  taken from the input pathname or it may be the default
		 *  address: "0,0".
		 */
		k = 0;
		(void) sort_nodes(dnp);
		ap = ((*cp == '@') ? cp : "@0,0");
		while (ap[k] && (ap[k] != '/') && (ap[k] != ':')) k++;

		if ((l == 2) && (*cp != '@') && (strncmp(path, "..", 2) == 0)) {
			/*
			 *  If next path component is "..", step back up
			 *  the tree.  If we're at the root, ".." is equivalent
			 *  to ".".
			 */
			if (dnp->dn_parent) dnp = dnp->dn_parent;

		} else if ((l > 1) || (*path != '.') || (*cp == '@')) {
			/*
			 *  If next path component is a real node name
			 *  (not "." or ".."), search the current node's
			 *  child list.
			 */
			for (dnp = dnp->dn_child; dnp; dnp = dnp->dn_peer) {
				/*
				 *  Step thru the current node's child list
				 *  looking for a node that matches the next
				 *  component of the path name.  The "l"
				 *  register holds the length of this component.
				 */
				unsigned char child_name[MAX1275NAME];
				unsigned char child_addr[MAX1275ADDR];

				(void) bgetprop(&bootops, "name",
					(char *)child_name, MAX1275NAME, dnp);
				format_dev_addr(dnp, (char *)child_addr, 1);

				if (!(j = strncmp(path,
				    (char *)child_name, l)) &&
				    !(j = (0 - child_name[l])) &&
				    !(j = strncmp(ap, (char *)child_addr, k)) &&
				    !(j = (0 - child_addr[k]))) {
					/*
					 *  Name and address components match.
					 *  This is the node we want, so move
					 *  on to the next component of
					 *  the path name.
					 */
					break;

				} else if (j < 0) {
					/*
					 *  There does not appear to be a
					 *  device with the given name at this
					 *  level of the hierarchy.  Return the
					 *  "not found" indication.
					 */
					dnp = (struct dnode *)0;
					break;
				}
			}
		}

		if ((*cp == '@') || (*cp == ':')) {
			/*
			 *  Skip over the address portion (if any) of the
			 *  current path component.
			 */
			while (*cp && (*cp != '/')) cp++;
		}
	}

	if (xp) bkmem_free(xp, x);
	return (dnp);
}

/*
 *  Device tree bootops:
 *
 *     These routines are called through the bootops vector.  Most are
 *     very straightforward ...
 */

/*ARGSUSED*/
phandle_t
bpeer(struct bootops *bop, phandle_t node)
{
	/* Return handle for given node's next sibling */
	return (node ? (phandle_t)((struct dnode *)node)->dn_peer : &root_node);
}

/*ARGSUSED*/
phandle_t
bchild(struct bootops *bop, phandle_t node)
{
	/* Return handle for given node's first child */
	return ((phandle_t)((struct dnode *)node)->dn_child);
}

/*ARGSUSED*/
phandle_t
bparent(struct bootops *bop, phandle_t node)
{
	/* Return handle for given node's parent */
	return ((phandle_t)((struct dnode *)node)->dn_parent);
}

/*ARGSUSED*/
phandle_t
bmknod(struct bootops *bop, phandle_t node)
{
	/*
	 *  Create a device tree node:
	 *
	 *  The new node becomes the first child of the specified parent "node";
	 *  we can't sort it into the peer list yet because it doesn't have a
	 *  "name" property!
	 *
	 *  Returns a pointer to the new node or NULL if we can't get memory.
	 *  Also makes the new node the active node.
	 */

	struct dnode *dnp = (struct dnode *)bkmem_alloc(sizeof (struct dnode));

	if (dnp != (struct dnode *)0) {
		/*  We've got the memory, now link into the tree */
		dnp->dn_proplist.address = 0;
		dnp->dn_child = (struct dnode *)0;
		dnp->dn_parent = (struct dnode *)node;
		dnp->dn_peer = ((struct dnode *)node)->dn_child;

		((struct dnode *)node)->dn_child = dnp;
		active_node = dnp;

	} else {
		/*  No memory, print error message */
		printf("mknod: no memory\n");
	}

	return ((phandle_t)dnp);
}

/*
 *  Stubs:
 *
 *	The following bootops are not yet implemented:
 */

/*ARGSUSED*/
ihandle_t
bmyself(struct bootops *bop)
{
	/*  Return an ihandle for active node.				*/
	return (0);
}

/*ARGSUSED*/
int
binst2path(struct bootops *bop, ihandle_t dev, char *path, int len)
{
	/*  Convert an instance (ihandle) to a path name		*/
	return (0);
}

/*ARGSUSED*/
phandle_t
binst2pkg(struct bootops *bop, ihandle_t dev)
{
	/*  Convert an instance (ihnadle) to a package (phandle)	*/
	return (0);
}

/*ARGSUSED*/
int
bpkg2path(struct bootops *bop, phandle_t node, char *path, int len)
{
	/*  Convert a package (phandle) to a path name			*/
	return (0);
}

/*
 *  Boot interpreter commands:
 *
 *      These boot interpreter commands may be used to navigate the device
 *	tree.  They are also used by realmode code to construct the device
 *	tree in the first place (aside from the "standard" nodes that were
 *      established by "setup_devtree").
 */

void
dev_cmd(int argc, char **argv)
{
	/*
	 *  Set the active node:
	 *
	 *  This is similar to the UNIX "cd" command, except that the (optional)
	 *  argument is a device tree path rather than a file path.  If the path
	 *  argument is omitted, "/chosen" is assumed.
	 */
	char *path = ((argc > 1) ? argv[1] : "/chosen");
	struct dnode *dnp = find_node(path, active_node);

	if (dnp) {
		/*
		 *  If "find_node" was able to locate the dnode struct,
		 *  it becomes the new active node ...
		 */
		active_node = dnp;

	} else {
		/*
		 *  Otherwise, we have a user error:
		 */

		printf("%s not found\n", argv[1]);
	}
}

/*ARGSUSED*/
void
pwd_cmd(int argc, char **argv)
{
	/*
	 *  Print active node name:
	 *
	 *  The "build_path_name" routine does the real work; all we have to do
	 *  is add the trailing newline.
	 */
	(void) build_path_name(active_node, 0, 0);
	printf("\n");
}

/*ARGSUSED*/
void
ls_cmd(int argc, char **argv)
{
	/*
	 *  Print names of all children of the active node:
	 *
	 *    This is much like the UNIX "ls" command, except that it doesn't
	 *    support fifty-jillion options.
	 */

	struct dnode *dnp;
	char node_name[MAX1275NAME+MAX1275ADDR];
	int j, k, n = sort_nodes(active_node)+2;

	j = 0;			/* "j" is no. of names on current line */
	n += 3;			/* "n" is length of each name		   */
	k = LINESIZE/n;	/* "k" is max names per output line	   */
	if (k == 0) k = 1;

	for (dnp = active_node->dn_child; dnp; dnp = dnp->dn_peer) {
		/*
		 *  Print the child list across the screen.  We allow no
		 *  less than four spaces between each name.
		 */
		int x = bgetprop(&bootops, "name", node_name,
		    sizeof (node_name), dnp);
		char *cp = &node_name[x-1];

		format_dev_addr(dnp, cp, 0);
		while (*cp) (cp += 1, x += 1);
		while (x++ < n) *cp++ = ' ';
		*cp = '\0';

		printf("%s", node_name);

		if ((++j >= k) || !dnp->dn_peer) {
			/*  That's it for this line! */
			printf("\n");
			j = 0;
		}
	}
}

/*
 * Free any dynamically created device nodes below this branch.
 *
 * We make use of the the fact that there are no statically
 * created nodes below any dynamic ones.
 */
static struct dnode *
free_dnodes(struct dnode *dnp)
{
	struct dnode *peer = dnp->dn_peer;
	extern void free_props();

	if (dnp->dn_child) {
		(void) free_dnodes(dnp->dn_child);
	}

	if (peer) {
		dnp->dn_peer = peer = free_dnodes(peer);
	}

	if ((dnp < &devtree[0]) || (dnp > &devtree[DEFAULT_NODES - 1])) {
		dnp->dn_parent->dn_child = peer;
		free_props(addr(dnp->dn_proplist));
		bkmem_free((caddr_t)dnp, sizeof (struct dnode));
		return (peer);
	} else {
		if (dnp->dn_parent) { /* check for non root node */
			dnp->dn_parent->dn_child = dnp;
		}
		return (dnp);
	}
}

/*ARGSUSED*/
void
resetdtree_cmd(int argc, char **argv)
{
	/*
	 * Reset the device tree to its initial state.
	 * Delete any dynamically created device nodes, by walking
	 * the device tree and bkmem_free all dnodes except the
	 * default dnodes (devtree[0] - devtree[DEFAULT_NODES - 1]).
	 *
	 * For each dnode freed also free all associated properties.
	 */
	if (argc > 1) {
		printf("no arguments to resetdtree_cmd\n");
		return;
	}
	/*
	 * recursively delete dynamically created device nodes
	 * starting at the root
	 */
	(void) free_dnodes(&devtree[0]);
}

void
mknod_cmd(int argc, char **argv)
{
	/*
	 *  Create a device node:
	 *
	 *    This is analogus to the UNIX "mknod" command.  It creates the
	 *    node named by its first argument and sets its "reg" property
	 *    to the value of the second argument (if the second argument is
	 *    omitted, no "reg" property is created).  When present, the second
	 *    argument must be a comma-separated integer list a-la the
	 *    "setbinprop" command.
	 *
	 *    This command has the side effect of changing the active node to
	 *    the newly created node (unless it fails, of course).
	 */
	if (argc >= 2) {
		/*
		 *  User-supplied path name is in "argv[1]".   The last part
		 *  of this string becomes the "name" property of the new node.
		 */
		struct dnode *dxp, *dnp = active_node;
		struct dnode *save = active_node;
		char *path = argv[1];
		char *regs = 0;
		int n = -1;
		char *cp;

		if ((argc > 2) &&
		    ((n = get_bin_prop(argv[2], &regs, "mknod")) < 0)) {
			/*
			 *  Second argument (reg list) is ill-formed.
			 *  The "get_bin_prop" routine has already issued an
			 *  error message.
			 */
			return;
		}

		if (cp = strrchr(path, '/')) {
			/*
			 *  Non-simple path name.  Use the "find_node" routine
			 *  to locate the parent node to which we'll attach the
			 *  new node.
			 */
			*cp = '\0';	/* Remove the last slash! */

			if (cp == path) {
				/*
				 *  New node is to be a child of the root!
				 */
				dnp = &root_node;
			} else if (!(dnp = find_node(path, active_node))) {
				/*
				 *  Path was bogus.  Print an error message
				 *  and bail out.
				 */
				printf("%s not found\n", path);
				return;
			}

			path = cp+1; /* Node name is last part of path name */
		}

		if (strchr(path, '@') || strchr(path, ':')) {
			/*
			 *  These characters are not valid in a device name!
			 */
			printf("mknod: bogus path name\n");
			return;
		}

		if (dxp = (struct dnode *)bmknod(&bootops, dnp)) {
			/*
			 *  The "mknod" bootop created the node for us, all we
			 *  have to do now is name it.  The only way this could
			 *  fail is if we were to run out of memory.
			 */
			if (bsetprop(&bootops, "name", path,
			    strlen(path)+1, dxp) ||
			    ((n >= 0) &&
				bsetprop(&bootops, "reg", regs, n, dxp))) {
				/*
				 *  No memory left; "bsetprop" already
				 *  generated the error message, but we have
				 *  to unlink the partially created dev node.
				 *  Also restore the active node pointer JIC
				 *  "bmknod" changed it!
				 */
				active_node = save;
				dnp->dn_child = dxp->dn_peer;
				bkmem_free((caddr_t)dxp, sizeof (struct dnode));
			}
		}

		if (n > 0) {
			/*
			 *  We have a register property buffer that needs
			 *  to be freed.
			 */
			bkmem_free(regs, n);
		}

	} else {
		/*
		 *  User forgot the pathname argument.
		 */

		printf("usage: mknode path [regs]\n");
	}
}

struct nlink			/* Path name changing element used by	*/
{				/* .. "show-devs" command.		*/
	struct nlink *next;	/* .. .. ptr to next element		*/
	char *name;		/* .. .. name of this element		*/
};

static void
show_nodes(struct dnode *dnp, struct nlink *lp, struct nlink *root)
{
	/*
	 *  Depth-first device tree search:
	 *
	 *    This recursive routine is used by the "show-devs" command to
	 *    search the device tree in a depth-first manner, looking for any
	 *    and all nodes with a "$at" property.  Such nodes are assumed to
	 *    be "real" device nodes, and the corresponding path names are
	 *    printed.
	 */
	(void) sort_nodes(dnp);
	for (dnp = dnp->dn_child; dnp; dnp = dnp->dn_peer) {
		/*
		 *  Check all children of the current node.  Those with a "$at"
		 *  property are printed before we descend recursively.
		 */
		char *buf;
		int j = bgetproplen(&bootops, "name", dnp);

		if (buf = bkmem_alloc(j+MAX1275ADDR)) {
			/*
			 *  We have a buffer into which we can place the
			 *  current node name.  Build it up and see if we
			 *  should print it now.  Then add a "/" to the end
			 *  of the name buffer and recursively descend
			 *  into the named node.
			 */
			struct nlink link, *nlp;
			lp->next = &link;
			link.name = buf;
			link.next = 0;

			(void) bgetprop(&bootops, "name", buf, j, dnp);
			format_dev_addr(dnp, &buf[j-1], 0);

			if (buf[j-1] != '\0') {
				/*
				 *  This node has a "$at" property, which means
				 *  that it's a true device node and we should
				 *  print its name.
				 */
				for (nlp = root; nlp; nlp = nlp->next)
					printf("%s", nlp->name);
				printf("\n");
			}

			(void) strcat(buf, "/");
			show_nodes(dnp, &link, root);

			bkmem_free(buf, j+MAX1275ADDR);
		}
	}
}

void
show_cmd(int argc, char **argv)
{
	/*
	 *  List configured devices:
	 *
	 *    This command produces a depth-first list of all nodes below the
	 *    target that have a "$at" property [i.e, that correspond to real
	 *    devices].  The target node is either the node specified by the
	 *    first argument, or the active node when the argument is omitted.
	 */
	struct dnode *dnp = ((argc > 1) ? find_node(argv[1], active_node) :
	    active_node);

	if (dnp != 0) {
		/*
		 *  The "dnp" register now points to the target node.  Build
		 *  up the initial path name and use the "show_nodes" routine
		 *  above to print the names of all device nodes below the
		 *  target.
		 */
		int j;
		int plen = 0;
		char *path = "";
		struct nlink link;

		if (argc > 1) {
			/*
			 *  Caller has supplied a target node name.  It may or
			 *  may not have a trailing slash.  If so, we can use
			 *  it as-is ...
			 */
			j = strlen(argv[1]);

			if ((*((path = argv[1])+j-1) != '/') &&
			    (path = bkmem_alloc(plen = j+2))) {
				/*
				 *  ... but if not, we have to copy it into a
				 *  slightly bigger buffer and insert the
				 *  trailing slash ourselves.
				 */
				(void) strcpy(path, argv[1]);
				path[j++] = '/';
				path[j] = '\0';

			} else if (!path) {
				/*
				 *  Error message if we run out of memory here.
				 *  But we're silent about failing mallocs from
				 *  this point on!
				 */
				printf("show-devs: no memory\n");
				return;
			}
		}

		link.name = path;
		link.next = 0;

		show_nodes(dnp, &link, &link);
		if (plen) bkmem_free(path, plen);

	} else {
		/*
		 *  Target node specification was bogus!
		 */

		printf("show-devs: %s not found\n", argv[1]);
	}
}

void
mount_cmd(int argc, char **argv)
{
	/*
	 *  Mount command:
	 *
	 *    The mount bootop does the real work.
	 */
	if (argc > 2) {
		/*
		 *  Call the "mount" bootop, which will print an error
		 *  message if it finds something wrong.
		 */

		(void) bkern_mount(&bootops, argv[1], argv[2],
		    (argc > 3) ? argv[3] : 0);
	} else {
		/*
		 *  Bogus command syntax.
		 */

		printf("usage: mount device path [type]\n");
	}
}
