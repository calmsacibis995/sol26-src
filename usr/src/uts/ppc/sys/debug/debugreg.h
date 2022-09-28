/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#ifndef _SYS_DEBUG_DEBUGREG_H
#define	_SYS_DEBUG_DEBUGREG_H

#pragma ident	"@(#)debugreg.h	1.2	94/02/10 SMI"

/*
 * This file is a place holder for future implementation of hw debugregs
 * support.
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Maximum number of debugregs in any implementation of PowerPC.
 *	CPU_601		3
 *	CPU_604		?
 */
#define	NDEBUGREG	3

typedef struct dbregset {
	unsigned int	debugreg[NDEBUGREG];
} dbregset_t;

#define	hid_cpu601_1	debugreg[0]
#define	hid_cpu601_2	debugreg[1]
#define	hid_cpu601_5	debugreg[2]

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DEBUG_DEBUGREG_H */
