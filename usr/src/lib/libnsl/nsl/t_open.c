/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1993-96 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#ident	"@(#)t_open.c	1.23	96/10/14 SMI"	/* SVr4.0 1.5.3.3	*/

#include <fcntl.h>
#include <rpc/trace.h>
#include <unistd.h>
#include <errno.h>
#include <stropts.h>
#include <sys/timod.h>
#include <xti.h>
#include <signal.h>
#include <syslog.h>
#include "timt.h"
#include "tx.h"

int
_tx_open(path, flags, info, api_semantics)
char *path;
int flags;
register struct t_info *info;
int api_semantics;
{
	int retval, fd, sv_errno;
	register struct _ti_user *tiptr;
	sigset_t mask;

	trace2(TR_t_open, 0, flags);
	if (!(flags & O_RDWR)) {
		t_errno = TBADFLAG;
		trace2(TR_t_open, 1, flags);
		return (-1);
	}

	if ((fd = open(path, flags)) < 0) {
		sv_errno = errno;

		trace2(TR_t_open, 1, flags);
		errno = sv_errno;
		t_errno = TSYSERR;
		if (api_semantics == TX_XTI_API && errno == ENOENT)
			/* XTI only */
			t_errno = TBADNAME;
		return (-1);
	}
	/*
	 * is module already pushed
	 */
	do {
		retval = _ioctl(fd, I_FIND, "timod");
	} while (retval < 0 && errno == EINTR);

	if (retval < 0) {
		sv_errno = errno;

		t_errno = TSYSERR;
		(void) close(fd);
		trace2(TR_t_open, 1, flags);
		errno = sv_errno;
		return (-1);
	}

	if (retval == 0) {
		/*
		 * "timod" not already on stream, then push it
		 */
		do {
			/*
			 * Assumes (correctly) that I_PUSH  is
			 * atomic w.r.t signals (EINTR error)
			 */
			retval = _ioctl(fd, I_PUSH, "timod");
		} while (retval < 0 && errno == EINTR);

		if (retval < 0) {
			int sv_errno = errno;

			t_errno = TSYSERR;
			(void) close(fd);
			trace2(TR_t_open, 1, flags);
			errno = sv_errno;
			return (-1);
		}
	}

	MUTEX_LOCK_PROCMASK(&_ti_userlock, mask);
	tiptr = _t_create(fd, info, api_semantics);
	if (tiptr == NULL) {
		int sv_errno = errno;
		(void) close(fd);
		MUTEX_UNLOCK_PROCMASK(&_ti_userlock, mask);
		errno = sv_errno;
		return (-1);
	}

	/*
	 * _t_create synchronizes state witk kernel timod and
	 * already sets it to T_UNBND - what it needs to be
	 * be on T_OPEN event. No _T_TX_NEXTSTATE needed here.
	 */
	MUTEX_UNLOCK_PROCMASK(&_ti_userlock, mask);

	do {
		retval = _ioctl(fd, I_FLUSH, FLUSHRW);
	} while (retval < 0 && errno == EINTR);

	/*
	 * We ignore other error cases (retval < 0) - assumption is
	 * that I_FLUSH failures is temporary (e.g. ENOSR) or
	 * otherwise benign failure on a this newly opened file
	 * descriptor and not a critical failure.
	 */

	trace2(TR_t_open, 1, flags);
	return (fd);
}
