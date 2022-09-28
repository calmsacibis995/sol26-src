/*	Copyright (c) 1984, 1986, 1987, 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1992-1995 Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef _SYS_DKTP_FDISK_H
#define	_SYS_DKTP_FDISK_H

#pragma ident	"@(#)fdisk.h	1.6	95/10/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * fdisk.h
 * This file defines the structure of physical disk sector 0 for use on
 * AT386 systems.  The format of this sector is constrained by the ROM
 * BIOS and MS-DOS conventions.
 * Note that this block does not define the partitions used by the unix
 * driver.  The unix partitions are obtained from the VTOC.
 */

#define	BOOTSZ		446	/* size of boot code in master boot block */
#define	FD_NUMPART	4	/* number of 'partitions' in fdisk table */
#define	MBB_MAGIC	0xAA55	/* magic number for mboot.signature */
#define	DEFAULT_INTLV	4	/* default interleave for testing tracks */
#define	MINPSIZE	4	/* minimum number of cylinders in a partition */
#define	TSTPAT		0xE5	/* test pattern for verifying disk */

/*
 * structure to hold the fdisk partition table
 */
struct ipart {
	unsigned char bootid;	/* bootable or not */
	unsigned char beghead;	/* beginning head, sector, cylinder */
	unsigned char begsect;	/* begcyl is a 10-bit number. High 2 bits */
	unsigned char begcyl;	/*	are in begsect. */
	unsigned char systid;	/* OS type */
	unsigned char endhead;	/* ending head, sector, cylinder */
	unsigned char endsect;	/* endcyl is a 10-bit number.  High 2 bits */
	unsigned char endcyl;	/*	are in endsect. */
	long	relsect;	/* first sector relative to start of disk */
	long	numsect;	/* number of sectors in partition */
};
/*
 * Values for bootid.
 */
#define	NOTACTIVE	0
#define	ACTIVE		128
/*
 * Values for systid.
 */
#define	DOSOS12		1	/* DOS partition, 12-bit FAT */
#define	PCIXOS		2	/* PC/IX partition */
#define	DOSDATA		86	/* DOS data partition */
#define	DOSOS16		4	/* DOS partition, 16-bit FAT */
#define	EXTDOS		5	/* EXT-DOS partition */
#define	DOSHUGE		6	/* Huge DOS partition  > 32MB */
#define	OTHEROS		98	/* part. type for appl. (DB?) needs */
				/* raw partition.  ID was 0 but conflicted */
				/* with DOS 3.3 fdisk    */
#define	UNIXOS		99	/* UNIX V.x partition */
#define	UNUSED		100	/* unassigned partition */
#define	PPCBOOT		0x41	/* PowerPC boot partition (OS independent) */
#define	SUNIXOS		130	/* Solaris UNIX partition */
#define	X86BOOT		190	/* x86 Solaris boot partition */
#define	MAXDOS		65535L	/* max size (sectors) for DOS partition */

/*
 * structure to hold master boot block in physical sector 0 of the disk.
 * Note that partitions stuff can't be directly included in the structure
 * because of lameo '386 compiler alignment design.
 */

struct mboot {	/* master boot block */
	char    bootinst[BOOTSZ];
	char    parts[FD_NUMPART * sizeof (struct ipart)];
	ushort   signature;
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_FDISK_H */
