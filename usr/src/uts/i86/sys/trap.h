/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any		*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_TRAP_H
#define	_SYS_TRAP_H

#pragma ident	"@(#)trap.h	1.7	94/05/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Trap type values
 */

#define	T_ZERODIV	0	/* divide by 0 error		*/
#define	T_SGLSTP	1	/* single step			*/
#define	T_NMIFLT	2	/* NMI				*/
#define	T_BPTFLT	3	/* breakpoint fault		*/
#define	T_OVFLW		4	/* INTO overflow fault		*/
#define	T_BOUNDFLT	5	/* BOUND instruction fault	*/
#define	T_ILLINST	6	/* invalid opcode fault		*/
#define	T_NOEXTFLT	7	/* extension not available fault */
#define	T_DBLFLT	8	/* double fault			*/
#define	T_EXTOVRFLT	9	/* extension overrun fault	*/
#define	T_TSSFLT	10	/* invalid TSS fault		*/
#define	T_SEGFLT	11	/* segment not present fault	*/
#define	T_STKFLT	12	/* stack fault			*/
#define	T_GPFLT		13	/* general protection fault	*/
#define	T_PGFLT		14	/* page fault			*/
#define	T_EXTERRFLT	16	/* extension error fault	*/
#define	T_ALIGNMENT	17	/* alignment check error (486 only) */
#define	T_ENDPERR	33	/* emulated extension error flt	*/
#define	T_ENOEXTFLT	32	/* emulated ext not present	*/
#define	T_FASTTRAP	210	/* fast system call		*/

/*
 * Pseudo traps.
 * XXX - check?
 */
#define	T_INTERRUPT		0x100
#define	T_FAULT			0x200
#define	T_AST			0x400
#define	T_SYSCALL		0x180		/* XXX - check? */


/*
 *  Values of error code on stack in case of page fault
 */

#define	PF_ERR_MASK	0x01	/* Mask for error bit */
#define	PF_ERR_PAGE	0	/* page not present */
#define	PF_ERR_PROT	1	/* protection error */
#define	PF_ERR_WRITE	2	/* fault caused by write (else read) */
#define	PF_ERR_USER	4	/* processor was in user mode */
				/*	(else supervisor) */

/*
 *  Definitions for fast system call subfunctions
 */
#define	T_FNULL		0	/* Null trap for testing		*/
#define	T_FGETFP	1	/* Get emulated FP context		*/
#define	T_FSETFP	2	/* Set emulated FP context		*/
#define	T_GETHRTIME	3	/* Get high resolution time		*/
#define	T_GETHRVTIME	4	/* Get high resolution virtual time	*/
#define	T_GETHRESTIME	5	/* Get high resolution time		*/

#define	T_LASTFAST	5	/* Last valid subfunction		*/

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TRAP_H */
