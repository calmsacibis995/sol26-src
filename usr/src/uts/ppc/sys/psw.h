/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_PSW_H
#define	_SYS_PSW_H

#pragma ident	"@(#)psw.h	1.7	94/12/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definition of bits in the PowerPC MSR (Machine Status Register).
 *
 * NOTE: The MSR is a 32-bit register on the MPU601, but is a 64-bit
 * register on other PowerPC processors.
 */

/*
 * MSR bits defined in the PowerPC architecture, present on the 601.
 */
#define	MSR_EE	0x00008000	/* External Interrupt Enable */
#define	MSR_PR	0x00004000	/* Privilege level: 0-supervisor, 1-user */
#define	MSR_FP	0x00002000	/* Floating-point Available */
#define	MSR_ME	0x00001000	/* Machine Check Enable */
#define	MSR_FE0	0x00000800	/* Floating-point Exception mode 0 */
#define	MSR_SE	0x00000400	/* Single-step trace Enable */
#define	MSR_FE1	0x00000100	/* Floating-point Exception mode 1 */
#define	MSR_EP	0x00000040	/* Exception (vector) Prefix */
#define	MSR_IP	MSR_EP
#define	MSR_IR	0x00000020	/* Instruction address translation enable */
#define	MSR_DR	0x00000010	/* Data address translation enable */
/*
 * MSR bits defined in the PowerPC architecture, not implemented on the 601.
 */
#define	MSR_BE	0x00000200	/* Branch trace Enable */
#define	MSR_ILE	0x00010000	/* Interrupt Little-Endian mode */
#define	MSR_POW	0x00040000	/* Power management enable */
#define	MSR_RE	0x00000002	/* Recoverable Exception */
#define	MSR_RI	MSR_RE
#define	MSR_LE	0x00000001	/* Little-endian Enable */

/* MSR bits specific to MP603 */
#define	MSR_TGPR 0x00020000	/* Temporary GPR remapping (MP603 only) */

/* MSR bits specific to MP604 */
#define	MSR_PM	0x00000004	/* Performance Monitor marked mode */

#if !defined(_ASM)
typedef int	psw_t;
#endif	/* !defined(_ASM) */

/*
 * MSR bits that may be changed by user processes.
 */
#define	MSR_USER_BITS		(MSR_PR | MSR_FE0 | MSR_FE1)
#define	MSR_USER_BITS_MASK	(MSR_FP)

/*
 * PSL_USER - initial value of MSR for user thread. This is used in generic
 * code also.
 */
#if !defined(_ASM)
long get_msr(void);
#define	PSL_USER ((get_msr() & ~MSR_USER_BITS_MASK) | MSR_USER_BITS)
#endif	/* !defined(_ASM) */

/*
 * Macros to decode psr.
 *
 */
#define	USERMODE(ps)	(((ps) & MSR_PR) != 0)

#include <sys/spl.h>

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PSW_H */
