/*
 * Copyright (c) 1990-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)socketpair.c	1.7	96/08/19 SMI"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>

extern int	errno;

int
socketpair(family, type, protocol, sv)
register int	family;
register int	type;
register int	protocol;
register int	sv[2];
{
	int	ret;
	static int map[] = {0, 2, 1, 4, 5, 6};
	if ((ret = _socketpair_bsd(family, map[type], protocol,
					sv)) == -1) {
		maperror(errno);
		switch (errno) {
		case EAFNOSUPPORT:
		case EPROTOTYPE:
			errno = EPROTONOSUPPORT;
			break;
		}
	}
	return (ret);
}
