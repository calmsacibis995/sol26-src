/*
 * Copyright (c) 1990-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)send.c	1.3	96/05/30 SMI"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>

extern int	errno;

#define  N_AGAIN	11

int	send(s, msg, len, flags)
int	s;
char	*msg;
int	len, flags;
{
	int	a;
	if ((a = _send(s, msg, len, flags)) == -1) {
		if (errno == N_AGAIN)
			errno = EWOULDBLOCK;
		else
			maperror(errno);
	}
	return(a);
}


int	sendto(s, msg, len, flags, to, tolen)
int	s;
char	*msg;
int	len, flags;
struct sockaddr *to;
int	tolen;
{
	int	a;
	if ((a = _sendto(s, msg, len, flags, to, tolen)) == -1) {
		if (errno == N_AGAIN)
			errno = EWOULDBLOCK;
		else
			maperror(errno);
	}
	return(a);
}


int	sendmsg(s, msg, flags)
int	s;
struct msghdr *msg;
int	flags;
{
	int	a;
	if ((a = _sendmsg(s, msg, flags)) == -1) {
		if (errno == N_AGAIN)
			errno = EWOULDBLOCK;
		else
			maperror(errno);
	}
	return(a);
}


