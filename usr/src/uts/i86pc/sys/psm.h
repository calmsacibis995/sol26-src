/*
 * Copyright (c) 1993-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PSM_H
#define	_SYS_PSM_H

#pragma ident	"@(#)psm.h	1.4	96/05/16 SMI"

/*
 * Platform Specific Module (PSM)
 */

/*
 * Include the loadable module wrapper.
 */
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/sunddi.h>
#include <sys/kmem.h>
#include <sys/psm_defs.h>
#include <sys/psm_types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * PSM External Interfaces
 */
extern int psm_mod_init(void **, struct psm_info *);
extern int psm_mod_fini(void **, struct psm_info *);
extern int psm_mod_info(void **, struct psm_info *, struct modinfo *);

extern int psm_add_intr(int, avfunc, char *, int, caddr_t);
extern int psm_add_nmintr(int, avfunc, char *, caddr_t);
extern processorid_t psm_get_cpu_id(void);

/* map physical address							*/
extern caddr_t psm_map(paddr_t, ulong, ulong);
/* unmap the physical address return from psm_map_phys()		*/
extern void psm_unmap(caddr_t, ulong, ulong);

#define	PSM_PROT_READ		0x0000
#define	PSM_PROT_WRITE		0x0001
#define	PSM_PROT_SAMEADDR 	0x0002
#define	PSM_PROT_CACHE		0x0004

/* handle memory error */
extern void psm_handle_memerror(paddr_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PSM_H */
