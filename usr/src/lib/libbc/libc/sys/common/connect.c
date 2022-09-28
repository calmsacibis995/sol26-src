/*
 * Copyright (c) 1990-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)connect.c	1.6	96/05/30 SMI"

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>

/* SVR4 stream operation macros */
#define	STR		('S'<<8)
#define	I_SWROPT	(STR|023)
#define	SNDPIPE		0x002

extern int	errno;

connect(s, name, namelen)
int	s;
struct sockaddr *name;
int	namelen;
{
	int	a;

	if ((a = _connect(s, name, namelen)) == -1)
		maperror();
	return (a);
}
