/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#ifndef _PRDEBUG_H
#define _PRDEBUG_H

#pragma ident   "@(#)prdebug.h 1.2     96/04/01 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#define	NETPRDFILE	"/tmp/netpr.debug"

#define NETDB(fd, msg)	\
	{	\
		fprintf(fd,	\
		"%s: line:%d  %s\n", __FILE__,  __LINE__, msg);	\
		fflush(fd); \
		fsync(fd); \
	}


#define	_SZ_BUFF	100

#endif /* _PRDEBUG_H */
