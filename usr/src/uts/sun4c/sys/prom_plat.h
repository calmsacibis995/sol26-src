/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PROM_PLAT_H
#define	_SYS_PROM_PLAT_H

#pragma ident	"@(#)prom_plat.h	1.5	96/02/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file contains external platform-specific promif interface definitions
 * for OpenBoot(tm) on SMCC's 32 bit SPARC sun4c platform architecture.
 */

/*
 * "reg"-format for 32 bit cell-size, 2-cell physical addresses,
 * with a single 'size' cell:
 */

struct prom_reg {
	unsigned int hi, lo, size;
};

/*
 * resource allocation group: OBP only. (mapping functions are platform
 * dependent because they use physical address arguments.)
 */
extern	caddr_t		prom_map(caddr_t virthint, u_int space,
			    u_int phys, u_int size);

/*
 * I/O Group:
 */

extern	int		prom_input_source(void);
extern	int		prom_output_sink(void);

/*
 * Administrative group: SMCC platform specific.
 *
 * This assumes SMCC idprom hardware.
 */

extern	int		prom_getidprom(caddr_t addr, int size);
extern	int		prom_getmacaddr(ihandle_t hd, caddr_t ea);

/*
 * MMU management: sunmmu
 */

extern	void		prom_setcxsegmap(int c, caddr_t v, int seg);

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_PROM_PLAT_H */
