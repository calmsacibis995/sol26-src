/*
 * Copyright (c) 1994-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_REG_H
#define	_SYS_REG_H

#pragma ident	"@(#)reg.h	1.16	96/10/02 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


#if !defined(_XPG4_2) || defined(__EXTENSIONS__)

/*
 * Location of the users' stored registers.
 * Usage is u.u_ar0[XX].
 */
#define	R_R0	0
#define	R_R1	1
#define	R_R2	2
#define	R_R3	3
#define	R_R4	4
#define	R_R5	5
#define	R_R6	6
#define	R_R7	7
#define	R_R8	8
#define	R_R9	9
#define	R_R10	10
#define	R_R11	11
#define	R_R12	12
#define	R_R13	13
#define	R_R14	14
#define	R_R15	15
#define	R_R16	16
#define	R_R17	17
#define	R_R18	18
#define	R_R19	19
#define	R_R20	20
#define	R_R21	21
#define	R_R22	22
#define	R_R23	23
#define	R_R24	24
#define	R_R25	25
#define	R_R26	26
#define	R_R27	27
#define	R_R28	28
#define	R_R29	29
#define	R_R30	30
#define	R_R31	31
#define	R_CR	32
#define	R_LR	33
#define	R_PC	34
#define	R_MSR	35
#define	R_CTR	36
#define	R_XER	37
#define	R_MQ	38
/*
 * NOTE: The MQ register is available only on the MPU601, and contains the
 * product/dividend for multiply/divide operations.  It is included at the
 * end of the register save area, so that non-ABI compliant software that
 * utilizes this 601-specific register will not be clobbered during context
 * switches.
 */

/*
 * Distance from beginning of thread stack (t_stk) to saved regs struct.
 */
#define	REGOFF	MINFRAME

#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

/*
 * A gregset_t is defined as an array type for compatibility with the reference
 * source. This is important due to differences in the way the C language
 * treats arrays and structures as parameters.
 *
 * Note that NGREG is really (sizeof (struct regs) / sizeof (greg_t)),
 * but that the ABI defines it absolutely to be 39.
 */
#define	_NGREG	39

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	NGREG	_NGREG
#endif

#if !defined(_ASM)

typedef int	greg_t;
typedef greg_t	gregset_t[_NGREG];

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)

/*
**  This is a template used by trap() and syscall() to find saved copies
**  of the registers on the stack.
*/

struct regs {
	greg_t	r_r0;		/* GPRs 0 - 31 */
	greg_t	r_r1;
	greg_t	r_r2;
	greg_t	r_r3;
	greg_t	r_r4;
	greg_t	r_r5;
	greg_t	r_r6;
	greg_t	r_r7;
	greg_t	r_r8;
	greg_t	r_r9;
	greg_t	r_r10;
	greg_t	r_r11;
	greg_t	r_r12;
	greg_t	r_r13;
	greg_t	r_r14;
	greg_t	r_r15;
	greg_t	r_r16;
	greg_t	r_r17;
	greg_t	r_r18;
	greg_t	r_r19;
	greg_t	r_r20;
	greg_t	r_r21;
	greg_t	r_r22;
	greg_t	r_r23;
	greg_t	r_r24;
	greg_t	r_r25;
	greg_t	r_r26;
	greg_t	r_r27;
	greg_t	r_r28;
	greg_t	r_r29;
	greg_t	r_r30;
	greg_t	r_r31;
	greg_t	r_cr;		/* Condition Register */
	greg_t	r_lr;		/* Link Register */
	greg_t	r_pc;		/* User PC (Copy of SRR0) */
	greg_t	r_msr;		/* saved MSR (Copy of SRR1) */
	greg_t	r_ctr;		/* Count Register */
	greg_t	r_xer;		/* Integer Exception Register */
	greg_t	r_mq;		/* MQ Register (601 only) */
};

#define	r_sp	r_r1		/* user stack pointer */
#define	r_toc	r_r2		/* user TOC */
#define	r_ps	r_msr
#define	r_srr0	r_pc
#define	r_srr1	r_msr
#define	r_ret0	r_r3		/* return value register 0 */
#define	r_ret1	r_r4		/* return value register 1 */

#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#endif /* !defined(_ASM) */

#ifdef	_KERNEL
#define	lwptoregs(lwp)	((struct regs *)((lwp)->lwp_regs))
#define	lwptofpu(lwp)	((struct fpu *)((lwp)->lwp_fpu))
#endif	/* _KERNEL */

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)

/* Bit definitions in Condition Register for CR0 */
#define	CR0_LT	0x80000000	/* Negative bit in CR0 */
#define	CR0_GT	0x40000000	/* Positive bit in CR0 */
#define	CR0_EQ	0x20000000	/* Zero bit in CR0 */
#define	CR0_SO	0x10000000	/* Summary Overflow bit in CR0 */

/* Bit definitions in XER (Integer Exception Register) */
#define	XER_SO	0x80000000	/* Summary Overflow bit */
#define	XER_OV	0x40000000	/* Overflow bit */
#define	XER_CA	0x20000000	/* Carry bit */

#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#if !defined(_ASM)

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)

/*
 * Floating point definitions.
 */
typedef struct fpu {
	double		fpu_regs[32];	/* FPU regs - 32 doubles */
	unsigned	fpu_fpscr;	/* FPU status/control reg */
	unsigned 	fpu_valid;	/* nonzero IFF the rest of this */
					/* structure contains valid data */
} fpregset_t;

/*
 * Structure mcontext defines the complete hardware machine state.
 */
typedef struct {
	gregset_t	gregs;		/* general register set */
	fpregset_t	fpregs;		/* floating point register set */
	long		filler[8];
} mcontext_t;

#else /* XPG4v2 requires mcontext_t  -- XPG4v2 allowed identifiers */

/*
 * Floating point definitions.
 */
typedef struct __fpu {
	double		__fpu_regs[32];	/* FPU regs - 32 doubles */
	unsigned	__fpu_fpscr;	/* FPU status/control reg */
	unsigned 	__fpu_valid;	/* nonzero IFF the rest of this */
					/* structure contains valid data */
} fpregset_t;

/*
 * Structure mcontext defines the complete hardware machine state.
 */
typedef struct {
	gregset_t	__gregs;	/* general register set */
	fpregset_t	__fpregs;	/* floating point register set */
	long		__filler[8];
} mcontext_t;

#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#endif	/* !defined(_ASM) */


/*
 * PowerPC floating point processor definitions
 */

/*
 * values that go into fp_version
 * NOTE: on the 60x series, this should always be FP_HW, because the
 * CPU contains an integrated Integer Unit, Branch Processor, and
 * Floating Point Unit.
 */
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	FP_NO   0	/* no fp chip, no emulator (no fp support) */
#define	FP_HW   1	/* fp processor present bit */

/*
 * masks for PowerPC FPSCR (Floating Point Status and Control Register)
 */
/*
 * Floating point exception summary bits
 */
#define	FP_EXCEPTN_SUMM	   0x80000000	/* FX - exception summary	*/
#define	FP_ENAB_EX_SUMM	   0x40000000	/* FEX-enabled exception summary */
#define	FP_INV_OP_EX_SUMM  0x20000000	/* VX-invalid op except'n summary */

/*
 * Floating point exception bits
 */
#define	FP_OVERFLOW	   0x10000000	/* OX - overflow exception bit	*/
#define	FP_UNDERFLOW	   0x08000000	/* UX - underflow exception bit */
#define	FP_ZERODIVIDE	   0x04000000	/* ZX - zerodivide except'n bit */
#define	FP_INEXACT	   0x02000000	/* XX - inexact exception bit	*/

/*
 * Floating point invalid operation exception bits
 */
#define	FP_INV_OP_SNaN	   0x01000000	/* VXSNaN - signalled NaN	*/
#define	FP_INV_OP_ISI	   0x00800000	/* VXISI - infinity-infinity	*/
#define	FP_INV_OP_IDI	   0x00400000	/* VXIDI - infinity/infinity	*/
#define	FP_INV_OP_ZDZ	   0x00200000	/* VXZDZ - 0/0			*/
#define	FP_INV_OP_IMZ	   0x00100000	/* VXIMZ - infinity*0		*/
#define	FP_INV_OP_VC	   0x00080000	/* VXVC - invalid compare	*/
#define	FP_INV_OP_SOFT	   0x00000400	/* VXSOFT - software request	*/
#define	FP_INV_OP_SQRT	   0x00000200	/* VXSQRT - invalid square root	*/
#define	FP_INV_OP_CVI	   0x00000100	/* VXCVI - invalid integer convert */

/*
 * Floating point status bits
 */
#define	FP_FRAC_ROUND	   0x00040000	/* FR - fraction rounded	*/
#define	FP_FRAC_INEXACT	   0X00020000	/* FI - fraction inexact	*/

/*
 * Floating point result flags (FPRF group)
 */
#define	FP_RESULT_CLASS	   0x00010000	/* C-result class descriptor	*/
/*
 * Floating point condition codes (FPRF:FPCC)
 */
#define	FP_LT_OR_NEG	   0x00008000	/* - normalized number		*/
#define	FP_GT_OR_POS	   0X00004000	/* + normalized number		*/
#define	FP_EQ_OR_ZERO	   0X00002200	/* + zero			*/
#define	FP_UN_OR_NaN	   0X00001000	/* unordered or Not a Number	*/

/*
 * Floating point exception enable bits
 */
#define	FP_INV_OP_ENAB	   0x00000080	/* VE-invalid operation enable	*/
#define	FP_OVRFLW_ENAB	   0x00000040	/* OE-overflow exception enable	*/
#define	FP_UNDRFLW_ENAB	   0x00000020	/* UE-underflow exception enable */
#define	FP_ZERODIV_ENAB	   0x00000010	/* ZE-zero divide exception enable */
#define	FP_INEXACT_ENAB	   0x00000008	/* XE-inexact exception enable	*/

/*
 * NOTE: This is an implementation-dependent bit, and is not used
 * on the MPU601.  If set, the results of floating point operations
 * may not conform to the IEEE standard; i.e., approximate results.
 */
#define	FP_NON_IEEE_MODE   0x00000004	/* NI - non-IEEE mode bit	*/

/*
 * Floating point rounding control bits (RN)
 */
#define	FP_ROUND_CTL1	   0x00000002
#define	FP_ROUND_CTL2	   0x00000001

/*
 * FPSCR Rounding Control Options
 *	00 Round to nearest
 *	01 Round toward zero
 *	10 Round toward +infinity
 *	11 Round toward -infinity
 */
#define	FP_ROUND_NEAREST   0x00000000	/* 00 - round to nearest	*/
#define	FP_ROUND_ZERO	   0x00000001	/* 01 - round toward zero	*/
#define	FP_ROUND_PLUS_INF  0x00000002	/* 10 - round toward +infinity	*/
#define	FP_ROUND_MINUS_INF 0x00000003	/* 11 - round toward -infinity	*/

/* Initial value of FPSCR as per ABI document */
#define	FPSCR_HW_INIT 0

#ifndef _ASM
extern int fp_version;			/* kind of fp support		*/
extern int fpu_exists;			/* FPU hw exists		*/
#endif

#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_REG_H */
