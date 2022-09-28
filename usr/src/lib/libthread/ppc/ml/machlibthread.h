/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.  All Rights Reserved.
 */

#pragma ident	"@(#)machlibthread.h	1.16	95/02/01 SMI"

typedef struct {
	long	rs_sp;		/* aka r1 */
	long	rs_pc;
	long	rs_r13;
	long	rs_r14;
	long	rs_r15;
	long	rs_r16;
	long	rs_r17;
	long	rs_r18;
	long	rs_r19;
	long	rs_r20;
	long	rs_r21;
	long	rs_r22;
	long	rs_r23;
	long	rs_r24;
	long	rs_r25;
	long	rs_r26;
	long	rs_r27;
	long	rs_r28;
	long	rs_r29;
	long	rs_r30;
	long	rs_r31;
	long	rs_cr;
	/* non volatile (reserved) FP regs f14-f31 */
	long	rs_fpscr;
	long	rs_fpvalid;	/* non zero IFF rest of the state is valid */
	double	rs_f14;
	double	rs_f15;
	double	rs_f16;
	double	rs_f17;
	double	rs_f18;
	double	rs_f19;
	double	rs_f20;
	double	rs_f21;
	double	rs_f22;
	double	rs_f23;
	double	rs_f24;
	double	rs_f25;
	double	rs_f26;
	double	rs_f27;
	double	rs_f28;
	double	rs_f29;
	double	rs_f30;
	double	rs_f31;
} resumestate_t;


#define	t_sp	t_resumestate.rs_sp
#define	t_pc	t_resumestate.rs_pc
#define	t_r13	t_resumestate.rs_r13
#define	t_r14	t_resumestate.rs_r14
#define	t_r15	t_resumestate.rs_r15
#define	t_r16	t_resumestate.rs_r16
#define	t_r17	t_resumestate.rs_r17
#define	t_r18	t_resumestate.rs_r18
#define	t_r19	t_resumestate.rs_r19
#define	t_r20	t_resumestate.rs_r20
#define	t_r21	t_resumestate.rs_r21
#define	t_r22	t_resumestate.rs_r22
#define	t_r23	t_resumestate.rs_r23
#define	t_r24	t_resumestate.rs_r24
#define	t_r25	t_resumestate.rs_r25
#define	t_r26	t_resumestate.rs_r26
#define	t_r27	t_resumestate.rs_r27
#define	t_r28	t_resumestate.rs_r28
#define	t_r29	t_resumestate.rs_r29
#define	t_r30	t_resumestate.rs_r30
#define	t_r31	t_resumestate.rs_r31
#define	t_cr	t_resumestate.rs_cr

#define	t_f14	t_resumestate.rs_f14
#define	t_f15	t_resumestate.rs_f15
#define	t_f16	t_resumestate.rs_f16
#define	t_f17	t_resumestate.rs_f17
#define	t_f18	t_resumestate.rs_f18
#define	t_f19	t_resumestate.rs_f19
#define	t_f20	t_resumestate.rs_f20
#define	t_f21	t_resumestate.rs_f21
#define	t_f22	t_resumestate.rs_f22
#define	t_f23	t_resumestate.rs_f23
#define	t_f24	t_resumestate.rs_f24
#define	t_f25	t_resumestate.rs_f25
#define	t_f26	t_resumestate.rs_f26
#define	t_f27	t_resumestate.rs_f27
#define	t_f28	t_resumestate.rs_f28
#define	t_f29	t_resumestate.rs_f29
#define	t_f30	t_resumestate.rs_f30
#define	t_f31	t_resumestate.rs_f31
#define	t_fpscr	t_resumestate.rs_fpscr
#define	t_fpvalid t_resumestate.rs_fpvalid
