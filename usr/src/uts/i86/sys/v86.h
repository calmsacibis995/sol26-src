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

#ifndef _SYS_V86_H
#define	_SYS_V86_H

#pragma ident	"@(#)v86.h	1.10	96/10/17 SMI"

/*
 * LIM 4.0 changes:
 * Dual-Mode Floating Point support:
 * Selectable OPcode emulation hook:
 * Copyright (c) 1989 Phoenix Technologies Ltd.
 * All Rights Reserved.
 */

#include <sys/types.h>
#include <sys/tss.h>
#include <sys/segment.h>
#include <sys/v86intr.h>
#include <sys/pcb.h>
#include <sys/t_lock.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef	unsigned char	uchar;

#define	V86VIRTSIZE	0x0100	/* Size of virtual 8086 in pages ( 1M bytes) */
#define	V86WRAPSIZE	0x0010	/* Size of copy/wrap in pages	 (64K bytes) */
#define	V86SIZE		0x0400	/* Max size of v86 segment	 ( 4M bytes) */

/*
 * Definitions for Lotus/Intel/Microsoft Expanded Memory Emulation
*/
#define	V86EMM_PGSIZE	0x4000		/* Size of Expanded memory page */
#define	V86EMM_LBASE	0x00200000	/* Expected lowest EMM logical */
					/* page addr */
#define	V86EMM_LENGTH	0x00200000	/* Expected size EMM logical pages */

/*
 * LIM 3 definitions -- for backward compatibility.
*/
#define	V86EMM_PBASE	0x000D0000	/* EMM physical page address */
#define	V86EMM_PBASE0	0x000D0000	/* EMM physical page address */
#define	V86EMM_PBASE1	0x000D4000	/* EMM physical page address */
#define	V86EMM_PBASE2	0x000D8000	/* EMM physical page address */
#define	V86EMM_PBASE3	0x000DC000	/* EMM physical page address */
#define	V86EMM_NUMPGS	4		/* # LIM 3 EMM physical pages */

/*
 *  General definitions for a dual mode process
*/

#define	XTSSADDR	((caddr_t)0x110000L)	/* XTSS addr in user mem */
#define	ARPL		0x63		/* ARPL inst, V86 invalid opcode */
#define	V86_RCS		0xF000		/* Reset CS for V86 mode	*/
#define	V86_RIP		0xFFF0		/* Reset IP for V86 mode	*/
#define	V86_TIMER_BOUND	10		/* Pending ticks limit for ECT	*/

#define	V86_SLICE	25		/* Default timeslice, >1 v86proc */
#define	V86_SLICE_SHIFT	3		/* Max shift for time slice	*/

#define	V86_PRI_NORM	0		/* Normal priority state	*/
#define	V86_PRI_LO	1		/* Low priority state, busywait	*/
#define	V86_PRI_HI	2		/* Hi priority state, pseudorupt */
#define	V86_PRI_XHI	3		/* Extra high, urgent pseudorupt */
#define	V86_SHIFT_NORM	0		/* Timeslice shift for norm pri	*/
#define	V86_SHIFT_LO	V86_SLICE_SHIFT	/* Timeslice shift for lo   pri */
#define	V86_SHIFT_HI	V86_SLICE_SHIFT	/* Timeslice shift for hi   pri */

/*
 * V86FRAME specifies the size of additional stack space used by the processor
 * when the processor takes a trap/interrupt from a v86 mode task. The
 * frame consists of the saved selectors (GS, FS, DS and ES).
 */
#define	V86FRAME	(4*4)

/*
 * The maximum number of arguments that can be used for v86() system
 * call is dependent on the maximum number of arguments allowed for
 * the sysi86 call in sysent.c. Since the v86() system call is a sub-
 * function (SI86V86) of the sysi86 call and v86() itself has sub-
 * functions, the maximum arguments for each sub-function of the v86()
 * system call is two less than the maximum allowed for the sysi86
 * system call. The value defined here is assumed in the library.
 */

#define	V86SC_MAXARGS	3	/* Includes sub-func # of v86() */

/*
 * SI86V86 is a sub system call of the system call "sysi86". The following
 * definitions are sub system calls for SI86V86 and are processed in v86.c
 */

#define	V86SC_INIT	1	/* v86init() system call	*/
#define	V86SC_SLEEP	2	/* v86sleep() system call	*/
#define	V86SC_MEMFUNC	3	/* v86memfunc() system call	*/
#define	V86SC_IOPL	4	/* v86iopriv () system call	*/
/*
 * The V86 timer has a frequency of 18.2 times a second. The Unix
 * timer has a frequency of HZ times a second. So the following
 * value ensures that the ECT gets an interrupt at least as often
 * as it needs one.
 */

#define	V86TIMER	((hz*10)/182)

/*  Software Interrupt mask bit array definitions */

#define	V86_IMASKSIZE	256		/* Max number of software INTs  */
#define	V86_IMASKBITS	((V86_IMASKSIZE + 31) / 32)
					/* # of bits for software INTs  */

/*
 * The offsets of members "xt_viflag", "xt_signo" and "xt_hdlr" are
 * hard coded in the file "v86enter.s". So any changes to the struc-
 * that changes these offsets have to be reflected in this file.
 * This file is part of the ECT.
 *
 * NOTE: The value of "xt_intr_pin" should be 0xFF or 0.
 */

typedef struct v86xtss {
	struct tss386	xt_tss;		/* Normal TSS structure		*/
	unsigned int	*xt_vflptr;	/* Ptr to 8086 virtual flags	*/
	unsigned char	xt_magictrap;	/* Saved byte of virt intr	*/
	unsigned char	xt_magicstat;	/* Status of magic byte		*/
	unsigned char	xt_tslice_shft;	/* Time slice shift requested	*/
	unsigned char	xt_intr_pin;	/* Interrupt to virtual machine */
	time_t		xt_lbolt;	/* Lightning bolt value		*/
	unsigned int	xt_viflag;	/* Virtual interrupt flag	*/
	unsigned int	xt_vimask;	/* Mask for virtual interrupts	*/
	unsigned int 	xt_signo;	/* Sig number on V86VI_SIGHDL	*/
	int		(*xt_hdlr)();	/* Sig handler for V86VI_SIGHDL */
	unsigned int	xt_timer_count;	/* Number of pending timer ticks */
	unsigned int	xt_timer_bound;	/* Ticks before forcing ECT in	*/
	uint_t xt_imaskbits[V86_IMASKBITS]; /* Bit map for software INTs */
					/* XXX - this changes for P5	*/
	unsigned int	xt_oldbitmap[32]; /* For I/O bitmap on old ECTs	*/
	unsigned char	xt_magic[4];	/* XTSS version indicator	*/
	unsigned int	xt_viurgent;	/* Mask for urgent interrupts	*/
	unsigned int	xt_vitoect;	/* Mask for interrupts to ECT	*/
	ushort		xt_vp_relcode;	/* VP/ix release code number	*/
	ushort		xt_oemcode;	/* OEM code number		*/
	ushort		xt_op_emul;	/* OPcode emulation enable mask */
	ushort		xt_rsvd0;	/* Reserved for future expansion */
	unsigned int	xt_reserved[8];	/* Reserved for future expansion */
} xtss_t;

/*
 * VP/ix release code (refers to kernel,
 * but non-driver based, supported functionality).
 *
 * 1 = D-M FP + LIM 4.0
 * 2 = Ver.1 + OPcode emulation xtss hook.
 *
 * set into xt_vp_relcode in v86.c:v86init()
*/
#define	VP_RELCODE	2

/* OPcode emulation bit defines (one bit/OPcode). */
#define	EN_VTR		0x0001		/* CGA status port read */

/* DEFINE for CGA status emulation table (allocated in vx/space.c) */
#define	CS_MAX		32

/*
 * Definitions for the field "xt_magicstat". The location "xt_magictrap"
 *  is valid only when "xt_magicstat" field is set to XT_MSTAT_OPSAVED.
 */

#define	XT_MSTAT_NOPROCESS	0	/* Do not process virtual intr  */
#define	XT_MSTAT_PROCESS	1	/* Process virtual intr		*/
#define	XT_MSTAT_OPSAVED	2	/* v86 program opcode saved	*/
#define	XT_MSTAT_POSTING	3	/* Phoenix defined		*/

struct v86parm {
	xtss_t		*xtssp;		/* Ptr to XTSS in user data	*/
	unsigned long	szxtss;		/* Length in bytes of XTSS	*/
	unsigned char	magic[4];	/* XTSS version indicator	*/
	unsigned long	szbitmap;	/* Length in bytes of I/O bitmap */
};

typedef struct v86memory {
	int	vmem_cmd;	/* Sub command for screen func 	*/
	paddr_t	vmem_physaddr;	/* Physical memory base for map	*/
	caddr_t	vmem_membase;	/* Screen memory base 		*/
	int	vmem_memlen;	/* Length of screen memory 	*/
} v86memory_t;

/*
 * Definitions for the field "vmem_cmd".
 */

#define	V86MEM_MAP	1		/* Map virt addr to physical	*/
#define	V86MEM_TRACK	2		/* Track memory modifications	*/
#define	V86MEM_UNTRACK	3		/* Untrack memory modifications */
#define	V86MEM_UNMAP	4		/* Unmap virt addr		*/
#define	V86MEM_EMM	5		/* LIM expanded memory emulation */
#define	V86MEM_GROW	6		/* Grow Lim expanded memory */

/*
 * Ops structure for both VPIX and MERGE386 functions.
 */

typedef struct {
	int	(* v86_swtch)();	/* called from resume() */
	int	(* v86_rtt)();		/* called from sys_rtt() */
	int	(* v86_sighdlint)();	/* called from sendsig() */
	int	(* v86_exit)();		/* called from exit() */
	int	(* v86_reserved)();	/* reserved */
} v86_ops;


typedef struct v86dat {
	v86_ops	vp_ops;
	v86int_t *vp_ilist;		/* Head of pseudorupt list	*/
	time_t	vp_lbolt_update;	/* Last time timer was updated  */
	u_short	vp_oldtr;		/* Old task register		*/
	u_short	vp_szxtss;		/* Size of XTSS in user space   */
	xtss_t	*vp_xtss;		/* Address of nailed XTSS	*/
	v86memory_t vp_mem;		/* Memory map/track definitions */
	int	vp_wakeid;		/* ID of v86sleep timeout	*/
	char	vp_slice_shft;		/* Last slice shift from xtss   */
	char	vp_pri_state;		/* Priority state = hi,norm,lo	*/
					/* Virtual86 task fp save area  */
	fpu_ctx_t vp_fpu;		/* fpu context for v86mode task */
	struct seg_desc vp_xtss_desc;	/* Segment descriptor for xtss  */
	kcondvar_t vp_cv;		/* conditional variable 	*/
	kmutex_t vp_mutex;		/* mutex lock 			*/
} v86_t;

#define	V86_MAGIC0	'X'		/* Byte for XTSS magic[0]	*/
#define	V86_MAGIC1	'T'		/* Byte for XTSS magic[1]	*/

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_V86_H */
