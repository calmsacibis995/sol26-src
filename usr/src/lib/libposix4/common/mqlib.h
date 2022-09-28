/*	Copyright (c) 1993 SMI	*/
/*	 All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_MQLIB_H
#define	_MQLIB_H

#pragma ident	"@(#)mqlib.h	1.4	94/10/14 SMI"

/*
 * mqlib.h - Header file for POSIX.4 message queue
 */

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/feature_tests.h>

#include <sys/types.h>
#include <synch.h>
#include <sys/types.h>
#include <signal.h>
#include <mqueue.h>


/*
 * Default values per message queue
 */
#define	MQ_MAXMSG	128
#define	MQ_MAXSIZE	1024
#define	MQ_MAXPRIO	sysconf(_SC_MQ_PRIO_MAX)
#define	MQ_MAXOPEN	sysconf(_SC_MQ_OPEN_MAX)

#define	MQ_MAGIC	0x4d534751		/* "MSGQ" */
#define	MQ_SIZEMASK	(~0x3)
/*
 * Message header which is part of messages in link list
 */

typedef struct mq_msg_hdr {
	struct mq_msg_hdr *msg_next;	/* next message in the link */
	long		msg_len;	/* length of the message */
} msghdr_t;

/*
 * message queue descriptor structure
 */
typedef struct mq_des {
	ulong 		mqd_magic;	/* magic # to identify mq_des */
	struct mq_header *mqd_mq;	/* address pointer of message Q */
	ulong		mqd_flags;	/* operation flag per open */
	struct mq_dn	*mqd_mqdn;	/* open	description */
} mqdes_t;

/*
 * message queue description
 */
struct mq_dn {
	ulong		mqdn_flags;	/* open description flags */
};


/*
 * message queue common header which is part of mmap()ed file.
 */
typedef struct mq_header {
	/* first field should be mq_totsize, DO NOT insert before this	*/
	long		mq_totsize;	/* total size of the Queue */
	ulong_t		mq_magic;	/* support more implementations */
	ulong_t		mq_flag;	/* various message Q flags */
	long		mq_maxsz;	/* max size of each message */
	long		mq_maxmsg;	/* max messages in the queue */
	long		mq_count;	/* current count of messages */
	long		mq_waiters;	/* current count of receivers */
	long		mq_maxprio;	/* maximum MQ priority */
	long		mq_curmaxprio;	/* current maximum MQ priority */
	ulong_t		*mq_maskp;	/* pointer to priority bitmask */
	msghdr_t	*mq_freep;	/* free message's head pointer */
	msghdr_t	**mq_headpp;	/* pointer to head pointers */
	msghdr_t	**mq_tailpp;	/* pointer to tail pointers */
	signotify_id_t	mq_sigid;	/* notification id */
	mqdes_t		*mq_des;	/* pointer to msg Q descriptor */
	mutex_t		mq_lock;	/* to protect pointers of Qs */
	cond_t		mq_recv;	/* Sync for receiving a message */
	cond_t		mq_send;	/* Sync for sending a message */
	long		mq_pad[4];	/* reserved for future */
} mqhdr_t;


/*
 * REQUIRES sys/types.h
 */

#define	BT_NBIPUL	32	/* n bits per ulong */
#define	BT_ULSHIFT	5	/* log base 2 of BT_NBIPUL, */
				/* to extract word index */
#define	BT_ULMASK	0x1f	/* to extract bit index */

/*
 * bitmap is a ulong *, bitindex an index_t
 */

/*
 * word in map
 */
#define	BT_WIM(bitmap, bitindex) \
	((bitmap)[(bitindex) >> BT_ULSHIFT])
/*
 * bit in word
 */
#define	BT_BIW(bitindex) \
	(1 << ((bitindex) & BT_ULMASK))

/*
 * BT_BITOUL == n bits to n ulongs
 */
#define	BT_BITOUL(nbits) \
	((nbits) >> BT_ULSHIFT)
#define	BT_TEST(bitmap, bitindex) \
	(BT_WIM((bitmap), (bitindex)) & BT_BIW(bitindex))
#define	BT_SET(bitmap, bitindex) \
	{ BT_WIM((bitmap), (bitindex)) |= BT_BIW(bitindex); }
#define	BT_CLEAR(bitmap, bitindex) \
	{ BT_WIM((bitmap), (bitindex)) &= ~BT_BIW(bitindex); }


/* prototype for signotify system call. unexposed to user */
int _signotify(int cmd, siginfo_t *sigonfo, signotify_id_t *sn_id);

#ifdef	__cplusplus
}
#endif

#endif	/* _MQLIB_H */
