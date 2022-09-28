/*
 * Copyright (c) 1990-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)socket.c	1.6	96/08/19 SMI"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>

extern int	errno;

int
socket(family, type, protocol)
register int	family;
register int	type;
register int	protocol;
{
	int	a;
	static int map[]={0,2,1,4,5,6};
	if ((a = _socket_bsd(family, map[type], protocol)) == -1) {
		maperror(errno);
		switch (errno) {
		case EAFNOSUPPORT:
		case EPROTOTYPE:
			errno = EPROTONOSUPPORT;
			break;
		}
	}
	return(a);
}


