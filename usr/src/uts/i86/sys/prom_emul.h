/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PROM_EMUL_H
#define	_SYS_PROM_EMUL_H

#pragma ident	"@(#)prom_emul.h	1.1	96/05/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The following structure describes a property attached to a node
 * in the in-kernel copy of the PROM device tree.
 */
struct prom_prop {
	struct prom_prop *pp_next;
	char		 *pp_name;
	int		 pp_len;
	caddr_t		 pp_val;
};

/*
 * The following structure describes a node in the in-kernel copy
 * of the PROM device tree.
 */
struct prom_node {
	struct prom_prop *pn_propp;
	struct prom_node *pn_child;
	struct prom_node *pn_sibling;
};

typedef struct prom_node prom_node_t;
typedef struct prom_node *prom_node_handle_t;


#ifdef	__cplusplus
}
#endif

#endif /* _SYS_PROM_EMUL_H */
