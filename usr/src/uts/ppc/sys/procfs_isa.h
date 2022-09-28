/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_PROCFS_ISA_H
#define	_SYS_PROCFS_ISA_H

#pragma ident	"@(#)procfs_isa.h	1.2	96/06/23 SMI"

/*
 * Instruction Set Architecture specific component of <sys/procfs.h>
 * PowerPC version
 */

#include <sys/regset.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Holds one PowerPC instruction
 */
typedef	uint32_t	instr_t;

#define	prgreg_t	greg_t
#define	prgregset_t	gregset_t
#define	prfpregset	fpu
#define	prfpregset_t	fpregset_t

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PROCFS_ISA_H */
