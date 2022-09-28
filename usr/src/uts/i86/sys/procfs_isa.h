/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_PROCFS_ISA_H
#define	_SYS_PROCFS_ISA_H

#pragma ident	"@(#)procfs_isa.h	1.1	96/06/18 SMI"

/*
 * Instruction Set Architecture specific component of <sys/procfs.h>
 * i386 version
 */

#include <sys/regset.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Holds one 386/486 instruction opcode
 */
typedef	u_char	instr_t;

#define	prgreg_t	greg_t
#define	prgregset_t	gregset_t
#define	prfpregset	fpu
#define	prfpregset_t	fpregset_t

/*
 * The following defines are for portability (see <sys/reg.h>).
 */
#define	R_PC	EIP
#define	R_PS	EFL
#define	R_SP	UESP
#define	R_FP	EBP
#define	R_R0	EAX
#define	R_R1	EDX

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PROCFS_ISA_H */
