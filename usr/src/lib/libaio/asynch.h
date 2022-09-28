/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#ifndef _SYS_ASYNCH_H
#define	_SYS_ASYNCH_H

#pragma ident	"@(#)asynch.h	1.11	96/04/25 SMI"

#include <sys/feature_tests.h>
#include <sys/types.h>
#include <sys/aio.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	AIO_INPROGRESS	-2	/* values not set by the system */

/* large file compilation environment setup */
#if _FILE_OFFSET_BITS == 64
#ifdef __PRAGMA_REDEFINE_EXTNAME
#pragma redefine_extname	aioread		aioread64
#pragma redefine_extname	aiowrite	aiowrite64
#else
#define	aioread			aioread64
#define	aiowrite		aiowrite64
#endif
#endif	/* _FILE_OFFSET_BITS */


int	aioread(int, caddr_t, int, off_t, int, aio_result_t *);
int	aiowrite(int, caddr_t, int, off_t, int, aio_result_t *);
int	aiocancel(aio_result_t *);
aio_result_t *aiowait(struct timeval *);

/* transitional large file interfaces */
#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
int	aioread64(int, caddr_t, int, off64_t, int, aio_result_t *);
int	aiowrite64(int, caddr_t, int, off64_t, int, aio_result_t *);
#endif	/* _LARGEFILE64_SOURCE... */

#define	MAXASYNCHIO 200		/* maxi.number of outstanding i/o's */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_ASYNCH_H */
