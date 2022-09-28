/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_PROCFS_ISA_H
#define	_SYS_PROCFS_ISA_H

#pragma ident	"@(#)procfs_isa.h	1.1	96/06/18 SMI"

/*
 * Instruction Set Architecture specific component of <sys/procfs.h>
 * sparc v8 version
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Holds one sparc instruction
 */
typedef	uint32_t	instr_t;

/*
 * General register access (sparc).
 * Don't confuse definitions here with definitions in <sys/regset.h>.
 */
#define	NPRGREG	38
typedef	int		prgreg_t;
typedef	prgreg_t	prgregset_t[NPRGREG];

#define	R_G0	0
#define	R_G1	1
#define	R_G2	2
#define	R_G3	3
#define	R_G4	4
#define	R_G5	5
#define	R_G6	6
#define	R_G7	7
#define	R_O0	8
#define	R_O1	9
#define	R_O2	10
#define	R_O3	11
#define	R_O4	12
#define	R_O5	13
#define	R_O6	14
#define	R_O7	15
#define	R_L0	16
#define	R_L1	17
#define	R_L2	18
#define	R_L3	19
#define	R_L4	20
#define	R_L5	21
#define	R_L6	22
#define	R_L7	23
#define	R_I0	24
#define	R_I1	25
#define	R_I2	26
#define	R_I3	27
#define	R_I4	28
#define	R_I5	29
#define	R_I6	30
#define	R_I7	31
#define	R_PSR	32
#define	R_PC	33
#define	R_nPC	34
#define	R_Y	35
#define	R_WIM	36
#define	R_TBR	37

/*
 * The following defines are for portability.
 */
#define	R_PS	R_PSR
#define	R_SP	R_O6
#define	R_FP	R_I6
#define	R_R0	R_O0
#define	R_R1	R_O1

/*
 * Floating-point register access (sparc FPU).
 * See <sys/regset.h> for details of interpretation.
 */
typedef struct prfpregset {
	union {				/* FPU floating point regs */
		uint32_t pr_regs[32];		/* 32 singles */
		double	pr_dregs[16];		/* 16 doubles */
	} pr_fr;
	void *	pr_filler;
	uint32_t pr_fsr;			/* FPU status register */
	u_char	pr_qcnt;		/* # of entries in saved FQ */
	u_char	pr_q_entrysize;		/* # of bytes per FQ entry */
	u_char	pr_en;			/* flag signifying fpu in use */
	uint32_t pr_q[64];		/* contains the FQ array */
} prfpregset_t;

/*
 * Extra register access
 */

#define	XR_G0		0
#define	XR_G1		1
#define	XR_G2		2
#define	XR_G3		3
#define	XR_G4		4
#define	XR_G5		5
#define	XR_G6		6
#define	XR_G7		7
#define	NPRXGREG	8

#define	XR_O0		0
#define	XR_O1		1
#define	XR_O2		2
#define	XR_O3		3
#define	XR_O4		4
#define	XR_O5		5
#define	XR_O6		6
#define	XR_O7		7
#define	NPRXOREG	8

#define	NPRXFILLER	8

#define	XR_TYPE_V8P	1		/* interpret union as pr_v8p */

typedef struct prxregset {
	uint32_t	pr_type;		/* how to interpret union */
	uint32_t	pr_align;		/* alignment for the union */
	union {
	    struct pr_v8p {
		union {				/* extra FP registers */
			uint32_t	pr_regs[32];
			double		pr_dregs[16];
			long double	pr_qregs[8];
		} pr_xfr;
		uint32_t	pr_xfsr;	/* upper 32bits, FP state reg */
		uint32_t	pr_fprs;	/* FP registers state */
		uint32_t	pr_xg[NPRXGREG]; /* upper 32bits, G registers */
		uint32_t	pr_xo[NPRXOREG]; /* upper 32bits, O registers */
		longlong_t	pr_tstate;	/* TSTATE register */
		uint32_t	pr_filler[NPRXFILLER];
	    } pr_v8p;
	} pr_un;
} prxregset_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PROCFS_ISA_H */
