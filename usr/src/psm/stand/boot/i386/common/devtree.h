/*
 * Copyright (c) 1994-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef _DEVTREE_H
#define	_DEVTREE_H

#pragma ident	"@(#)devtree.h	1.9	96/09/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct dprop;			/* Forward referece to remove warnings	*/

typedef union
{
	/*
	 *  AVL tree pointer type:
	 *
	 *  The low order bit of each AVL tree pointer word is used to indicate
	 *  that the corresponding side of the tree is "heavy".  This union hack
	 *  relies on peculiarities of little-endian bit fields:  The "heavy"
	 *  bit must occupy the low-order bit position!
	 */
	struct dprop *address;	/* "ptr" used as an address		*/
	unsigned int  heavy:1;	/* "ptr" used as a "heavy" flag		*/

#define	addr(p) ((struct dprop *)((unsigned)((p).address) & ~1))

} ptr;

struct dprop
{
	/*
	 *  Property list node:
	 *
	 *  There is one of these structs associated with each property assigned
	 *  to a given node.  They hang in an AVL tree that's rooted in the
	 *  corresponding device tree node.
	 *
	 *  A pair of pseudo-methods define computed values:
	 *
	 *     dp_getflag:   A property node's balance flag is -1 if the subtree
	 *     dp_setflag:   rooted at that node is balanced, 0 if the left
	 *		     subtree is heavy, and 1 if the right subtree is
	 *		     heavy.
	 *
	 *		     This allows balance flags to be used to index into
	 *		     the "dp_link" array.
	 */
	struct dprop *dp_parent; /* Used for tree traversal		*/

	ptr	dp_link[2];	/* AVL tree link fields			*/
#define	left    0	/* .. Points to the left subtree	*/
#define	rite    1	/* .. Points to the right subtree	*/

#define	dp_getflag(p)	/* Return property node's balance flag	*/ \
	((p)->dp_link[rite].heavy ? 1: ((int)(p)->dp_link[left].heavy-1))

#define	dp_setflag(p, f)	/* Set property node's balance flag	*/ \
	{ \
	    (p)->dp_link[left].heavy = (p)->dp_link[rite].heavy = 0;	\
	    if ((f) >= 0) (p)->dp_link[f].heavy = 1;			\
	}
#define	dp_setflag_nohvy(p)	/* Set property node's balance flag	*/ \
	{ \
	    (p)->dp_link[left].heavy = (p)->dp_link[rite].heavy = 0;	\
	}

	short  dp_namsize;	/* Size of prop's name (inlcudes null)	*/
	int    dp_valsize;	/* Size of prop's value			*/
	int    dp_size;		/* Total size (of entire struct)	*/

	char   dp_buff[2];	/* Buffer contaiing name & value	*/
#define	dp_name(p)	(&(p)->dp_buff[0])
#define	dp_value(p)	(&(p)->dp_buff[(p)->dp_namsize])
};

struct dnode
{
	/*
	 *  Device Tree Node:
	 *
	 *     The boot device tree is extremely simple.  Each node consists of
	 *     nothing more a list of associated properties and the pointers
	 *     required to navigate the tree.
	 */
	struct dnode *dn_parent; /* Ptr to parent node (null for root)	*/
	struct dnode *dn_child;	 /* Ptr to 1st child (null for leaves)	*/
	struct dnode *dn_peer;	 /* Next sibling node 			*/
	int dn_maxchildname;	 /* Length of longest child name	*/

	ptr dn_proplist;	/* Head of property list AVL tree	*/
};

#define	MAX1275NAME 32		/* Max length of a prop or dev name	*/
#define	MAX1275ADDR 40		/* Maximum formatted device addr	*/

extern struct dnode devtree[];	/* Statically allocated portion of tree */

#define	root_node    	devtree[0]
#define	boot_node    	devtree[1]
#define	bootmem_node 	devtree[2]
#define	alias_node   	devtree[3]
#define	chosen_node  	devtree[4]
#define	mem_node	devtree[5]
#define	mmu_node	devtree[6]
#define	prom_node	devtree[7]
#define	option_node	devtree[8]
#define	package_node	devtree[9]
#define	delayed_node	devtree[10]
#define	DEFAULT_NODES	11

struct pprop			/* Pseudo-property definition:		*/
{
    struct dnode *node;		/* .. Node to which the prop belongs	*/
    char  *name;		/* .. The propery's name		*/
    int  (*get)();		/* .. The special "get" function	*/
    void *getarg;		/* .. Instance flag for "get" function	*/
    int  (*put)();		/* .. The special "put" function	*/
    void *putarg;		/* .. Instance flag for "put" function  */
};

extern struct pprop pseudo_props[];
extern int pseudo_prop_count;

#ifdef	__cplusplus
}
#endif

#endif /* _DEVTREE_H */
