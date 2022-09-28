/*	Copyright (c) 1996 Sun Microsystems, Inc	*/
/*	All Rights Reserved				*/

#ident	"@(#)weaks.c	1.6	96/08/23 SMI"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/socketvar.h>

#pragma weak bind = _bind
#pragma weak listen = _listen
#pragma weak accept = _accept
#pragma weak connect = _connect
#pragma weak shutdown = _shutdown
#pragma weak recv = _recv
#pragma weak recvfrom = _recvfrom
#pragma weak recvmsg = _recvmsg
#pragma weak send = _send
#pragma weak sendmsg = _sendmsg
#pragma weak sendto = _sendto
#pragma weak getpeername = _getpeername
#pragma weak getsockname = _getsockname
#pragma weak getsockopt = _getsockopt
#pragma weak setsockopt = _setsockopt

int
_bind(sock, addr, addrlen)
	int sock;
	struct sockaddr *addr;
	int addrlen;
{
	return (_so_bind(sock, addr, addrlen, SOV_DEFAULT));
}

int
_listen(sock, backlog)
	int sock, backlog;
{
	return (_so_listen(sock, backlog, SOV_DEFAULT));
}

int
_accept(sock, addr, addrlen)
	int sock;
	struct sockaddr *addr;
	int *addrlen;
{
	return (_so_accept(sock, addr, addrlen, SOV_DEFAULT));
}

int
_connect(sock, addr, addrlen)
	int sock;
	struct sockaddr *addr;
	int addrlen;
{
	return (_so_connect(sock, addr, addrlen, SOV_DEFAULT));
}

int
_shutdown(sock, how)
	int sock, how;
{
	return (_so_shutdown(sock, how, SOV_DEFAULT));
}

int
_recv(sock, buf, len, flags)
	int sock;
	char *buf;
	int len;
	int flags;
{
	return (_so_recv(sock, buf, len, flags & ~MSG_XPG4_2));
}

int
_recvfrom(sock, buf, len, flags, addr, addrlen)
	int sock;
	char *buf;
	int len;
	int flags;
	struct sockaddr *addr;
	int *addrlen;
{
	return (_so_recvfrom(sock, buf, len, flags & ~MSG_XPG4_2,
		addr, addrlen));
}

int
_recvmsg(sock, msg, flags)
	int sock;
	struct msghdr *msg;
	int flags;
{
	return (_so_recvmsg(sock, msg, flags & ~MSG_XPG4_2));
}

int
_send(sock, buf, len, flags)
	int sock;
	char *buf;
	int len;
	int flags;
{
	return (_so_send(sock, buf, len, flags & ~MSG_XPG4_2));
}

int
_sendmsg(sock, msg, flags)
	int sock;
	struct msghdr *msg;
	int flags;
{
	return (_so_sendmsg(sock, msg, flags & ~MSG_XPG4_2));
}

int
_sendto(sock, buf, len, flags, addr, addrlen)
	int sock;
	char *buf;
	int len;
	int flags;
	struct sockaddr *addr;
	int *addrlen;
{
	return (_so_sendto(sock, buf, len, flags & ~MSG_XPG4_2,
		addr, addrlen));
}

int
_getpeername(sock, name, namelen)
	int sock;
	struct sockaddr *name;
	int *namelen;
{
	return (_so_getpeername(sock, name, namelen, SOV_DEFAULT));
}

int
_getsockname(sock, name, namelen)
	int sock;
	struct sockaddr *name;
	int *namelen;
{
	return (_so_getsockname(sock, name, namelen, SOV_DEFAULT));
}

int
_getsockopt(sock, level, optname, optval, optlen)
	int sock, level, optname;
	char *optval;
	int *optlen;
{
	return (_so_getsockopt(sock, level, optname, optval, optlen,
		SOV_DEFAULT));
}

int
_setsockopt(sock, level, optname, optval, optlen)
	int sock, level, optname;
	char *optval;
	int optlen;
{
	return (_so_setsockopt(sock, level, optname, optval, optlen,
		SOV_DEFAULT));
}

int
__xnet_bind(sock, addr, addrlen)
	int sock;
	const struct sockaddr *addr;
	int addrlen;
{
	return (_so_bind(sock, addr, addrlen, SOV_XPG4_2));
}


__xnet_listen(sock, backlog)
	int sock, backlog;
{
	return (_so_listen(sock, backlog, SOV_XPG4_2));
}

int
__xnet_connect(sock, addr, addrlen)
	int sock;
	const struct sockaddr *addr;
	int addrlen;
{
	return (_so_connect(sock, addr, addrlen, SOV_XPG4_2));
}

int
__xnet_recvmsg(sock, msg, flags)
	int sock;
	struct msghdr *msg;
	int flags;
{
	return (_so_recvmsg(sock, msg, flags | MSG_XPG4_2));
}

int
__xnet_sendmsg(sock, msg, flags)
	int sock;
	const struct msghdr *msg;
	int flags;
{
	return (_so_sendmsg(sock, msg, flags | MSG_XPG4_2));
}

int
__xnet_sendto(sock, buf, len, flags, addr, addrlen)
	int sock;
	const char *buf;
	int len;
	int flags;
	const struct sockaddr *addr;
	int *addrlen;
{
	return (_so_sendto(sock, buf, len, flags | MSG_XPG4_2,
		addr, addrlen));
}
