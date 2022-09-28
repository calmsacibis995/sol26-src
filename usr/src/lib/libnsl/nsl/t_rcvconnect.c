/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#ident	"@(#)t_rcvconnect.c	1.17	96/10/14 SMI"	/* SVr4.0 1.3	*/

#include <stdlib.h>
#include <errno.h>
#include <stropts.h>
#include <rpc/trace.h>
#include <xti.h>
#include <sys/timod.h>
#include "timt.h"
#include "tx.h"


/*
 * t_rcvconnect() is documented to be only called with non-blocking
 * endpoints for asynchronous connection establishment. However, the
 * direction taken by XTI is to allow it to be called if t_connect()
 * fails with TSYSERR/EINTR and state is T_OUTCON (i.e. T_CONN_REQ was
 * sent down). This implies that an interrupted synchronous connection
 * establishment which was interrupted after connection request was transmitted
 * can now be completed by calling t_rcvconnect()
 */
int
_tx_rcvconnect(fd, call, api_semantics)
int fd;
struct t_call *call;
int api_semantics;
{
	register struct _ti_user *tiptr;
	int retval, sv_errno;
	sigset_t mask;
	struct strbuf ctlbuf;
	int didalloc;

	trace2(TR_t_rcvconnect, 0, fd);
	if ((tiptr = _t_checkfd(fd, 0, api_semantics)) == NULL) {
		sv_errno = errno;
		trace2(TR_t_rcvconnect, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);

	if (api_semantics == TX_XTI_API) {
		/*
		 * User level state verification only done for XTI
		 * because doing for TLI may break existing applications
		 */
		if (tiptr->ti_state != T_OUTCON) {
			t_errno = TOUTSTATE;
			MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
			trace2(TR_t_rcvconnect, 1, fd);
			return (-1);
		}
	}

	/*
	 * Acquire ctlbuf for use in sending/receiving control part
	 * of the message.
	 */
	if (_t_acquire_ctlbuf(tiptr, &ctlbuf, &didalloc) < 0) {
		sv_errno = errno;
		MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
		trace2(TR_t_rcvconnect, 1, fd);
		errno = sv_errno;
		return (-1);
	}

	if (((retval = _t_rcv_conn_con(tiptr, call, &mask,
	    &ctlbuf, api_semantics)) == 0) ||
	    (t_errno == TBUFOVFLW)) {
		_T_TX_NEXTSTATE(T_RCVCONNECT, tiptr,
			"t_rcvconnect: Invalid state on event T_RCVCONNECT");
	}
	sv_errno = errno;
	if (didalloc)
		free(ctlbuf.buf);
	else
		tiptr->ti_ctlbuf = ctlbuf.buf;
	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	trace2(TR_t_rcvconnect, 1, fd);
	errno = sv_errno;
	return (retval);
}
