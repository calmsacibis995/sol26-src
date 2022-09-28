/*	Copyright (c) 1993, by Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _AIO_H
#define	_AIO_H

#pragma ident	"@(#)aio.h	1.14	96/08/21 SMI"

#include <sys/feature_tests.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/siginfo.h>
#include <sys/file.h>
#include <sys/aiocb.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if	(_POSIX_C_SOURCE - 0 > 0) && (_POSIX_C_SOURCE - 0 <= 2)
#error	"POSIX Asynchronous I/O is not supported in POSIX.1-1990"
#endif

/*
 * function prototypes
 */
#if	defined(__STDC__)
#include <sys/time.h>
extern int	aio_read(aiocb_t *aiocb_tp);
extern int	aio_write(aiocb_t *aiocb_tp);
extern int	lio_listio(int mode, aiocb_t * const list[], int nent,
			struct sigevent *sig);
extern int	aio_error(const aiocb_t *aiocb_tp);
extern int	aio_return(aiocb_t *aiocb_tp);
extern int	aio_cancel(int fildes, aiocb_t *aiocb_tp);
extern int	aio_suspend(const aiocb_t * const list[], int nent,
			const struct timespec *timeout);
extern int	aio_fsync(int op, aiocb_t *aiocb_tp);

#ifdef _LARGEFILE64_SOURCE
extern int	aio_read64(aiocb64_t *aiocb_tp);
extern int	aio_write64(aiocb64_t *aiocb_tp);
extern int	lio_listio64(int mode, aiocb64_t * const list[], int nent,
			struct sigevent *sig);
extern int	aio_error64(const aiocb64_t *aiocb_tp);
extern int	aio_return64(aiocb64_t *aiocb_tp);
extern int	aio_cancel64(int fildes, aiocb64_t *aiocb_tp);
extern int	aio_suspend64(const aiocb64_t * const list[], int nent,
			const struct timespec *timeout);
extern int	aio_fsync64(int op, aiocb64_t *aiocb_tp);
#endif

#else
extern int	aio_read();
extern int	aio_write();
extern int	lio_listio();
extern int	aio_error();
extern int	aio_return();
extern int	aio_cancel();
extern int	aio_suspend();
extern int	aio_fsync();

#ifdef _LARGEFILE64_SOURCE
extern int	aio_read64();
extern int	aio_write64();
extern int	lio_listio64();
extern int	aio_error64();
extern int	aio_return64();
extern int	aio_cancel64();
extern int	aio_suspend64();
extern int	aio_fsync64();
#endif

#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _AIO_H */
