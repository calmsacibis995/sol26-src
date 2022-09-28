/*
 * Copyright (c) 1986-1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident "@(#)allregs.h 1.4	96/02/13 SMI"

/*
 * adb keeps its own idea of the current value of most of the
 * processor registers, in an "adb_regs" structure.
 */

#ifndef _ALLREGS_H
#define	_ALLREGS_H

#ifndef rw_fp
#include <sys/reg.h>
#endif /* !rw_fp */

#define	R_F0	40
#define	R_F1	41
#define	R_F2	42
#define	R_F3	43
#define	R_F4	44
#define	R_F5	45
#define	R_F6	46
#define	R_F7	47
#define	R_F8	48
#define	R_F9	49
#define	R_F10	50
#define	R_F11	51
#define	R_F12	52
#define	R_F13	53
#define	R_F14	54
#define	R_F15	55
#define	R_F16	56
#define	R_F17	57
#define	R_F18	58
#define	R_F19	59
#define	R_F20	60
#define	R_F21	61
#define	R_F22	62
#define	R_F23	63
#define	R_F24	64
#define	R_F25	65
#define	R_F26	66
#define	R_F27	67
#define	R_F28	68
#define	R_F29	69
#define	R_F30	70
#define	R_F31	71

#ifndef _ASM
#include <sys/pcb.h>
typedef struct allregs {
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
	int	r_fpscr;	/* 39th reg */
	double r_f0;		/* 40th reg */
	double r_f1;
	double r_f2;
	double r_f3;
	double r_f4;
	double r_f5;
	double r_f6;
	double r_f7;
	double r_f8;
	double r_f9;
	double r_f10;
	double r_f11;
	double r_f12;
	double r_f13;
	double r_f14;
	double r_f15;
	double r_f16;
	double r_f17;
	double r_f18;
	double r_f19;
	double r_f20;
	double r_f21;
	double r_f22;
	double r_f23;
	double r_f24;
	double r_f25;
	double r_f26;
	double r_f27;
	double r_f28;
	double r_f29;
	double r_f30;
	double r_f31;
	} allregs;
#endif /* !_ASM */

#endif /* !_ALLREGS_H */
