/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#ident	"@(#)t_look.c	1.21	96/10/31 SMI"	/* SVr4.0 1.2	*/

#include <rpc/trace.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stream.h>
#include <stropts.h>
#define	_SUN_TPI_VERSION 1
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <xti.h>
#include <assert.h>
#include "timt.h"
#include "tx.h"

int
_tx_look(fd, api_semantics)
int fd;
int api_semantics;
{
	int state;
	int sv_errno;
	register struct _ti_user *tiptr;
	sigset_t mask;

	trace2(TR_t_look, 0, fd);
	if ((tiptr = _t_checkfd(fd, 0, api_semantics)) == NULL) {
		sv_errno = errno;
		trace2(TR__t_look, 1, fd);
		errno = sv_errno;
		return (-1);
	}
	MUTEX_LOCK_THRMASK(&tiptr->ti_lock, mask);

	state = _t_look_locked(fd, tiptr, api_semantics);

	sv_errno = errno;

	MUTEX_UNLOCK_THRMASK(&tiptr->ti_lock, mask);
	trace2(TR_t_look, 1, fd);
	errno = sv_errno;
	return (state);
}

/*
 * _t_look_locked() assumes tiptr->ti_lock lock is already held and signals
 * already blocked in MT case.
 * Intended for use by other TLI routines only.
 */
int
_t_look_locked(fd, tiptr, api_semantics)
int fd;
register struct _ti_user *tiptr;
int api_semantics;
{
	struct strpeek strpeek;
	int retval, sv_errno;
	union T_primitives *pptr;
	long type;
	unsigned long ctltype;

	trace2(TR__t_look_locked, 0, fd);

	assert(MUTEX_HELD(&tiptr->ti_lock));

#ifdef notyet
	if (api_semantics == TX_XTI_API) {
		/*
		 * XTI requires the strange T_GODATA and T_GOEXDATA
		 * events which are almost brain-damaged but thankfully
		 * not tested. Anyone feeling the need for those should
		 * consider the need for using non-blocking endpoint.
		 * Probably introduced at the behest of some weird-os
		 * vendor which did not understand the non-blocking endpoint
		 * option.
		 * We choose not to implment these mis-features.
		 * Here is the plan-of-action (POA)if we are ever forced
		 * to implement these.
		 * - When returning TFLOW set state to indicate if it was
		 *   a normal or expedited data send attempt.
		 * - In routines that set TFLOW, clear the above set state
		 *   on each entry/reentry
		 * - In this routine, if that state flag is set,
		 * do a I_CANPUT on appropriate band to to see if it
		 * is writeable. If that indicates that the band is
		 * writeable, return T_GODATA or T_GOEXDATA event.
		 *
		 * Actions are also influenced by whether T_EXDATA_REQ stays
		 * band 1 or goes to band 0 if EXPINLINE is set
		 *
		 * We will also need to sort out if "write side" events
		 * (such as T_GODATA/T_GOEXDATA) take precedence over
		 * all other events (all read side) or not.
		 */
	}
#endif notyet

	strpeek.ctlbuf.maxlen = sizeof (ctltype);
	strpeek.ctlbuf.len = 0;
	strpeek.ctlbuf.buf = (char *) &ctltype;
	strpeek.databuf.maxlen = 0;
	strpeek.databuf.len = 0;
	strpeek.databuf.buf = NULL;
	strpeek.flags = 0;

	do {
		retval = _ioctl(fd, I_PEEK, &strpeek);
	} while (retval < 0 && errno == EINTR);

	if (retval < 0) {
		sv_errno = errno;
		trace2(TR__t_look_locked, 1, fd);
		errno = sv_errno;
		if (api_semantics == TX_TLI_API) {
			/*
			 * This return of T_ERROR event is ancient
			 * SVR3 TLI semantics and not documented for
			 * current SVR4 TLI interface.
			 * Fixing this will impact some apps
			 * (e.g. nfsd,lockd) in ON consolidation
			 * so they need to be fixed first before TLI
			 * can be fixed.
			 * XXX Should we never fix this because it might
			 * break apps in field ?
			 */
			return (T_ERROR);
		} else {
			/*
			 * XTI semantics (also identical to documented,
			 * but not implemented TLI semantics).
			 */
			t_errno = TSYSERR;
			return (-1);
		}
	}

	/*
	 * if something there and cntl part also there
	 */
	if ((tiptr->ti_lookcnt > 0) ||
	    ((retval > 0) && (strpeek.ctlbuf.len >= (int)sizeof (long)))) {
		pptr = (union T_primitives *)strpeek.ctlbuf.buf;
		if (tiptr->ti_lookcnt > 0) {
			type = *((long *)tiptr->ti_lookbufs.tl_lookcbuf);
			/*
			 * If message on stream head is a T_DISCON_IND, that has
			 * priority over a T_ORDREL_IND in the look buffer.
			 * (This assumes that T_ORDREL_IND can only be in the
			 * first look buffer in the list)
			 */
			if ((type == T_ORDREL_IND) && retval &&
			    (pptr->type == T_DISCON_IND)) {
				type = pptr->type;
				/*
				 * Blow away T_ORDREL_IND
				 */
				_t_free_looklist_head(tiptr);
			}
		} else
			type = pptr->type;

		switch (type) {

		case T_CONN_IND:
			trace2(TR__t_look_locked, 1, fd);
			return (T_LISTEN);

		case T_CONN_CON:
			trace2(TR__t_look_locked, 1, fd);
			return (T_CONNECT);

		case T_DISCON_IND:
			trace2(TR__t_look_locked, 1, fd);
			return (T_DISCONNECT);

		case T_DATA_IND:
		case T_UNITDATA_IND:
			trace2(TR__t_look_locked, 1, fd);
			return (T_DATA);

		case T_EXDATA_IND:
			trace2(TR__t_look_locked, 1, fd);
			return (T_EXDATA);

		case T_UDERROR_IND:
			trace2(TR__t_look_locked, 1, fd);
			return (T_UDERR);

		case T_ORDREL_IND:
			trace2(TR__t_look_locked, 1, fd);
			return (T_ORDREL);

		default:
			t_errno = TSYSERR;
			trace2(TR__t_look_locked, 1, fd);
			errno = EPROTO;
			return (-1);
		}
	}

	/*
	 * if something there put no control part
	 * it must be data
	 */
	if ((retval > 0) && (strpeek.ctlbuf.len <= 0)) {
		trace2(TR__t_look_locked, 1, fd);
		return (T_DATA);
	}

	/*
	 * if msg there and control
	 * part not large enough to determine type?
	 * it must be illegal TLI message
	 */
	if ((retval > 0) && (strpeek.ctlbuf.len > 0)) {
		t_errno = TSYSERR;
		trace2(TR__t_look_locked, 1, fd);
		errno = EPROTO;
		return (-1);
	}
	trace2(TR__t_look_locked, 1, fd);
	return (0);
}
