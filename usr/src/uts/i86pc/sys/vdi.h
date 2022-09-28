/*
 * Copyrighted as an unpublished work.
 * (c) Copyright 1988 Sun Microsystems, Inc.
 * All rights reserved.
 *
 * RESTRICTED RIGHTS
 *
 * These programs are supplied under a license.  They may be used,
 * disclosed, and/or copied only as permitted under such license
 * agreement.  Any copy must contain the above copyright notice and
 * this restricted rights notice.  Use, copying, and/or disclosure
 * of the programs is strictly prohibited unless otherwise provided
 * in the license agreement.
 */

#ifndef _SYS_VDI_H
#define	_SYS_VDI_H

#pragma ident	"@(#)vdi.h	1.2	94/09/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * definitions for PC AT VP/ix Device Interface driver
 */

#define	VDNINTR		16	/* sixteen interrupt lines on dual 8259's */
#define	VDMAXVDEVS	20	/* maximum number of generic devices */
				/* supported */
#define	VDMAX_IOLEN	40	/* maximum number of consecutive registers */
#define	VDUSELEN	50	/* maximum number of chunks of */
				/* registers/memory */
#define	VDCHECK		1	/* perform range checking */

/*
 * this structure defines the interface between the user process and
 * the generic device driver ioctl function calls.  The user process
 * can request chunks of io or memory with successive ioctl calls
 */
struct vdev {	/* generic device structure */
	ushort	vdi_iobase;	/* base io address */
	int	vdi_iolen;	/* length of io address */
	long	vdi_pmembase;	/* base physical memory address */
	long	vdi_vmembase;	/* base virtual memory address */
	int	vdi_memlen;	/* length of memory required */
	int	vdi_intnum;	/* interrupt vector required */
	int	vdi_pseudomask;	/* pseudorupt to send */
	int	vdi_dma_chan;	/* DMA channel requested */
	ushort	vdi_flag;	/* indicates edge or level triggered device */
};

struct vdi_used {
	long	base;
	int	len;
	char	dev;
};

/*
 * IOCTLs supported by the generic device driver
 */
#define	VDIOC		('v'<<8)
#define	VDI_SET		(VDIOC|1)	/* setup device parameters */
#define	VDI_UNSET	(VDIOC|2)	/* undo device parameters */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_VDI_H */
