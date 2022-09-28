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

#ifndef _SYS_V86INTR_H
#define	_SYS_V86INTR_H

#pragma ident	"@(#)v86intr.h	1.4	94/09/03 SMI"

/* from SVR4/MP:sys/v86intr.h	1.1.3.1 */

/*
 * Definitions required by non-VP/ix code for sending v86 interrupts.
 *
 * Copyright (c) 1991 Interactive Systems Corporation.
 * All Rights Reserved.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/thread.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Virtual interrupt destination structure.  Drivers which need to
 * generate VP/ix pseudorupts maintain one instance of this structure
 * per unit.  They use the special routines v86stash(), v86deliver()
 * and v86unstash() to record the interrupt destination, deliver
 * interrupts and disable interrupts.
 */
typedef struct v86interrupt v86int_t;
struct v86interrupt {
	struct _kthread	*v86i_t;	/* Interrupt destination thread */
	v86int_t	*v86i_i;	/* Next structure on list for    */
					/* this process			 */
	kmutex_t	v86i_lock;	/* multiprocessing lock		 */
	char		v86i_state;	/* initialization state variable */
};

/*
 * Virtual interrupt stream message structure.  Streams modules needing
 * to generate pseudorupts send M_VPIXINT messages upstream after sending
 * the data for which the pseudorupt is generated.  The stream will
 * generate the pseudorupt when the M_VPIXINT message reaches the stream
 * head.  By that time the data will be available if VP/ix calls read.
 */
typedef struct v86msg v86msg_t;
struct v86msg {
	v86int_t	*v86m_i;	/* interrupt destination info */
	int		v86m_m;		/* pseudorupt type mask */
};

/*
 * Virtual interrupt bit definitions for the field "vp_viflag".
 * The low order 16 bits reflect the setting of the AT hardware
 * interrupts. The high order 16 bits are used for other interrupts.
 *
 * NOTE: The value of V86VI_SIGHDL is hard coded in the file
 * "v86enter.s". Any change to this value must also be made there.
 * "v86enter.s" is part of the ECT.
 */

#define	V86VI_NONE	0x00000000	/* No interrupts		*/
#define	V86VI_TIMER	0x00000001	/* Virtual timer interrupt	*/
#define	V86VI_KBD	0x00000002	/* Scancode rcvd when buf empty */
#define	V86VI_SLAVE	0x00000004	/* Can be reused when needed	*/
#define	V86VI_SERIAL1	0x00000008	/* Serial port 1 state change	*/
#define	V86VI_SERIAL0	0x00000010	/* Serial port 0 state change	*/
#define	V86VI_PRL1	0x00000020	/* Parallel port 1 state change */
#define	V86VI_MOUSE	0x00000020	/* Microsoft mouse		*/
#define	V86VI_DISK	0x00000040	/* Fixed and floppy disk	*/
#define	V86VI_PRL0	0x00000080	/* Parallel port 0 state change */

#define	V86VI_RCLOCK	0x00000100	/* Realtime Clock interrupt	*/
#define	V86VI_NET	0x00000200	/* Network interrupts		*/
#define	V86VI_RSVD_1	0x00000400	/* Reserved 1			*/
#define	V86VI_RSVD_2	0x00000800	/* Reserved 2			*/
#define	V86VI_RSVD_3	0x00001000	/* Reserved 3			*/
#define	V86VI_COPROC	0x00002000	/* Coprocessor interrupt	*/
#define	V86VI_FDISK	0x00004000	/* Fixed disk controller	*/
#define	V86VI_RSVD_4	0x00008000	/* Reserved 4			*/

#define	V86VI_DIV0	0x00010000	/* Divide by 0 (vector 0)	*/
#define	V86VI_SGLSTP	0x00020000	/* Single step intr (vector 1)	*/
#define	V86VI_BRKPT	0x00040000	/* Break point intr (vector 3)	*/
#define	V86VI_OVERFLOW	0x00080000	/* Overflow fault (vector 4)	*/
#define	V86VI_BOUND	0x00100000	/* Bound exception (vector 5)	*/
#define	V86VI_INVOP	0x00200000	/* Invalid opcode (vector 6)	*/
#define	V86VI_SIGHDL	0x00400000	/* Virtual signal hdlr interrupt */
#define	V86VI_MEMORY	0x00800000	/* Tracking memory has changed	*/
#define	V86VI_LBOLT	0x01000000	/* Lbolt value has changed	*/

/*
 * Virtual interrupt subcodes for V86VI_SERIAL[01] interrupts.  These
 * values appear in  the fields "xt_s[01]flag".
 */
#define	V86SI_DATA	0x00000001	/* New data available from kbd  */
#define	V86SI_MODEM	0x00000002	/* Modem status line[s] changed */

/*
 * Structure for 386 ioctls requiring user context
 */
struct  v86blk {
	struct  _kthread	*v86b_t;
	pid_t   		v86b_p_pid;	/* XXX used? */
	struct 	cred 		*v86b_p_cred;
	struct  v86dat  	*v86b_p_v86;
};

extern int v86stash();
extern void v86unstash();
extern void v86deliver();
extern void v86sdeliver();

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_V86INTR_H */
