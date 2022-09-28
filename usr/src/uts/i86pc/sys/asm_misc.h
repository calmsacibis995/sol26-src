/*
 * Copyright (c) 1992-1993,  by Sun Microsystems, Inc.
 */

#ifndef _SYS_ASM_MISC_H
#define	_SYS_ASM_MISC_H

#pragma ident	"@(#)asm_misc.h	1.10	94/09/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _ASM	/* The remainder of this file is only for assembly files */

/* Load reg with pointer to per-CPU structure */
#define	LOADCPU(reg)			\
	movl	%fs:0, reg;

/* Load reg with pointer to cpu private data */
#define	LOAD_CPU_PRI_DATA(reg)		\
	movl	%gs:CPU_M+CPU_PRI_DATA, reg;

/* reg = cpu[cpuno]->cpu_m.cpu_pri; */
#define	GETIPL(reg)	\
	movl	%gs:CPU_M+CPU_PRI, reg;

/* cpu[cpuno]->cpu_m.cpu_pri; */
#define	SETIPL(val)	\
	movl	val, %gs:CPU_M+CPU_PRI;

#endif /* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ASM_MISC_H */
