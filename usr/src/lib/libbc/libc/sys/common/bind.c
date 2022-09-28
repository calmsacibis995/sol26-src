/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/socket.h>

#pragma ident	"@(#)bind.c	1.5	96/09/12 SMI"	/* optional comment */

extern int	errno;

#define	SOV_SOCKBSD	3

bind(s, name, namelen)
int	s;
struct sockaddr *name;
int	namelen;
{
	int	a;
	if ((a = _so_bind(s, name, namelen, SOV_SOCKBSD)) != 0) {
		maperror(errno);
	}
	return (a);
}
