/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T */
/*	  All Rights Reserved   */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)socket.c	1.13	96/08/23 SMI"   /* SVr4.0 1.8	*/

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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/socketvar.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

int _socket_create(int, int, int, int);

#pragma weak socket = _socket

int
_socket(family, type, protocol)
	register int			family;
	register int			type;
	register int			protocol;
{
	return (_socket_create(family, type, protocol, SOV_DEFAULT));
}

/*
 * Used by the BCP library.
 */
int
_socket_bsd(family, type, protocol)
	register int			family;
	register int			type;
	register int			protocol;
{
	return (_socket_create(family, type, protocol, SOV_SOCKBSD));
}

int
_socket_svr4(family, type, protocol)
	register int			family;
	register int			type;
	register int			protocol;
{
	return (_socket_create(family, type, protocol, SOV_SOCKSTREAM));
}

int
_socket_create(family, type, protocol, version)
	register int			family;
	register int			type;
	register int			protocol;
	register int			version;
{
	int fd;

	/*
	 * Try creating without knowing the device assuming that
	 * the transport provider is registered in /etc/sock2path.
	 * If none found fall back to using /etc/netconfig to look
	 * up the name of the transport device name. This provides
	 * backwards compatibility for transport providers that have not
	 * yet been converted to using /etc/sock2path.
	 * XXX When all transport providers use /etc/sock2path this
	 * part of the code can be removed.
	 */
	fd = _so_socket(family, type, protocol, NULL, version);
	if (fd == -1) {
		char *devpath;
		int saved_errno = errno;
		int prototype = 0;

		switch (errno) {
		case EAFNOSUPPORT:
		case EPROTOTYPE:
		case EPROTONOSUPPORT:
			break;

		default:
			return (-1);
		}
		if (_s_netconfig_path(family, type, protocol,
					&devpath, &prototype) == -1) {
			errno = saved_errno;
			return (-1);
		}
		fd = _so_socket(family, type, protocol, devpath, version);
		free(devpath);
		if (fd == -1) {
			errno = saved_errno;
			return (-1);
		}
		if (prototype != 0) {
			if (_setsockopt(fd, SOL_SOCKET, SO_PROTOTYPE,
				(caddr_t)&prototype, sizeof (prototype)) < 0) {
				int sv_errno = errno;
				(void) close(fd);
				/*
				 * setsockopt often fails with ENOPROTOOPT
				 * but socket() should fail with
				 * EPROTONOSUPPORT.
				 */
				errno = EPROTONOSUPPORT;
				return (-1);
			}
		}
	}
	return (fd);
}
