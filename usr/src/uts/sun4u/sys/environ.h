/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_ENVIRON_H
#define	_SYS_ENVIRON_H

#pragma ident	"@(#)environ.h	1.17	96/01/16 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* useful debugging stuff */
#define	ENVIRON_ATTACH_DEBUG	0x1
#define	ENVIRON_INTERRUPT_DEBUG	0x2
#define	ENVIRON_REGISTERS_DEBUG	0x4

/*
 * OBP supplies us with 1 register set for the environment node
 *
 * It is:
 * 	0	Temperature register
 */

#if defined(_KERNEL)

/* Structures used in the driver to manage the hardware */
struct environ_soft_state {
	dev_info_t *dip;		/* dev info of myself */
	dev_info_t *pdip;		/* dev info of parent */
	struct environ_soft_state *next;
	int board;			/* Board number for this FHC */
	volatile u_char *temp_reg;	/* VA of temperature register */
	struct temp_stats tempstat;	/* in memory storage of temperature */
};

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ENVIRON_H */
