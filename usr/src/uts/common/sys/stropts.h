/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1994-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_STROPTS_H
#define	_SYS_STROPTS_H

#pragma ident	"@(#)stropts.h	1.25	96/10/18 SMI"	/* SVr4.0 11.20	*/

#include <sys/feature_tests.h>
#include <sys/types.h>
/*
 * For FMNAMESZ define.
 */
#include <sys/conf.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Write options.
 */
#define	SNDZERO		0x001		/* send a zero length message */
#define	SNDPIPE		0x002		/* send SIGPIPE on write and */
					/* putmsg if sd_werror is set */

/*
 * Read options
 */

#define	RNORM		0x000		/* read msg norm */
#define	RMSGD		0x001		/* read msg discard */
#define	RMSGN		0x002		/* read msg no discard */

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	RMODEMASK	0x003		/* all above bits */
#endif

/*
 * These next three read options are added for the sake of
 * user-level transparency.  RPROTDAT will cause the stream head
 * to treat the contents of M_PROTO and M_PCPROTO message blocks
 * as data.  RPROTDIS will prevent the stream head from failing
 * a read with EBADMSG if an M_PROTO or M_PCPROTO message is on
 * the front of the stream head read queue.  Rather, the protocol
 * blocks will be silently discarded and the data associated with
 * the message (in linked M_DATA blocks), if any, will be delivered
 * to the user.  RPROTNORM sets the default behavior, where read
 * will fail with EBADMSG if an M_PROTO or M_PCPROTO are at the
 * stream head.
 */
#define	RPROTDAT	0x004		/* read protocol messages as data */
#define	RPROTDIS	0x008		/* discard protocol messages, but */
					/* read data portion */
#define	RPROTNORM	0x010

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	RPROTMASK	0x01c		/* all RPROT bits */
#endif

/*
 * Error options
 */

/*
 * Error options to adjust the stream head error behavior with respect
 * to M_ERROR message for read and write side errors respectively.
 * The normal case is that the read/write side error is
 * persistent and these options allow the application or streams module/driver
 * to specify that errors are nonpersistent. In this case the error is cleared
 * after having been returned to read(), getmsg(), ioctl(), write(), putmsg(),
 * etc.
 */
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	RERRNORM	0x001		/* Normal, persistent read errors */
#define	RERRNONPERSIST	0x002		/* Nonpersistent read errors */

#define	RERRMASK	(RERRNORM|RERRNONPERSIST)

#define	WERRNORM	0x004		/* Normal, persistent write errors */
#define	WERRNONPERSIST	0x008		/* Nonpersistent write errors */

#define	WERRMASK	(WERRNORM|WERRNONPERSIST)
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

/*
 * Flush options
 */

#define	FLUSHR		0x01		/* flush read queue */
#define	FLUSHW		0x02		/* flush write queue */
#define	FLUSHRW		0x03		/* flush both queues */
#define	FLUSHBAND	0x04		/* flush only band specified */
					/* in next byte */
/*
 * Copy options for M_SETOPS/SO_COPYOPT
 */
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	MAPINOK		0x01		/* ok to mapin instead of copyin */
#define	NOMAPIN		0x02		/* don't mapin on the transmit side */
#define	REMAPOK		0x04		/* ok to pageflip instead of copyout */
#define	NOREMAP		0x08		/* don't pageflip on the receive side */
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

/*
 * Events for which to be sent SIGPOLL signal and for which events
 * can be posted by the I_SETEV ioctl.
 */
#define	S_INPUT		0x0001		/* any msg but hipri on read Q */
#define	S_HIPRI		0x0002		/* high priority msg on read Q */
#define	S_OUTPUT	0x0004		/* write Q no longer full */
#define	S_MSG		0x0008		/* signal msg at front of read Q */
#define	S_ERROR		0x0010		/* error msg arrived at stream head */
#define	S_HANGUP	0x0020		/* hangup msg arrived at stream head */
#define	S_RDNORM	0x0040		/* normal msg on read Q */
#define	S_WRNORM	S_OUTPUT
#define	S_RDBAND	0x0080		/* out of band msg on read Q */
#define	S_WRBAND	0x0100		/* can write out of band */
#define	S_BANDURG	0x0200		/* modifier to S_RDBAND, to generate */
					/* SIGURG instead of SIGPOLL */

/*
 * Flags for getmsg() and putmsg() syscall arguments.
 * "RS" stands for recv/send.  The system calls were originally called
 * recv() and send(), but were renamed to avoid confusion with the BSD
 * calls of the same name.  A value of zero will cause getmsg() to return
 * the first message on the stream head read queue and putmsg() to send
 * a normal priority message.
 *
 * Flags for strmakemsg() arguments (should define strmakemsg() flags).
 * Used to determine the message type of the control part of a message,
 * if RS_HIPRI, M_PCPROTO, else M_PROTO.
 * Used to determine if data should be copyin()ed for the data part of
 * a message, if STRUIO_POSTPONE, no copyin(), else copyin().
 */

#define	RS_HIPRI	0x01		/* send/recv high priority message */
#define	STRUIO_POSTPONE	0x08		/* postpone copyin() for struio() */
#define	STRUIO_MAPIN	0x10		/* use strmapin() instead of copyin() */

/*
 * Flags for getpmsg() and putpmsg() syscall arguments.
 */

/*
 * These are settable by the user and will be set on return
 * to indicate the priority of message received.
 */
#define	MSG_HIPRI	0x01		/* send/recv high priority message */
#define	MSG_ANY		0x02		/* recv any messages */
#define	MSG_BAND	0x04		/* recv messages from specified band */
#define	MSG_XPG4	0x08		/* XPG4 application specific flag */

#ifdef _KERNEL

/*
 * Additional private flags for kstrgetmsg and kstrputmsg.
 * These must be bit-wise distinct from the above MSG flags.
 */
#define	MSG_IPEEK	0x10		/* Peek - don't remove the message */
#define	MSG_DISCARDTAIL	0x20		/* Discard tail if it doesn't fit */
#define	MSG_HOLDSIG	0x40		/* Ignore signals. */
#define	MSG_IGNERROR	0x80		/* Ignore stream head errors */
#define	MSG_DELAYERROR	0x100		/* Delay error check until we sleep */
#define	MSG_IGNFLOW	0x200		/* Ignore flow control */
#define	MSG_NOMARK	0x400		/* Do not read if message is marked */

#endif /* _KERNEL */

/*
 * Flags returned as value of getmsg() and getpmsg() syscall.
 */
#define	MORECTL		1		/* more ctl info is left in message */
#define	MOREDATA	2		/* more data is left in message */

/*
 * Define to indicate that all multiplexors beneath a stream should
 * be unlinked.
 */
#define	MUXID_ALL	(-1)

/*
 * Flag definitions for the I_ATMARK ioctl.
 */
#define	ANYMARK		0x01
#define	LASTMARK	0x02

/*
 *  Stream Ioctl defines
 */
#define	STR		('S'<<8)
/* (STR|000) in use */
#define	I_NREAD		(STR|01)
#define	I_PUSH		(STR|02)
#define	I_POP		(STR|03)
#define	I_LOOK		(STR|04)
#define	I_FLUSH		(STR|05)
#define	I_SRDOPT	(STR|06)
#define	I_GRDOPT	(STR|07)
#define	I_STR		(STR|010)
#define	I_SETSIG	(STR|011)
#define	I_GETSIG	(STR|012)
#define	I_FIND		(STR|013)
#define	I_LINK		(STR|014)
#define	I_UNLINK	(STR|015)
/* (STR|016) in use */
#define	I_PEEK		(STR|017)
#define	I_FDINSERT	(STR|020)
#define	I_SENDFD	(STR|021)

#if defined(_KERNEL)
#define	I_RECVFD	(STR|022)
#define	I_E_RECVFD	(STR|016)
#else	/* user level definition */
#define	I_RECVFD	(STR|016)	/* maps to kernel I_E_RECVFD */
#endif /* defined(_KERNEL) */

#define	I_SWROPT	(STR|023)
#define	I_GWROPT	(STR|024)
#define	I_LIST		(STR|025)
#define	I_PLINK		(STR|026)
#define	I_PUNLINK	(STR|027)
#define	I_SETEV		(STR|030)
#define	I_GETEV		(STR|031)
#define	I_STREV		(STR|032)
#define	I_UNSTREV	(STR|033)
#define	I_FLUSHBAND	(STR|034)
#define	I_CKBAND	(STR|035)
#define	I_GETBAND	(STR|036)
#define	I_ATMARK	(STR|037)
#define	I_SETCLTIME	(STR|040)
#define	I_GETCLTIME	(STR|041)
#define	I_CANPUT	(STR|042)
#define	I_SERROPT	(STR|043)
#define	I_GERROPT	(STR|044)
#define	I_ESETSIG	(STR|045)
#define	I_EGETSIG	(STR|046)

/*
 * User level ioctl format for ioctls that go downstream (I_STR)
 */
struct strioctl {
	int 	ic_cmd;			/* command */
	int	ic_timout;		/* timeout value */
	int	ic_len;			/* length of data */
	char	*ic_dp;			/* pointer to data */
};

/*
 * Value for timeouts (ioctl, select) that denotes infinity
 */
#define	_INFTIM		-1
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	INFTIM		_INFTIM
#endif

/*
 * Stream buffer structure for putmsg and getmsg system calls
 */
struct strbuf {
	int	maxlen;			/* no. of bytes in buffer */
	int	len;			/* no. of bytes returned */
	char	*buf;			/* pointer to data */
};

/*
 * Stream I_PEEK ioctl format
 */
struct strpeek {
	struct strbuf	ctlbuf;
	struct strbuf	databuf;
	long		flags;
};

/*
 * Stream I_FDINSERT ioctl format
 */
struct strfdinsert {
	struct strbuf	ctlbuf;
	struct strbuf	databuf;
	long		flags;
	int		fildes;
	int		offset;
};

/*
 * Receive file descriptor structure
 */

#if defined(_KERNEL)

struct o_strrecvfd {	/* SVR3 syscall structure */
	union {
		struct file *fp;
		int fd;
	} f;
	o_uid_t uid;		/* always ushort */
	o_gid_t gid;
	char fill[8];
};

/*
 * Although EFT is enabled in the kernel we kept the following definition
 * to support an EFT application on a 4.0 non-EFT system.
 */

struct e_strrecvfd {	/* SVR4 expanded syscall interface structure */
	union {
		struct file *fp;
		int fd;
	} f;
	uid_t uid;		/* always long */
	gid_t gid;
	char fill[8];
};

struct strrecvfd {	/* Kernel structure dependent on EFT definition */
	union {
		struct file *fp;
		int fd;
	} f;
	uid_t uid;
	gid_t gid;
	char fill[8];
};

#else	/* EFT user definition */

struct strrecvfd {
	int fd;
	uid_t uid;
	gid_t gid;
#if defined(_XPG4_2)
	char __fill[8];
#else
	char fill[8];
#endif
};

#endif	/* defined(_KERNEL) */

/*
 * For I_LIST ioctl.
 */
struct str_mlist {
	char l_name[FMNAMESZ+1];
};

struct str_list {
	int sl_nmods;
	struct str_mlist *sl_modlist;
};

/*
 * For I_FLUSHBAND ioctl.  Describes the priority
 * band for which the operation applies.
 */
struct bandinfo {
	unsigned char	bi_pri;
	int		bi_flag;
};

/*
 * The argument for I_ESETSIG and I_EGETSIG ioctls.
 */
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
struct strsigset {
	pid_t		ss_pid;		/* pgrp if negative */
	long		ss_events;	/* S_ events */
};
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#ifdef	_XPG4_2
#ifdef	__PRAGMA_REDEFINE_EXTNAME

#pragma	redefine_extname putmsg		__xpg4_putmsg
#pragma	redefine_extname putpmsg	__xpg4_putpmsg

#else	/* __PRAGMA_REDEFINE_EXTNAME */

#define	putmsg	__xpg4_putmsg
#define	putpmsg	__xpg4_putpmsg

#endif	/* __PRAGMA_REDEFINE_EXTNAME */
#endif	/* _XPG4_2 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_STROPTS_H */
