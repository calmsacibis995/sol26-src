/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_MACHPARAM_H
#define	_SYS_MACHPARAM_H

#pragma ident	"@(#)machparam.h	1.21	96/10/17 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Machine dependent parameters and limits - PC version.
 */

/*
 * The following symbols are required for the Sun4c architecture.
 *	OPENPROMS	# Open Boot Prom interface
 *	MEMSEG		# uses 4.1 style memory segments
 *	NEWDUMP		# uses sparse crash dumping scheme
 *	SAIO_COMPAT	# OBP uses "bootparam" interface
 */
#define	NCPU 	21 	/* XXX no MPs in this architectural family */

/*
 * Define the FPU symbol if we could run on a machine with an external
 * FPU (i.e. not integrated with the normal machine state like the vax).
 *
 * The fpu is defined in the architecture manual, and the kernel hides
 * its absence if it is not present, that's pretty integrated, no?
 */

/*
 * MMU_PAGES* describes the physical page size used by the mapping hardware.
 * PAGES* describes the logical page size used by the system.
 */

#define	MMU_PAGESIZE	0x1000		/* 4096 bytes */
#define	MMU_PAGESHIFT	12		/* log2(MMU_PAGESIZE) */
#define	MMU_PAGEOFFSET	(MMU_PAGESIZE-1) /* Mask of address bits in page */
#define	MMU_PAGEMASK	(~MMU_PAGEOFFSET)

#define	PAGESIZE	0x1000		/* All of the above, for logical */
#define	PAGESHIFT	12
#define	PAGEOFFSET	(PAGESIZE - 1)
#define	PAGEMASK	(~PAGEOFFSET)

/*
 * DATA_ALIGN is used to define the alignment of the Unix data segment.
 */
#define	DATA_ALIGN	PAGESIZE

/*
 * DEFAULT KERNEL THREAD stack size.    XXX should this be 1???
 */
#define	DEFAULTSTKSZ	2*PAGESIZE

/*
 * KERNELSIZE is the amount of virtual address space the kernel
 * uses in all contexts.
 */
#define	KERNELSIZE	(512*1024*1024)

/*
 * KERNELBASE is the virtual address which
 * the kernel text/data mapping starts in all contexts.
 */
#define	KERNELBASE	(0-KERNELSIZE)

/*
 * This is the last 4MB of the 4G address space. Some psm modules
 * need this region of virtual address space mapped 1-1
 */
#define	PROMSTART	(0xffc00000)

/*
 * Define upper limit on user address space
 */
#define	USERLIMIT	KERNELBASE

/*
 * KERNELSHADOW_VADDR is the virtual address used to relocate kernel text+data
 * to a 4MB page
 */
#define	KERNELSHADOW_VADDR	((u_int)KERNELBASE - KERNELSIZE)

/*
 * SYSBASE is the virtual address which
 * the kernel allocated memory mapping starts in all contexts.
 * SYSLIMIT - SYSBASE is the maximum amount of memory that can be allocated
 * from kernelmap.  Currently we want this to be about 150 MB.  The trade-off
 * is that big servers hang if you don't provide enough kernelmap, but small
 * machines take a performance hit (because you need physical memory for the
 * mapping resources for kernelmap) if you make it too big.  This should all
 * be handled dynamically ... someday.
 */
#define	SYSBASE		(0-(172*1024*1024))
#define	SYSLIMIT	(0-(16*1024*1024))

extern char Sysbase[];
extern char Syslimit[];

/*
 * ARGSBASE is the base virtual address of the range which
 * the kernel uses to map the arguments for exec.
 */
#define	ARGSBASE	PROMSTART

/*
 * Size of a kernel threads stack.  It must be a whole number of pages
 * since the segment it comes from will only allocate space in pages.
 */
#define	T_STACKSZ	2*PAGESIZE

/*
 * Msgbuf size.
 */
#define	MSG_BSIZE	((8 * 1024) - sizeof (struct msgbuf_hd))

/*
 * XXX - Old names for some backwards compatibility
 * Ones not used by any code in the Sun-4 S5R4 should be nuked (this means that
 * if some driver uses it, you can only nuke it if you change the driver not to
 * use it, which may not be worth the effort; if some common component of S5R4
 * uses it, it stays around until that component is changed).
 */
#define	NBPG		MMU_PAGESIZE
#define	PGOFSET		MMU_PAGEOFFSET
#define	PGSHIFT		MMU_PAGESHIFT

#define	CLSIZE		1
#define	CLSIZELOG2	0
#define	CLBYTES		PAGESIZE
#define	CLOFSET		PAGEOFFSET
#define	CLSHIFT		PAGESHIFT
#define	clrnd(i)	(i)

/*
 * Bus types
 */
#define	BTISA		1
#define	BTEISA		2
#define	BTMCA		3

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MACHPARAM_H */
