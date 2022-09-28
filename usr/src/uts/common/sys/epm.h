/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_EPM_H
#define	_SYS_EPM_H

#pragma ident	"@(#)epm.h	1.5	96/05/24 SMI"	/* SVr4.0 */

#include <sys/sunddi.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

/*
 * epm.h:	Function prototypes and data structs for kernel pm functions.
 */

dev_info_t *
e_pm_busy(dev_info_t *dip, int why);

dev_info_t *
e_pm_busy_dev(dev_t dev, int why);

void
e_pm_unbusy(dev_info_t *dip, int why);

dev_info_t *
e_pm_unbusy_dev(dev_t dev, int why);

int
e_pm_props(dev_info_t *dip);

/*
 * "Why" values used by e_pm_props and friends, also found in devi_comp_flags
 */
#define	PMC_ALWAYS	1		/* component always busy */
#define	PMC_DRIVER	2		/* driver does bookkeeping */
#define	PMC_OPEN	4		/* component busy when device is open */
#define	PMC_READ	8		/* component busy when in read(9e) */
#define	PMC_WRITE	0x10		/* component busy when in write(9e) */
#define	PMC_IOCTL	0x20		/* component busy when in ioctl(9e) */
#define	PMC_PHYSIO	0x40		/* component busy when in strategy */
#define	PMC_APHYSIO	0x80		/* component busy when in strategy */
#define	PMC_MMAP	0x100		/* component busy when mmaped */
/*
 * Actual flag value used is 0, this is a token for getting there
 */
#define	PMC_NEVER	0x200		/* component never busy  */

#define	PMC_AUTO	(PMC_OPEN|PMC_READ|PMC_WRITE|PMC_IOCTL|PMC_PHYSIO|\
	PMC_APHYSIO|PMC_MMAP)

/*
 * Other flag values sharing the same flag int
 */
#define	PMC_NEEDS_SR	0x400		/* do suspend/resume despite no "reg" */
#define	PMC_NO_SR	0x800		/* don't suspend/resume despite "reg" */
#define	PMC_PARENTAL_SR	0x1000		/* call up tree to suspend/resume */
#define	PMC_TSPROP	0x2000		/* uses old pm_timestamp prop */
#define	PMC_NPPROP	0x4000		/* uses old pm_norm_pwr prop */

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_EPM_H */
