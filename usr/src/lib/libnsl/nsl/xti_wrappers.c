/*	Copyright (c) 1996 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

#pragma ident	"@(#)xti_wrappers.c	1.2	96/10/14 SMI"


#include <xti.h>
#include <unistd.h>
#include <stropts.h>
#include "timt.h"
#include "tx.h"

int
_xti_accept(int fd, int resfd, struct t_call *call)
{
	return (_tx_accept(fd, resfd, call, TX_XTI_API));
}

char *
_xti_alloc(int fd, int struct_type, int fields)
{
	return (_tx_alloc(fd, struct_type, fields, TX_XTI_API));
}

int
_xti_bind(int fd, struct t_bind *req, struct t_bind *ret)
{
	return (_tx_bind(fd, req, ret, TX_XTI_API));
}

int
_xti_close(int fd)
{
	return (_tx_close(fd, TX_XTI_API));
}

int
_xti_connect(int fd, struct t_call *sndcall, struct t_call *rcvcall)
{
	return (_tx_connect(fd, sndcall, rcvcall, TX_XTI_API));
}

/*
 * Note: The TLI version of t_error has return type void. XTI has "int".
 * The spec probably needs to change to void *
 */
int
_xti_error(char *errmsg)
{
	return (_tx_error(errmsg, TX_XTI_API));
}

int
_xti_free(char *ptr, int struct_type)
{
	return (_tx_free(ptr, struct_type, TX_XTI_API));
}

int
_xti_getinfo(int fd, struct t_info *info)
{
	return (_tx_getinfo(fd, info, TX_XTI_API));
}

int
_xti_getprotaddr(int fd, struct t_bind *boundaddr, struct t_bind *peeraddr)
{
	return (_tx_getprotaddr(fd, boundaddr, peeraddr, TX_XTI_API));
}

int
_xti_getstate(int fd)
{
	return (_tx_getstate(fd, TX_XTI_API));
}

int
_xti_listen(int fd, struct t_call *call)
{
	return (_tx_listen(fd, call, TX_XTI_API));
}

int
_xti_look(int fd)
{
	return (_tx_look(fd, TX_XTI_API));
}

int
_xti_open(char *path, int flags, struct t_info *info)
{
	return (_tx_open(path, flags, info, TX_XTI_API));
}

int
_xti_optmgmt(int fd, struct t_optmgmt *req, struct t_optmgmt *ret)
{
	return (_tx_optmgmt(fd, req, ret, TX_XTI_API));
}

int
_xti_rcv(int fd, char *buf, unsigned int nbytes, int *flags)
{
	return (_tx_rcv(fd, buf, nbytes, flags, TX_XTI_API));
}

int
_xti_rcvconnect(int fd, struct t_call *call)
{
	return (_tx_rcvconnect(fd, call, TX_XTI_API));
}

int
_xti_rcvdis(int fd, struct t_discon *discon)
{
	return (_tx_rcvdis(fd, discon, TX_XTI_API));
}

int
_xti_rcvrel(int fd)
{
	return (_tx_rcvrel(fd, TX_XTI_API));
}

int
_xti_rcvudata(int fd, struct t_unitdata *unitdata, int *flags)
{
	return (_tx_rcvudata(fd, unitdata, flags, TX_XTI_API));
}


int
_xti_rcvuderr(int fd, struct t_uderr *uderr)
{
	return (_tx_rcvuderr(fd, uderr, TX_XTI_API));
}

int
_xti_snd(int fd, char *buf, unsigned int nbytes, int flags)
{
	return (_tx_snd(fd, buf, nbytes, flags, TX_XTI_API));
}

int
_xti_snddis(int fd, struct t_call *call)
{
	return (_tx_snddis(fd, call, TX_XTI_API));

}

int
_xti_sndrel(int fd)
{
	return (_tx_sndrel(fd, TX_XTI_API));
}

int
_xti_sndudata(int fd, struct t_unitdata *unitdata)
{
	return (_tx_sndudata(fd, unitdata, TX_XTI_API));
}

char *
_xti_strerror(int errnum)
{
	return (_tx_strerror(errnum, TX_XTI_API));
}


int
_xti_sync(int fd)
{
	return (_tx_sync(fd, TX_XTI_API));
}

int
_xti_unbind(int fd)
{
	return (_tx_unbind(fd, TX_XTI_API));
}
