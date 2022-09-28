/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)pdevinfo_sun4u.c 1.12	95/12/11 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <varargs.h>
#include <errno.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/openpromio.h>
#include <kstat.h>
#include "pdevinfo.h"
#include "pdevinfo_sun4u.h"
#include "display.h"
#include "display_sun4u.h"

/*
 * This file represents the splitting out of some functionality
 * of prtdiag due to the port to the sun4u platform. The PROM
 * tree-walking functions which contain sun4u specifics were moved
 * into this module. Analogous functions should reside in the sun4d
 * source tree.
 */

int desktop = 0;
struct ffbinfo *ffb_list = NULL;

extern int get_upa_id(Prom_node *);

/* Function prototypes */
static void add_node(Sys_tree *, Prom_node *);
static void resolve_board_types(Sys_tree *);
static Prom_node *walk(Sys_tree *, Prom_node *, int);
static void find_ffb_nodes(Sys_tree *);

int
do_prominfo(int syserrlog)
{
	Sys_tree sys_tree;		/* system information */
	Prom_node *root_node;		/* root node of OBP device tree */
	struct system_kstat_data sys_kstat; /* kstats for non-OBP data */

	/* set the the system tree fields */
	sys_tree.sys_mem = NULL;
	sys_tree.boards = NULL;
	sys_tree.bd_list = NULL;
	sys_tree.board_cnt = 0;

	if (promopen(O_RDONLY))  {
		exit(_error("openeepr device open failed"));
	}

	if (is_openprom() == 0)  {
		(void) fprintf(stderr, badarchmsg);
		return (2);
	}

	if (next(0) == 0) {
		return (2);
	}

	/*
	 * set desktop == 1. the walk() routine will set it to 0
	 * if it finds a central node. This is a special node found
	 * only on servers.
	 */
	desktop = 1;
	root_node = walk(&sys_tree, NULL, next(0));
	promclose();

	/* resolve the board types now */
	resolve_board_types(&sys_tree);

	/* find all the ffb nodes and store in ffbinfo structs */
	find_ffb_nodes(&sys_tree);

	read_sun4u_kstats(&sys_tree, &sys_kstat);

	return (display(&sys_tree, root_node, &sys_kstat, syserrlog));
}

/*
 * Walk the PROM device tree and build the system tree and root tree.
 * Nodes that have a board number property are placed in the board
 * structures for easier processing later. Child nodes are placed
 * under their parents. ffb (Fusion Frame Buffer) nodes are handled
 * specially, because they do not contain board number properties.
 * This was requested from OBP, but was not granted. So this code
 * must parse the UPA MID of the FFB to find the board#.
 */
static Prom_node *
walk(Sys_tree *tree, Prom_node *root, int id)
{
	register int curnode;
	Prom_node *pnode;
	char *name;
	int board_node = 0;

	/* allocate a node for this level */
	if ((pnode = (Prom_node *) malloc(sizeof (struct prom_node))) ==
	    NULL) {
		perror("malloc");
		exit(2);	/* program errors cause exit 2 */
	}

	/* assign parent Prom_node */
	pnode->parent = root;
	pnode->sibling = NULL;
	pnode->child = NULL;

	/* read properties for this node */
	dump_node(pnode);

	/*
	 * Check for this being a server. If so, then set desktop == 0.
	 * The way we identify a server is if it has a node named central.
	 */
	name = get_node_name(pnode);
	if ((name != NULL) && (strcmp(name, "central") == 0)) {
		desktop = 0;
	}

	/*
	 * Place a node in a 'board' if it has 'board'-ness. The definition
	 * is that all nodes that are children of root should have a
	 * board# property. But the PROM tree does not exactly follow
	 * this. This is where we start hacking. The name 'ffb' can
	 * change, so watch out for this.

	 * The UltaSPARC, sbus, pci and ffb nodes will exit in
	 * the desktops and will not have board# properties. These
	 * cases must be handled here.
	 */
	if (name != NULL) {
		if (has_board_num(pnode)) {
			add_node(tree, pnode);
			board_node = 1;
		} else if ((strcmp(name, "SUNW,ffb")  == 0) ||
		    (strcmp(name, "SUNW,UltraSPARC") == 0) ||
		    (strcmp(name, "pci") == 0) ||
		    (strcmp(name, "counter-timer") == 0) ||
		    (strcmp(name, "sbus") == 0)) {
			add_node(tree, pnode);
			board_node = 1;
		}
	}

	if (curnode = child(id)) {
		pnode->child = walk(tree, pnode, curnode);
	}

	if (curnode = next(id)) {
		if (board_node) {
			return (walk(tree, root, curnode));
		} else {
			pnode->sibling = walk(tree, root, curnode);
		}
	}

	if (board_node) {
		return (NULL);
	} else {
		return (pnode);
	}
}

/*
 * Fucntion resolve_board_types
 *
 * After the tree is walked and all the information is gathered, this
 * function is called to resolve the type of each board.
 */
static void
resolve_board_types(Sys_tree *tree)
{
	Board_node *bnode;
	Prom_node *pnode;

	bnode = tree->bd_list;
	while (bnode != NULL) {
		/*
		 * try to set the board type. There is no gaurantee of having a
		 * CPU on a CPU board. So If we do not find one, look for a
		 * simm-status node. The number of sbus nodes is critical,
		 * since the FFB/SBUS board will only have one SBus, and is
		 * not gauranteed to have any FFB nodes.
		 */
		if (dev_find_type(bnode->nodes, "cpu") != NULL) {
			bnode->board_type = CPU_BOARD;
		} else if (dev_find_node(bnode->nodes, "simm-status") != NULL) {
			bnode->board_type = MEM_BOARD;
		} else if ((pnode = dev_find_node(bnode->nodes, "sbus"))
		    != NULL) {
			if (dev_next_node(pnode, "sbus") != NULL) {
				bnode->board_type = IO_2SBUS_BOARD;
			} else {
				bnode->board_type = IO_SBUS_FFB_BOARD;
			}
		} else if (dev_find_node(bnode->nodes, "pci") != NULL) {
			bnode->board_type = IO_PCI_BOARD;
		}
		bnode = bnode->next;
	}

}

/*
 * add_node
 *
 * This function adds a board node to the board structure where that
 * that node's physical component lives.
 */
void
add_node(Sys_tree *root, Prom_node *pnode)
{
	int board;
	Board_node *bnode;
	char *name = get_node_name(pnode);

	/* add this node to the Board list of the appropriate board */
	if ((board = get_board_num(pnode)) == -1) {
		void *value;

		/*
		 * If this is a desktop, then the board number is 0, but
		 * if it is a server, pci nodes and ffb nodes never have
		 * board number properties and software can find the board
		 * number from the reg property. It is derived from the
		 * high word of the 'reg' property, which contains the
		 * UPA-mid.
		 */
		if (desktop) {
			board = 0;
		} else if ((name != NULL) &&
		    ((strcmp(name, "SUNW,ffb") == 0) ||
		    (strcmp(name, "SUNW,pci") == 0) ||
		    (strcmp(name, "counter-timer") == 0))) {
			/* extract the board number from the 'reg' prop. */
			if ((value = get_prop_val(find_prop(pnode,
			    "reg"))) == NULL) {
				(void) printf("add_node() no reg property\n");
				exit(2);
			}
			board = (*(int *)value - 0x1c0) / 4;
		}
	}

	/* find the node with the same board number */
	if ((bnode = find_board(root, board)) == NULL) {
		bnode = insert_board(root, board);
		bnode->board_type = UNKNOWN_BOARD;
	}

	/* now attach this prom node to the board list */
	if (bnode->nodes != NULL)
		pnode->sibling = bnode->nodes;

	bnode->nodes = pnode;
}

/*
 * Find the UPA device on the current board with the requested device ID
 * and name. If this rountine is passed a NULL pointer, it simply returns
 * NULL.
 */
Prom_node *
find_upa_device(Board_node *board, int upa_id, char *name)
{
	Prom_node *pnode;

	/* find the first cpu node */
	pnode = dev_find_node(board->nodes, name);

	while (pnode != NULL) {
		if ((get_upa_id(pnode) & 0x1F) == upa_id)
			return (pnode);

		pnode = dev_next_node(pnode, name);
	}
	return (NULL);
}

int
get_upa_id(Prom_node *node)
{
	int *value;

	if ((value = (int *)get_prop_val(find_prop(node, "upa-portid"))) ==
	    NULL) {
		return (-1);
	}
	return (*value);
}

#define	PATH_MAX	1024

static void
find_ffb_nodes(Sys_tree *tree)
{
	DIR *dirp;
	int board;
	int upa_id;
	char *ptr;
	struct ffbinfo *ffb_data;
	struct ffbinfo *list, **vect;
	struct dirent *direntp;

	/* search /dev/fbs directory for ffb nodes */
	if ((dirp = opendir("/devices")) == NULL) {
		perror("open of /devices failed");
		return;
	}

	while ((direntp = readdir(dirp)) != NULL) {
		Board_node *bnode;

		/* see if this directory entry is an FFB */
		if (strstr(direntp->d_name, "ffb") != NULL) {
			/* parse device entry to determine board number */

			/* find the '@' in the device name */
			ptr = strchr(direntp->d_name, '@');

			/* skip over the '@' character */
			ptr++;

			/* now get the UPA ID of the FFB */
			upa_id = strtol(ptr, NULL, 16);
			board = upa_id/2;

			if (desktop) {
				if (find_upa_device(tree->bd_list, 0x1E,
				    FFB_NAME) == NULL) {
					continue;
				}
				board = 0;
			} else {
				/* Is this physical board really present? */
				if ((bnode = find_board(tree, board)) ==
				    NULL) {
					continue;
				}

				/* Is it an IOG board? */
				if (bnode->board_type != IO_SBUS_FFB_BOARD) {
					continue;
				}

				/* Is an FFB installed? */
				if (find_upa_device(bnode, board << 1,
				    FFB_NAME) == NULL) {
					continue;
				}
			}

			/* create a new FFB struct */
			if ((ffb_data = (struct ffbinfo *)
			    malloc(sizeof (struct ffbinfo))) == NULL) {
				perror("ffb data malloc failed\n");
				return;
			}

			/* fill in the details. */
			ffb_data->board = board;
			ffb_data->upa_id = upa_id;
			ffb_data->dev = strdup(direntp->d_name);
			ffb_data->next = NULL;


			/* link it into the list, in ascending board order */
			if (ffb_list == NULL) {
				ffb_list = ffb_data;
				continue;
			}

			for (list = ffb_list, vect = &ffb_list; list != NULL;
			    vect = &list->next, list = list->next) {
				/* see if this ffb should be inserted here */
				if (board < list->board) {
					ffb_data->next = list;
					*vect = ffb_data;
					break;
				}
				if (list->next == NULL) {
					list->next = ffb_data;
					break;
				}
			}
		}
	}
}
