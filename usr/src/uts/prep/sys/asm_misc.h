/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#ifndef _SYS_ASM_MISC_H
#define	_SYS_ASM_MISC_H

#pragma ident	"@(#)asm_misc.h	1.2	94/02/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _ASM	/* The remainder of this file is only for assembly files */

/*
 * XXXPPC NEEDS REVIEW.
 */

/* Load reg with pointer to per-CPU structure */
#define	LOADCPU(reg)			\
	lwz	reg, T_CPU(THREAD_REG);

/* Load reg with pointer to cpu private data */
#define	LOAD_CPU_PRI_DATA(reg)		\
	lwz	reg, T_CPU(THREAD_REG); \
	lwz	reg, CPU_PRI_DATA(reg);

/* reg = cpu[cpuno]->cpu_m.cpu_pri_data->c_cmdport[port]; */
/* port had better be a register; tmp is scratch register */
#define	GETCMDPORT(reg, port, tmp)	\
	LOAD_CPU_PRI_DATA(reg); \
	slwi	tmp, port, 1; \
	la	reg, C_CMDPORT(reg); \
	lhzx	reg, tmp, reg;

/* reg = cpu[cpuno]->cpu_m.cpu_pri; */
#define	GETIPL(reg)	\
	lwz	reg, T_CPU(THREAD_REG); \
	lwz	reg, CPU_PRI(reg);

/* cpu[cpuno]->cpu_m.cpu_pri = val; */
#define	SETIPL(val, tmp)	\
	lwz	tmp, T_CPU(THREAD_REG); \
	stw	val, CPU_PRI(tmp);

/* reg = cpu[cpuno]->cpu_m.cpu_pri_data->c_picipl; */
#define	GETPICIPL(reg)	\
	LOAD_CPU_PRI_DATA(reg);		\
	lwz	reg, C_PICIPL(reg);

/* cpu[cpuno]->cpu_m.cpu_pri_data->c_picipl = val; */
/* can't pass %edx as val */
#define	SETPICIPL(val, tmp)	\
	LOAD_CPU_PRI_DATA(tmp);		\
	stw	val, C_PICIPL(tmp);

/* reg = cpu[cpuno]->cpu_m.cpu_pri_data->c_npic; */
#define	GETNPIC(reg)	\
	LOAD_CPU_PRI_DATA(reg);		\
	lwz	reg, C_NPIC(reg);

/* reg = cpu[cpuno]->cpu_m.cpu_pri_data->c_curmask[index]; */
/* index better be a register */
#define	GETCURMASK(reg, index)	\
	LOAD_CPU_PRI_DATA(reg);		\
	la	reg, C_CURMASK(reg); \
	lbzx	reg, index, reg;

/* reg = cpu[cpuno]->cpu_m.cpu_pri_data->c_iplmask[index]; */
/* index better be a register */
#define	GETIPLMASK(reg, index)	\
	LOAD_CPU_PRI_DATA(reg);		\
	la	reg, C_IPLMASK(reg); \
	lbzx	reg, index, reg;

/* cpu[cpuno]->cpu_m.cpu_pri_data->c_iplmask[index] = val; */
/* index better be a register */
#define	SETIPLMASK(val, index, tmp)	\
	LOAD_CPU_PRI_DATA(tmp);		\
	la	tmp, C_IPLMASK(tmp); \
	stbx	val, index, tmp;

/* reg = cpu[cpuno]->cpu_m.cpu_pri_data->c_imrport[index]; */
/* index better be a register */
#define	GETIMRPORT(reg, index, tmp)	\
	LOAD_CPU_PRI_DATA(reg);		\
	la	reg, C_IMRPORT(reg); \
	slwi	tmp, index, 1; \
	lhzx	reg, tmp, reg;

/*
 * return the Priority level for IRQ vect
 */
#define	GETPRILEV(reg, vect, tmp)		\
	lwz	reg, autovect@toc; \
	slwi	tmp, vect, 3; \
	la	reg, AVH_HI_PRI(reg); \
	lhzx	reg, tmp, reg; \
	andi.	reg, reg, 0xff;

#endif /* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ASM_MISC_H */
