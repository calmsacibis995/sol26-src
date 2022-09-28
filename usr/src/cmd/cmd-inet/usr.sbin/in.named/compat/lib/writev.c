/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)writev.c	1.1	96/05/09 SMI"

#ifndef LINT
static char rcsid[] = "$Id: writev.c,v 8.1 1994/12/15 06:23:51 vixie Exp $";
#endif

/* writev() emulations contained in this source file for the following systems:
 *
 *	Cray UNICOS
 *	SCO
 */

#if defined(_CRAY)
#define OWN_WRITEV
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/socket.h>

int
__writev(int fd, struct iovec *iov, int iovlen)
{
	struct stat statbuf;

	if (fstat(fd, &statbuf) < 0)
		return (-1);

	/*
	 * Allow for atomic writes to network.
	 */
	if (statbuf.st_mode & S_IFSOCK) {
		struct msghdr   mesg;		

		mesg.msg_name = 0;
		mesg.msg_namelen = 0;
		mesg.msg_iov = iov;
		mesg.msg_iovlen = iovlen;
		mesg.msg_accrights = 0;
		mesg.msg_accrightslen = 0;
		return (sendmsg(fd, &mesg, 0));
	} else {
		register struct iovec *tv;
		register int i, rcode = 0, count = 0;

		for (i = 0, tv = iov; i <= iovlen; tv++) {
			rcode = write(fd, tv->iov_base, tv->iov_len);

			if (rcode < 0)
				break;

			count += rcode;
		}

		if (count == 0)
			return (rcode);
		else
			return (count);
	}
}
#endif

#if defined (M_UNIX) || defined (NEED_WRITEV)
#define OWN_WRITEV
#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>

int
__writev(fd, vp, vpcount)
	int fd;
	const struct iovec *vp;
	register int vpcount;
{
	register int count = 0;

	while (vpcount-- > 0) {
		register int written = write(fd, vp->iov_base, vp->iov_len);

		if (written <= 0)
			return (-1);
		count += written;
		vp++;
	}
	return (count);
}
#endif

#ifndef OWN_WRITEV
int __bindcompat_writev;
#endif
