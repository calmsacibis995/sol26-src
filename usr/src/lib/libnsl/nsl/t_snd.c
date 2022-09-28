/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#ident	"@(#)t_snd.c	1.21	96/10/14 SMI"	/* SVr4.0 1.3.1.2	*/

#include <rpc/trace.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stropts.h>
#include <sys/stream.h>
#define	_SUN_TPI_VERSION 1
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <xti.h>
#include <syslog.h>
#include "timt.h"
#include "tx.h"


#define	ALL_VALID_FLAGS (T_MORE|T_EXPEDITED)
int
_tx_snd(fd, buf, nbytes, flags, api_semantics)
int fd;
register char *buf;
unsigned nbytes;
int flags;
int api_semantics;
{
	struct T_data_req datareq;
	struct strbuf ctlbuf, databuf;
	unsigned int bytes_sent, bytes_remaining, bytes_to_send;
	char *curptr;
	register struct _ti_user *tiptr;
	int band;
	int retval;
	sigset_t mask;
	int sv_errno;
	int doputmsg = 0;
	int32_t tsdu_limit;

	trace4(TR_t_snd, 0, fd, nbytes, flags);
	if ((tiptr = _t_checkfd(fd, 0, api_semantics)) == NULL) {
		sv_errno = errno;
		trace4(TR_t_snd, 1, fd, nbytes, flags);
		errno = sv_errno;
		return (-1);
	}
	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);

	if (tiptr->ti_servtype == T_CLTS) {
		t_errno = TNOTSUPPORT;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace4(TR_t_snd, 1, fd, nbytes, flags);
		return (-1);
	}

	if (api_semantics == TX_XTI_API) {
		/*
		 * User level state verification only done for XTI
		 * because doing for TLI may break existing applications
		 */
		if (! (tiptr->ti_state == T_DATAXFER ||
		    tiptr->ti_state == T_INREL)) {
			t_errno = TOUTSTATE;
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace4(TR_t_snd, 1, fd, nbytes, flags);
			return (-1);
		}
		/*
		 * XXX
		 * Is it OK to do this TBADFLAG check when XTI spec
		 * is being extended with new and interesting flags
		 * everyday ?
		 */
		if ((flags & ~(ALL_VALID_FLAGS)) != 0) {
			t_errno = TBADFLAG;
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace4(TR_t_snd, 1, fd, nbytes, flags);
			return (-1);
		}
		if (flags & T_EXPEDITED)
			tsdu_limit = tiptr->ti_etsdusize;
		else {
			/* normal data */
			tsdu_limit = tiptr->ti_tsdusize;
		}

		if ((tsdu_limit > 0) && /* limit meaningful and ... */
		    (nbytes > (uint32_t)tsdu_limit)) {
			t_errno = TBADDATA;
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace4(TR_t_snd, 1, fd, nbytes, flags);
			return (-1);
		}

		/*
		 * Check for incoming disconnect. Did anyone say
		 * "performance" ? Didn't hear that.
		 */
		if (_t_look_locked(fd, tiptr, api_semantics) == T_DISCONNECT) {
			t_errno = TLOOK;
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace4(TR_t_snd, 1, fd, nbytes, flags);
			return (-1);
		}

	}

	/* sending zero length data when not allowed */
	if (nbytes == 0 && !(tiptr->ti_prov_flag & (SENDZERO|OLD_SENDZERO))) {
		t_errno = TBADDATA;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace4(TR_t_snd, 1, fd, nbytes, flags);
		return (-1);
	}

	doputmsg = (int) ((tiptr->ti_tsdusize != 0) || (flags & T_EXPEDITED));

	if (doputmsg) {
		/*
		 * Initialize ctlbuf for use in sending/receiving control part
		 * of the message.
		 */
		ctlbuf.maxlen = sizeof (struct T_data_req);
		ctlbuf.len = sizeof (struct T_data_req);
		ctlbuf.buf = (char *) &datareq;

		band = TI_NORMAL; /* band 0 */
		if (flags & T_EXPEDITED) {
			datareq.PRIM_type = T_EXDATA_REQ;
			if (! (tiptr->ti_prov_flag & EXPINLINE))
				band = TI_EXPEDITED; /* band > 0 */
		} else
			datareq.PRIM_type = T_DATA_REQ;
	}

	bytes_remaining = nbytes;
	curptr = buf;

	/*
	 * Calls to send data (write or putmsg) can potentially
	 * block, for MT case, we drop the lock and enable signals here
	 * and acquire it back
	 */
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	do {
		bytes_to_send = bytes_remaining;
		if (doputmsg) {
			/*
			 * transport provider supports TSDU concept
			 * (unlike TCP) or it is expedited data.
			 * In this case do the fragmentation
			 */
			if (bytes_to_send > (unsigned int)tiptr->ti_maxpsz) {
				datareq.MORE_flag = 1;
				bytes_to_send = (unsigned int)tiptr->ti_maxpsz;
			} else {
				if (flags&T_MORE)
					datareq.MORE_flag = 1;
				else
					datareq.MORE_flag = 0;
			}
			databuf.maxlen = bytes_to_send;
			databuf.len = bytes_to_send;
			databuf.buf = curptr;
			retval = putpmsg(fd, &ctlbuf, &databuf, band, MSG_BAND);
			if (retval == 0)
				bytes_sent = bytes_to_send;
		} else {
			/*
			 * transport provider does *not* support TSDU concept
			 * (e.g. TCP) and it is not expedited data. A
			 * perf. optimization is used. Note: the T_MORE
			 * flag is ignored here even if set by the user.
			 */
			retval = write(fd, curptr, bytes_to_send);
			if (retval >= 0) {
				/* Amount that was actually sent */
				bytes_sent = retval;
			}
		}

		if (retval < 0) {
			if (nbytes == bytes_remaining) {
				/*
				 * Error on *first* putmsg/write attempt.
				 * Return appropriate error
				 */
				if (errno == EAGAIN)
					t_errno = TFLOW;
				else
					t_errno = TSYSERR;
				sv_errno = errno;
				trace4(TR_t_snd, 1, fd, nbytes, flags);
				errno = sv_errno;
				return (-1); /* return error */
			} else {
				/*
				 * Not the first putmsg/write
				 * [ partial completion of t_snd() case.
				 *
				 * Error on putmsg/write attempt but
				 * some data was transmitted so don't
				 * return error. Don't attempt to
				 * send more (break from loop) but
				 * return OK.
				 */
				break;
			}
		}
		bytes_remaining = bytes_remaining - bytes_sent;
		curptr = curptr + bytes_sent;
	} while (bytes_remaining != 0);

	_T_TX_NEXTSTATE(T_SND, tiptr, "t_snd: invalid state event T_SND");
	sv_errno = errno;
	trace4(TR_t_snd, 1, fd, nbytes, flags);
	errno = sv_errno;
	return (nbytes - bytes_remaining);
}
