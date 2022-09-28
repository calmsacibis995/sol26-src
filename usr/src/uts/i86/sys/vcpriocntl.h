/*
 *	Copyright (c) 1992, Sun Microsystems, Inc.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_VCPRIOCNTL_H
#define	_SYS_VCPRIOCNTL_H

#pragma ident	"@(#)vcpriocntl.h	1.2	94/09/03 SMI"

#include <sys/types.h>
#include <sys/thread.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * VP/ix process class specific structures for the priocntl system call.
 */

typedef struct vcparms {
	pri_t	vc_uprilim;	/* user priority limit */
	pri_t	vc_upri;	/* user priority */
} vcparms_t;


typedef struct vcinfo {
	pri_t	vc_maxupri;	/* configured limits of user priority range */
} vcinfo_t;

#define	VC_NOCHANGE	-32768

/*
 * The following is used by the dispadmin(1M) command for
 * scheduler administration and is not for general use.
 */

typedef struct vcadmin {
	struct vcdpent	*vc_dpents;
	short		vc_ndpents;
	short		vc_cmd;
} vcadmin_t;

#define	VC_GETDPSIZE	1
#define	VC_GETDPTBL	2
#define	VC_SETDPTBL	3

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_VCPRIOCNTL_H */
