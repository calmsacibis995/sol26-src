
/* Copyright (c) 1994 - 1996, Sun Microsystems, Inc.	*/
/* All rights reserved.					*/

#ifndef _POWERD_H
#define	_POWERD_H

#pragma ident	"@(#)powerd.h	1.6	96/04/12 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Data Structures for using IPC with powerd
 *
 * Shared memory
 */
struct pwr_info {
	int	pd_pid;			/* Process id of daemon */
	int	pd_flags;		/* Status of powerd */
	int	pd_idle_time;		/* in minutes */
	int	pd_start_time;		/* in minutes since midnight */
	int	pd_finish_time;		/* in minutes since midnight */

	int	pd_charge;		/* Percentage charge of battery */
	int	pd_time_left;		/* Estimated time on battery */
	int	pd_autoresume;		/* Autoresume active */
	int	pd_autoshutdown;	/* Autoshutdown active */

	int	pd_ttychars_idle;	/* Tty chars idle time */
	int	pd_ttychars_sum;	/* Sum of tty chars */
	int	pd_loadaverage_idle;	/* Load average idle time */
	float	pd_loadaverage;		/* Load average */
	int	pd_diskreads_idle;	/* Disk reads idle time */
	int	pd_diskreads_sum;	/* Sum of disk reads */
	int	pd_nfsreqs_idle;	/* NFS requests idle time */
	int	pd_nfsreqs_sum;		/* Sum of NFS requests */
};
typedef struct pwr_info pwr_info_t;

/* Various status flags for pd_flags */
#define	PD_BATTERY	0x01
#define	PD_AC		0x02
#define	PD_AUTORESUME	0x04
#define	PD_RESUME_ON	0x08

#ifdef	__cplusplus
}
#endif

#endif	/* _POWERD_H */
