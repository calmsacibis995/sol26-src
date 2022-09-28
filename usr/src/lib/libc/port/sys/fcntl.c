/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T */
/*	  All Rights Reserved   */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)fcntl.c	1.5	96/08/09 SMI"

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		  All rights reserved.
 *
 */

#ifdef __STDC__
#pragma weak fcntl = _fcntl
#pragma weak _fcntl = _libc_fcntl
#endif
#include	"synonyms.h"

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/filio.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/stropts.h>
#include <sys/socket.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/socketvar.h>
#include <sys/syscall.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

/*
 * XXX these hacks are needed for X.25 which assumes that s_fcntl and
 * s_ioctl exist in the socket library.
 * There is no need for a _s_ioctl for other purposes.
 */
extern int s_fcntl();
extern int _s_fcntl();
#pragma weak s_fcntl = _libc_fcntl
#pragma weak _s_fcntl = _libc_fcntl
#pragma weak s_ioctl = _s_ioctl
int
_s_ioctl(int fd, int cmd, int arg)
{
	return (_ioctl(fd, cmd, arg));
}
/* End XXX */

static int
issock(int fd)
{
	struct stat stats;

#if defined(i386)
	if (_fxstat(_STAT_VER, fd, &stats) == -1)
#else
	if (_fstat(fd, &stats) == -1)
#endif
		return (0);
	return (S_ISSOCK(stats.st_mode));
}


int
_libc_fcntl(int fd, int cmd, int arg)
{
	int		res;

	switch (cmd) {
	case F_SETOWN:
		return (_ioctl(fd, FIOSETOWN, (char *)&arg));

	case F_GETOWN:
		if (_ioctl(fd, FIOGETOWN, (char *)&res) < 0)
			return (-1);
		return (res);

	case F_SETFL:
		if (issock(fd)) {
			int len = sizeof (res);

			if (_so_getsockopt(fd, SOL_SOCKET, SO_STATE,
			    (char *)&res, &len) < 0)
				return (-1);

			if (arg & FASYNC)
				res |= SS_ASYNC;
			else
				res &= ~SS_ASYNC;
			if (_so_setsockopt(fd, SOL_SOCKET, SO_STATE,
			    (char *)&res, sizeof (res)) < 0)
				return (-1);
		}
		return (dosyscall(fd, cmd, arg));

	case F_GETFL: {
		register int flags;

		if ((flags = dosyscall(fd, cmd, arg)) < 0)
			return (-1);

		if (issock(fd)) {
			/*
			 * See if FASYNC is on.
			 */
			int len = sizeof (res);

			if (_so_getsockopt(fd, SOL_SOCKET, SO_STATE,
			    (char *)&res, &len) < 0)
				return (-1);

			if (res & SS_ASYNC)
				flags |= FASYNC;
		}
		return (flags);
	}

	default:
		return (dosyscall(fd, cmd, arg));
	}
}

static int
dosyscall(int fd, int cmd, int arg)
{
	int	retval;

	while ((retval = syscall(SYS_fcntl, fd, cmd, arg)) == ERESTART)
		;

	return (retval);
}
