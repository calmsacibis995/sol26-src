/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986,1987,1988,1989,1990  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

#ifndef	_VM_SEG_DEV_H
#define	_VM_SEG_DEV_H

#pragma ident	"@(#)seg_dev.h	1.30	96/06/25 SMI"
/*	From:	SVr4.0	"kernel:vm/seg_dev.h	1.8"		*/

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Structure whose pointer is passed to the segdev_create routine
 */
struct segdev_crargs {
	offset_t	offset;		/* starting offset */
	int	(*mapfunc)(dev_t dev, off_t off, int prot); /* map function */
	dev_t	dev;		/* device number */
	u_char	type;		/* type of sharing done */
	u_char	prot;		/* protection */
	u_char	maxprot;	/* maximum protection */
	u_int	hat_flags;	/* hat flags */
	void    *devmap_data;   /* devmap_handle private data */
};

/*
 * (Semi) private data maintained by the seg_dev driver per segment mapping
 *
 * The segment lock is necessary to protect fields that are modified
 * when the "read" version of the address space lock is held.  This lock
 * is not needed when the segment operation has the "write" version of
 * the address space lock (it would be redundant).
 *
 * The following fields in segdev_data are read-only when the address
 * space is "read" locked, and don't require the segment lock:
 *
 *	vp
 *	offset
 *	mapfunc
 *	maxprot
 */
struct	segdev_data {
	offset_t	offset;		/* device offset for start of mapping */
	kmutex_t	lock;		/* protects segdev_data */
	int	(*mapfunc)(dev_t dev, off_t off, int prot);
	struct	vnode *vp;	/* vnode associated with device */
	u_char	pageprot;	/* true if per page protections present */
	u_char	prot;		/* current segment prot if pageprot == 0 */
	u_char	maxprot;	/* maximum segment protections */
	u_char	type;		/* type of sharing done */
	struct	vpage *vpage;	/* per-page information, if needed */
	u_int	hat_flags;	/* hat flags - pass to attr in hat_devload */
	u_int	hat_noconsist;	/* Set HAT_LOAD_NOCONSIST flag in hat_devload */
	int		softlockcnt;	/* # of SOFTLOCKED in seg */
	void    *devmap_data;   /* devmap_handle private data */
};

#ifdef _KERNEL

extern int segdev_create(struct seg *, void *);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_SEG_DEV_H */
