/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_MACHPARAM_H
#define	_SYS_MACHPARAM_H

#pragma ident	"@(#)machparam.h	1.31	96/10/17 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Machine dependent parameters and limits - PowerPC version.
 */

/*
 * This is an artificial limit that will later be dynamic ??
 */
#ifdef MP
#define	NCPU 	4
#else
#define	NCPU 	1
#endif

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
 * Delay units are in microseconds.
 */
#define	usec_delay(n)	drv_usecwait(n)

/*
 * DEFAULT KERNEL THREAD stack size.
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
#define	KERNELSTART	(KERNELBASE+0x2000000)

/*
 * Define upper limit on user address space
 */
#define	USERLIMIT	KERNELBASE

/*
 * SYSBASE is the virtual address which
 * the kernel allocated memory mapping starts in all contexts.
 */
#define	SYSBASE		0xE7000000
#define	SYSLIMIT	0xEF000000

#ifndef _ASM
extern char Sysbase[];
extern char Syslimit[];
#endif

/*
 * PAGE_TABLE is the base virtual address of "the" page table.
 */
#define	PAGE_TABLE	0xF0000000	/* virtual addr of "the" page table */

/*
 * RAMDISK is the base virtual address of the ramdisk.
 */
#define	RAMDISK		0xFEF00000	/* virtual addr of ramdisk */
#define	RAMSIZE		0x00F00000	/* ramdisk size (maximum) */

/*
 * V_MSGBUF_ADDR is the base virtual address of the msgbuf.
 */
#define	V_MSGBUF_ADDR	0xE7001000	/* virtual addr of msgbuf */

/* NOTE: boot is using F3901000 for boot time NCR register mapping */

/*
 * PCI Configuration Space (needed for console initiialization)
 */
#define	PCI_CONFIG_PBASE	0x80800000
#define	PCI_CONFIG_VBASE	0xF3902000
#define	PCI_CONFIG_SIZE		0x00080000

/*
 * vsid range id used for kernel address space.
 */
#define	KERNEL_VSID_RANGE	1	/* vsids 16-31 */

/*
 * ARGSBASE is the base virtual address of the range which
 * the kernel uses to map the arguments for exec.
 */
#define	ARGSBASE	0xF3800000	/* Extends for NCARGS */

/*
 * Physical address of the PCI/ISA addresses
 */
#define	PCIIOBASE	0x80000000 /* Base of PCI IO space from CPU POV */
#define	PCIMEMORYBASE	0xc0000000 /* Base of PCI mem space from CPU POV */
#define	PCI_DMA_BASE	0x80000000 /* Base of memory from PCI POV */

/*
 * Virtual address of the PCI/ISA addresses
 */
#define	PCIISA_VBASE	0xF3000000	/* must be multiple of 64K for ml */

/*
 * Physical addresses from the I/O devices are not the same as physical
 * addresses as viewed by the processor.  We need to do a conversion
 * when computing addresses that will be used by I/O devices doing DMA.
 */

#define	CPUPHYS_TO_IOPHYS(cpuphys) (((paddr_t)cpuphys) + PCI_DMA_BASE)

/*
 * Size of a kernel threads stack.  It must be a whole number of pages
 * since the segment it comes from will only allocate space in pages.
 */
/*
 * KERNEL THREAD stack size.
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

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MACHPARAM_H */
