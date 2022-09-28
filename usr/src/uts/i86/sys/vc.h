/*
 * Copyright (c) 1992, by Sun Microsystems, Inc.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_VC_H
#define	_SYS_VC_H

#pragma ident	"@(#)vc.h	1.2	94/09/03 SMI"	/* from SVR4: vc.h	1.1 */

#include <sys/types.h>
#include <sys/thread.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * VP/ix process dispatcher parameter table entry
 */
typedef struct vcdpent {
	pri_t	vc_globpri;	/* global (class independent) priority */
	long	vc_quantum;	/* time quantum given to procs at this level */
	pri_t	vc_tqexp;	/* vc_umdpri assigned when proc at this level */
				/*   exceeds its time quantum */
	pri_t	vc_slpret;	/* vc_umdpri assigned when proc at this level */
				/*  returns to user mode after sleeping */
	short	vc_maxwait;	/* bumped to vc_lwait if more than vc_maxwait */
				/*  secs elapse before receiving full quantum */
	short	vc_lwait;	/* vc_umdpri assigned if vc_dispwait exceeds  */
				/*  vc_maxwait */
} vcdpent_t;


/*
 * VP/ix class specific proc structure
 */
typedef struct vcproc {
	long	vc_timeleft;	/* time remaining in procs quantum */
	pri_t	vc_cpupri;	/* system controlled component of vc_umdpri */
	pri_t	vc_uprilim;	/* user priority limit */
	pri_t	vc_upri;	/* user priority */
	pri_t	vc_umdpri;	/* user mode priority within vc class */
	char	vc_nice;	/* nice value for compatibility */
	unsigned char vc_flags;	/* flags defined below */
	short	vc_dispwait;	/* number of wall clock seconds since start */
				/*   of quantum (not reset upon preemption) */
	kthread_t *vc_tp;	/* pointer to thread */
	int	*vc_pstatp;	/* pointer to p_stat */
	pri_t	*vc_pprip;	/* pointer to p_pri */
	uint	*vc_pflagp;	/* pointer to p_flag */
	struct vcproc *vc_next;	/* link to next vcproc on list */
	struct vcproc *vc_prev;	/* link to previous vcproc on list */
} vcproc_t;


/* flags */
#define	VCKPRI	0x01		/* proc at kernel mode priority */
#define	VCBACKQ	0x02		/* proc goes to back of disp q when preempted */
#define	VCSLEPT	0x04		/* proc has forked, so don't reset full */
				/* quantum */
#define	VCBOOST	0x08		/* Process priority was boosted */
#define	VCBSYWT 0x10		/* Process is busywaiting */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_VC_H */
