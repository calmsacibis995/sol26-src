/*
 * Copyright (c) 1993, by Sun Microsystems, Inc.
 */

#ifndef _SYS_AIO_IMPL_H
#define	_SYS_AIO_IMPL_H

#pragma ident	"@(#)aio_impl.h	1.11	96/08/22 SMI"

#include <sys/aio_req.h>
#include <sys/aio.h>
#include <sys/aiocb.h>
#include <sys/uio.h>
#include <sys/dditypes.h>
#include <sys/siginfo.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _KERNEL

#define	AIO_HASHSZ		8192		/* power of 2 */
#define	AIO_HASH(cookie)	(((unsigned)(cookie) >> 3) & (AIO_HASHSZ-1))
#define	DUPLICATE 1

/*
 * an aio_list_t is the head of a list. a group of requests are in
 * the same list if their aio_req_list field point to the same list
 * head.
 *
 * a list head is used for notification. a group of requests that
 * should only notify a process when they are done will have a
 * list head. notification is sent when the group of requests are
 * done. individual requests do not send out any notification.
 */
typedef struct aio_lio {
	int 		lio_nent;		/* number of requests in list */
	int 		lio_refcnt;		/* number of requests active */
	struct aio_lio	*lio_next;		/* free list pointer */
	kcondvar_t	lio_notify;		/* list notification */
	sigqueue_t	*lio_sigqp;		/* sigqueue_t pointer */
} aio_lio_t;

/*
 * async I/O request struct - one per I/O request.
 */
typedef struct aio_req_t {
	struct aio_req	aio_req;
	int		aio_req_fd;		/* aio's file descriptor */
	int		aio_req_flags;		/* flags */
	aio_result_t	*aio_req_resultp;	/* pointer to user's results */
	int		(*aio_req_cancel)();	/* driver's cancel cb. */
	struct aio_req_t *aio_req_next;		/* doneq and pollq pointers */
	struct aio_req_t *aio_req_prev;		/* doubly linked list */
	struct aio_req_t *aio_hash_next;	/* next in a hash bucket */
	aio_lio_t 	*aio_req_lio;		/* head of list IO chain */
	struct uio	aio_req_uio;		/* uio struct */
	struct iovec	aio_req_iov;		/* iovec struct */
	struct buf	aio_req_buf;		/* buf struct */
	dev_info_t	*aio_req_pm_dip;	/* dip for tracking pm state */
	sigqueue_t	*aio_req_sigqp;		/* sigqueue_t pointer */
} aio_req_t;

/*
 * Struct for asynchronous I/O (aio) information per process.
 * Each proc stucture has a field pointing to this struct.
 * The field will be null if no aio is used.
 */
typedef struct aio {
	int		aio_pending;		/* # uncompleted requests */
	int		aio_outstanding;	/* total # of requests */
	int		aio_ok;			/* everything ok when set */
	int		aio_flags;		/* flags */
	aio_req_t	*aio_free;  		/* freelist of aio requests */
	aio_lio_t	*aio_lio_free;		/* freelist of lio heads */
	aio_req_t	*aio_doneq;		/* done queue head */
	aio_req_t	*aio_pollq;		/* poll queue head */
	kmutex_t    	aio_mutex;		/* mutex for aio struct */
	kcondvar_t  	aio_waitcv;		/* cv for aiowait()'ers */
	kcondvar_t  	aio_cleanupcv;		/* notify cleanup, aio_done */
	int 		aio_notifycnt;		/* # user-level notifications */
	aio_req_t 	*aio_hash[AIO_HASHSZ];	/* hash list of requests */
} aio_t;

/*
 * aio_flags in aio_req struct
 */
#define	AIO_POLL	0x1			/* AIO_INPROGRESS is set */
#define	AIO_PENDING	0x2			/* aio is in progress */
#define	AIO_DONE	0x4			/* aio req has completed */
#define	AIO_CLEANUP	0x8			/* do aio cleanup processing */

extern int aphysio(int (*)(), int (*)(), dev_t, int, void (*)(),
		struct aio_req *);
extern void aphysio_cleanup(aio_req_t *);
extern void aio_cleanup(int);
void aio_copyout_result(aio_req_t *);
extern void aio_cleanup_exit(void);

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_AIO_IMPL_H */
