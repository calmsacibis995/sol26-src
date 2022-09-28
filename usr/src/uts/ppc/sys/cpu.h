/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#ifndef _SYS_CPU_H
#define	_SYS_CPU_H

#pragma ident	"@(#)cpu.h	1.2	94/02/10 SMI"

/*
 * This file contains common identification and reference information
 * for all PowerPC-based kernels.
 */

/*
 * Include generic bustype cookies.
 */
#include <sys/bustypes.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	CPU_ARCH	0xf0		/* mask for architecture bits */
#define	CPU_MACH	0x0f		/* mask for machine implementation */

#define	CPU_ANY		(CPU_ARCH|CPU_MACH)
#define	CPU_NONE	0

/*
 * PowerPC chip architectures
 */

#define	PPC_601_ARCH	0x10		/* arch value for 601 */
#define	PPC_603_ARCH	0x20		/* arch value for 603 */
#define	PPC_604_ARCH	0x30		/* arch value for 604 */
#define	PPC_620_ARCH	0x40		/* arch value for 620 */

/*
 * PowerPC platforms
 */

#define	PREP		0x01

#define	PREP_601	(PPC_601_ARCH + PREP)

#define	CPU_601		(PPC_601_ARCH + CPU_MACH)
#define	CPU_603		(PPC_603_ARCH + CPU_MACH)
#define	CPU_604		(PPC_604_ARCH + CPU_MACH)
#define	CPU_620		(PPC_620_ARCH + CPU_MACH)


/*
 * Global kernel variables of interest
 */

#if defined(_KERNEL) && !defined(_ASM)
extern short cputype;			/* machine type we are running on */

#endif /* defined(_KERNEL) && !defined(_ASM) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CPU_H */
