/*	Copyright (c) 1993 SMI	*/
/*	 All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident   "@(#)machlibthread.h 1.7     96/03/11     SMI"


typedef struct {
	long	rs_bp;
	long	rs_edi;
	long	rs_esi;
	long	rs_ebx;
	long	rs_sp;
	long	rs_pc;
	fpregset_t rs_fpu;
} resumestate_t;

#define	t_bp 	t_resumestate.rs_bp
#define	t_edi 	t_resumestate.rs_edi
#define	t_esi 	t_resumestate.rs_esi
#define	t_ebx 	t_resumestate.rs_ebx
#define	t_sp	t_resumestate.rs_sp
#define	t_pc	t_resumestate.rs_pc
