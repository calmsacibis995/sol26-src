/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PROM_PLAT_H
#define	_SYS_PROM_PLAT_H

#pragma ident	"@(#)prom_plat.h	1.11	96/07/10 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file contains external platform-specific promif interface definitions
 * for the 32 bit PowerPC platform with IEEE 1275-1994 compliant prom.
 */

/*
 * "reg"-format for 32 bit cell-size, 1-cell physical addresses,
 * with a single 'size' cell:
 */

struct prom_reg {
	unsigned int addr, size;
};


extern	caddr_t		prom_malloc(caddr_t virt, u_int size, u_int align);

extern	caddr_t		prom_allocate_virt(u_int align, u_int size);
extern	caddr_t		prom_claim_virt(u_int size, caddr_t virt);
extern	void		prom_free_virt(u_int size, caddr_t virt);

extern	int		prom_map_phys(int mode, u_int size, caddr_t virt,
				u_int pa_hi, u_int pa_lo);
extern	int		prom_allocate_phys(u_int size, u_int align,
				u_int *addr);
extern	int		prom_claim_phys(u_int size, u_int addr);
extern	void		prom_free_phys(u_int size, u_int addr);

extern	int		prom_getmacaddr(ihandle_t hd, caddr_t ea);

/*
 * prom_translate_virt returns the physical address and virtualized "mode"
 * for the given virtual address. After the call, if *valid is non-zero,
 * a mapping to 'virt' exists and the physical address and virtualized
 * "mode" were returned to the caller.
 */
extern	int		prom_translate_virt(caddr_t virt, int *valid,
				u_int *physaddr, int *mode);
#ifdef	__cplusplus
}
#endif

#endif /* _SYS_PROM_PLAT_H */
