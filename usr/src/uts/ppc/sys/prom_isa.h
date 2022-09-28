/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PROM_ISA_H
#define	_SYS_PROM_ISA_H

#pragma ident	"@(#)prom_isa.h	1.6	96/10/15 SMI"

#include <sys/obpdefs.h>

/*
 * This file contains external ISA-specific promif interface definitions.
 * There may be none.  This file is included by reference in <sys/promif.h>
 *
 * This version of the file is for 32 bit PowerPC implementations.
 */

#ifdef	__cplusplus
extern "C" {
#endif

typedef	int	cell_t;

#define	p1275_ptr2cell(p)	((cell_t)((void *)(p)))
#define	p1275_int2cell(i)	((cell_t)((int)(i)))
#define	p1275_uint2cell(u)	((cell_t)((unsigned int)(u)))
#define	p1275_phandle2cell(ph)	((cell_t)((phandle_t)(ph)))
#define	p1275_dnode2cell(d)	((cell_t)((dnode_t)(d)))
#define	p1275_ihandle2cell(ih)	((cell_t)((ihandle_t)(ih)))
#define	p1275_ull2cell_high(ll)	(0LL)
#define	p1275_ull2cell_low(ll)	((cell_t)(ll))

#define	p1275_cell2ptr(p)	((void *)((cell_t)(p)))
#define	p1275_cell2int(i)	((int)((cell_t)(i)))
#define	p1275_cell2uint(u)	((unsigned int)((cell_t)(u)))
#define	p1275_cell2phandle(ph)	((phandle_t)((cell_t)(ph)))
#define	p1275_cell2dnode(d)	((dnode_t)((cell_t)(d)))
#define	p1275_cell2ihandle(ih)	((ihandle_t)((cell_t)(ih)))
#define	p1275_cells2ull(h, l)	((unsigned long long)(cell_t)(l))


#define	p1275_cif_handler	p1275_ppc_cif_handler

extern int	p1275_cif_handler(void *);

/* structure used for initializing callback handlers */

struct callbacks {
	char *name;		/* callback service name */
	int (*fn)();		/* handler for the service */
};

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_PROM_ISA_H */
