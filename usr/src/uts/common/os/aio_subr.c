/*
 * Copyright (c) 1993-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)aio_subr.c 1.22     96/08/27 SMI"

#include <sys/types.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <vm/as.h>
#include <vm/page.h>
#include <sys/uio.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/aio_impl.h>
#include <sys/epm.h>
#include <sys/fs/snode.h>
#include <sys/siginfo.h>

int aphysio(int (*)(), int (*)(), dev_t, int, void (*)(), struct aio_req *);
void aphysio_cleanup(aio_req_t *);
void aio_cleanup(int);
void aio_copyout_result(aio_req_t *);
void aio_cleanup_exit(void);


/*
 * private functions
 */
static void aio_done(struct buf *);

/*
 * async version of physio() that doesn't wait synchronously
 * for the driver's strategy routine to complete.
 */
int
aphysio(
	int (*strategy)(),
	int (*cancel)(),
	dev_t dev,
	int rw,
	void (*mincnt)(),
	struct aio_req *aio)
{
	struct uio *uio = aio->aio_uio;
	aio_req_t *reqp = (aio_req_t *)aio->aio_private;
	struct buf *bp = &reqp->aio_req_buf;
	struct iovec *iov;
	struct as *as;
	char *a;
	int c, error;
	struct page **pplist;

	/*
	 * Large Files: We do the check against SPEC_MAXOFFSET_T
	 * instead of MAXOFFSET_T because the value represents the
	 * maximum size that can be supported by specfs.
	 */

	if (uio->uio_loffset < 0 || uio->uio_loffset > SPEC_MAXOFFSET_T) {
		return (EINVAL);
	}

	iov = uio->uio_iov;
	sema_init(&bp->b_sem, 0, "bp owner", SEMA_DEFAULT, DEFAULT_WT);
	sema_init(&bp->b_io, 0, "bp io", SEMA_DEFAULT, DEFAULT_WT);

	bp->b_oerror = 0;		/* old error field */
	bp->b_error = 0;
	bp->b_iodone = (int (*)()) aio_done;
	bp->b_flags = B_KERNBUF | B_BUSY | B_PHYS | B_ASYNC | rw;
	bp->b_edev = dev;
	bp->b_dev = cmpdev(dev);
	bp->b_lblkno = btodt(uio->uio_loffset);
	/* b_forw points at an aio_req_t structure */
	bp->b_forw = (struct buf *)reqp;

	a = bp->b_un.b_addr = iov->iov_base;
	c = bp->b_bcount = iov->iov_len;

	(*mincnt)(bp);
	if (bp->b_bcount != iov->iov_len)
		return (ENOTSUP);

	bp->b_proc = curproc;
	as = curproc->p_as;

	error = as_pagelock(as, &pplist, a,
		    (uint)c, rw == B_READ? S_WRITE : S_READ);
	if (error != 0) {
		bp->b_flags |= B_ERROR;
		bp->b_error = error;
		bp->b_flags &= ~(B_BUSY|B_WANTED|B_PHYS|B_SHADOW);
		return (error);
	}
	bp->b_shadow = pplist;
	if (pplist != NULL) {
		bp->b_flags |= B_SHADOW;
	}

	if (cancel != anocancel)
		cmn_err(CE_PANIC,
		    "aphysio: cancellation not supported, use anocancel");

	reqp->aio_req_cancel = cancel;
	/*
	 * we clear dip here if there is nothing to do after completion
	 */
	reqp->aio_req_pm_dip =
	    e_pm_busy(reqp->aio_req_pm_dip, PMC_APHYSIO);
	return ((*strategy)(bp));
}

/*ARGSUSED*/
int
anocancel(struct buf *bp)
{
	return (ENXIO);
}

/*
 * Called from biodone().
 * Notify process that a pending AIO has finished.
 */
static void
aio_done(struct buf *bp)
{
	proc_t *p;
	struct as *as;
	aio_req_t *reqp;
	aio_lio_t *head;
	aio_t *aiop;
	int fd;
	int refcnt;
	int pollqflags;
	void (*func)();

	p = bp->b_proc;
	reqp = (aio_req_t *)bp->b_forw;
	fd = reqp->aio_req_fd;

	/* decrement fd's ref count by one, now that aio request is done. */
	areleasef(fd, p);

	aiop = p->p_aio;
	ASSERT(aiop != NULL);
	reqp->aio_req_next =  NULL;
	if (reqp->aio_req_pm_dip) {
		e_pm_unbusy(reqp->aio_req_pm_dip, PMC_APHYSIO);
		reqp->aio_req_pm_dip = NULL;
	}
	mutex_enter(&aiop->aio_mutex);
	ASSERT(aiop->aio_pending >= 0);
	ASSERT(reqp->aio_req_flags & AIO_PENDING);
	aiop->aio_pending--;
	reqp->aio_req_flags &= ~AIO_PENDING;
	/*
	 * Put the request on the per-process doneq or the
	 * per-process pollq depending on the requests flags.
	 */
	pollqflags = ((aiop->aio_flags & AIO_CLEANUP) |
			    (reqp->aio_req_flags & AIO_POLL));
	if (pollqflags) {
		/* put request on the poll queue. */
		if (pollqflags & AIO_CLEANUP) {
			as = p->p_as;
			mutex_enter(&as->a_contents);
		}
		if (aiop->aio_pollq == NULL) {
			aiop->aio_pollq = reqp;
			reqp->aio_req_next = reqp;
			reqp->aio_req_prev = reqp;
		} else {
			reqp->aio_req_next = aiop->aio_pollq;
			reqp->aio_req_prev = aiop->aio_pollq->aio_req_prev;
			aiop->aio_pollq->aio_req_prev->aio_req_next = reqp;
			aiop->aio_pollq->aio_req_prev = reqp;
		}
		if (pollqflags & AIO_CLEANUP) {
			cv_signal(&aiop->aio_cleanupcv);
			mutex_exit(&as->a_contents);
			mutex_exit(&aiop->aio_mutex);
		} else {
			/*
			 * let the pollq processing happen from an
			 * AST. set an AST on all threads in this process
			 * and wakeup anybody waiting in aiowait().
			 */
			cv_broadcast(&aiop->aio_waitcv);
			mutex_exit(&aiop->aio_mutex);
			mutex_enter(&p->p_lock);
			set_proc_ast(p);
			mutex_exit(&p->p_lock);
		}
	} else {
		/* put request on done queue. */
		if (aiop->aio_doneq == NULL) {
			aiop->aio_doneq = reqp;
			reqp->aio_req_next = reqp;
			reqp->aio_req_prev = reqp;
		} else {
			reqp->aio_req_next = aiop->aio_doneq;
			reqp->aio_req_prev = aiop->aio_doneq->aio_req_prev;
			aiop->aio_doneq->aio_req_prev->aio_req_next = reqp;
			aiop->aio_doneq->aio_req_prev = reqp;
		}
		ASSERT(aiop->aio_pending >= 0);

		/*
		 * If signal handling is enabled for the
		 * request, queue the signal now
		 * Do it before the list head processing as
		 * signals may be enabled for both the list and
		 * each individual request
		 */
		if (reqp->aio_req_sigqp != NULL) {
			mutex_enter(&p->p_lock);
			sigaddqa(p, NULL, reqp->aio_req_sigqp);
			mutex_exit(&p->p_lock);
		}
		/*
		 * when list IO notification is enabled, a signal
		 * is sent only when all entries in the list are
		 * done.
		 */
		if ((head = reqp->aio_req_lio) != NULL) {
			ASSERT(head->lio_refcnt > 0);
			if ((refcnt = --head->lio_refcnt) == 0)
				cv_signal(&head->lio_notify);
			mutex_exit(&aiop->aio_mutex);
			if (!refcnt && head->lio_sigqp) {
				mutex_enter(&p->p_lock);
				sigaddqa(p, NULL, head->lio_sigqp);
				mutex_exit(&p->p_lock);

			}
		} else {
			cv_broadcast(&aiop->aio_waitcv);
			mutex_exit(&aiop->aio_mutex);
			/*
			 * If the I/O request does not have signalling enabled,
			 * send a SIGIO signal if a handler is installed.
			 */
			if ((reqp->aio_req_sigqp == NULL) &&
				(func = p->p_user.u_signal[SIGIO - 1]) !=
				    SIG_DFL && (func != SIG_IGN))
					psignal(p, SIGIO);
		}
	}
}

/*
 * cleanup after aphysio(). aio request was SOFTLOCKed by aphysio()
 * and needs to be SOFTUNLOCKed.
 */
void
aphysio_cleanup(aio_req_t *reqp)
{
	proc_t *p;
	struct as *as;
	struct buf *bp;
	struct iovec *iov;
	int flags;

	if (reqp->aio_req_flags & AIO_DONE)
		return;

	if ((reqp->aio_req_flags & AIO_POLL) == 0)
		aio_copyout_result(reqp);

	p = curproc;
	as = p->p_as;
	bp = &reqp->aio_req_buf;
	iov = reqp->aio_req_uio.uio_iov;
	flags = (((bp->b_flags & B_READ) == B_READ) ? S_WRITE : S_READ);
	as_pageunlock(as, bp->b_shadow, iov->iov_base,
		(uint)iov->iov_len, flags);
	reqp->aio_req_flags |= AIO_DONE;
	bp->b_flags &= ~(B_BUSY|B_WANTED|B_PHYS|B_SHADOW);
	bp->b_flags |= B_DONE;
	if (bp->b_flags & B_REMAPPED)
		bp_mapout(bp);
}

/*
 * deletes a requests id from the hash table of outstanding
 * io.
 */
static void
aio_hash_delete(
	struct aio_req_t *reqp,
	aio_t *aiop)
{
	int index;
	aio_result_t *resultp = reqp->aio_req_resultp;
	aio_req_t *current;
	aio_req_t **nextp;

	index = AIO_HASH(resultp);
	nextp = (aiop->aio_hash + index);
	while ((current = *nextp) != NULL) {
		if (current->aio_req_resultp == resultp) {
			*nextp = current->aio_hash_next;
			return;
		}
		nextp = &current->aio_hash_next;
	}
}

/*
 * Put a list head struct onto its free list.
 */
static void
aio_lio_free(
	aio_lio_t *head,
	aio_t *aiop)
{
	ASSERT(MUTEX_HELD(&aiop->aio_mutex));

	head->lio_next = aiop->aio_lio_free;
	aiop->aio_lio_free = head;
}

/*
 * Put a reqp onto the freelist.
 */
void
aio_req_free(
	aio_req_t *reqp,
	aio_t *aiop)
{
	aio_lio_t *liop;

	ASSERT(MUTEX_HELD(&aiop->aio_mutex));

	if ((liop = reqp->aio_req_lio) != NULL) {
		if (--liop->lio_nent == 0)
			aio_lio_free(liop, aiop);
		reqp->aio_req_lio = NULL;
	}
	reqp->aio_req_next = aiop->aio_free;
	aiop->aio_free = reqp;
	aiop->aio_outstanding--;
	aio_hash_delete(reqp, aiop);
}

/*
 * cleanup aio requests that are on the per-process poll queue.
 */
void
aio_cleanup(int exitflg)
{
	aio_t *aiop = curproc->p_aio;
	aio_req_t *reqp, *next, *pollqhead;
	aio_req_t *doneqtail, *pollqtail;
	void (*func)();

	ASSERT(aiop != NULL);

	if (aiop->aio_pollq == NULL)
		return;

	/*
	 * take all the requests off the poll queue.
	 */
	mutex_enter(&aiop->aio_mutex);
	if ((pollqhead = aiop->aio_pollq) != NULL)
		aiop->aio_pollq = NULL;
	mutex_exit(&aiop->aio_mutex);

	/*
	 * return immediately if poll queue is empty.
	 * someone else must have already emptied it.
	 */
	if (pollqhead == NULL)
		return;

	/*
	 * do physio cleanup for requests on the pollq. if process
	 * is terminating, requests are freed.
	 */
	pollqhead->aio_req_prev->aio_req_next = NULL;
	for (reqp = pollqhead; reqp != NULL; reqp = next) {
		next = reqp->aio_req_next;
		aphysio_cleanup(reqp);
		if (exitflg) {
			/* reqp cann't be referenced after its freed */
			aio_req_free(reqp, aiop);
			continue;
		}
		/* copy out results to user-level result_t */
		if (reqp->aio_req_flags & AIO_POLL)
			aio_copyout_result(reqp);
	}

	/*
	 * if the process is terminating, exitflg is set; the requests
	 * are discarded and are not put on the done queue.
	 */
	if (exitflg)
		return;

	/*
	 * attach pollq to the end of the doneq.
	 */
	mutex_enter(&aiop->aio_mutex);
	if (aiop->aio_doneq == NULL) {
		/*
		 * pollq is not circularly linked. its tail is
		 * NULL terminated from above.
		 */
		pollqhead->aio_req_prev->aio_req_next = pollqhead;
		aiop->aio_doneq = pollqhead;
	} else {
		doneqtail = aiop->aio_doneq->aio_req_prev;
		pollqtail = pollqhead->aio_req_prev;
		/* attach pollq to end of doneq */
		doneqtail->aio_req_next = pollqhead;
		pollqtail->aio_req_next = aiop->aio_doneq;
		pollqhead->aio_req_prev = doneqtail;
		aiop->aio_doneq->aio_req_prev = pollqtail;
	}
	/* wakeup everybody in this process blocked in aiowait() */
	cv_broadcast(&aiop->aio_waitcv);
	mutex_exit(&aiop->aio_mutex);
	/*
	 * Send a SIGIO signal to the process if a signal
	 * handler is installed.
	 */
	if ((func = curproc->p_user.u_signal[SIGIO - 1]) != SIG_DFL &&
	    func != SIG_IGN)
		psignal(curproc, SIGIO);
}

/*
 * called by exit(). waits for all outstanding kaio to finish
 * before the kaio resources are freed.
 */
void
aio_cleanup_exit()
{
	proc_t *p = curproc;
	aio_t *aiop = p->p_aio;
	aio_req_t *reqp, *next, *head;
	aio_lio_t *nxtlio, *liop;

	/*
	 * wait for all outstanding kaio to complete. process
	 * is now single-threaded; no other kaio requests can
	 * happen once aio_pending is zero.
	 */
	mutex_enter(&aiop->aio_mutex);
	aiop->aio_flags |= AIO_CLEANUP;
	while (aiop->aio_pending != 0)
		cv_wait(&aiop->aio_cleanupcv, &aiop->aio_mutex);
	mutex_exit(&aiop->aio_mutex);

	/* cleanup the poll queue. */
	aio_cleanup(1);

	/*
	 * free up the done queue's resources.
	 */
	if ((head = aiop->aio_doneq) != NULL) {
		head->aio_req_prev->aio_req_next = NULL;
		for (reqp = head; reqp != NULL; reqp = next) {
			next = reqp->aio_req_next;
			aphysio_cleanup(reqp);
			kmem_free(reqp, sizeof (struct aio_req_t));
		}
	}
	/*
	 * release aio request freelist.
	 */
	for (reqp = aiop->aio_free; reqp != NULL; reqp = next) {
		next = reqp->aio_req_next;
		kmem_free(reqp, sizeof (struct aio_req_t));
	}

	/*
	 * release io list head freelist.
	 */
	for (liop = aiop->aio_lio_free; liop != NULL; liop = nxtlio) {
		nxtlio = liop->lio_next;
		kmem_free(liop, sizeof (aio_lio_t));
	}

	mutex_destroy(&aiop->aio_mutex);
	kmem_free(p->p_aio, sizeof (struct aio));
}

/*
 * copy out aio request's result to a user-level result_t buffer.
 */
void
aio_copyout_result(aio_req_t *reqp)
{
	struct buf *bp;
	struct iovec *iov;
	aio_result_t *resultp;
	int errno;
	int retval;

	iov = reqp->aio_req_uio.uio_iov;
	bp = &reqp->aio_req_buf;
	/* "resultp" points to user-level result_t buffer */
	resultp = reqp->aio_req_resultp;
	if (bp->b_flags & B_ERROR) {
		if (bp->b_error)
			errno = bp->b_error;
		else
			errno = EIO;
		retval = -1;
	} else {
		errno = 0;
		retval = iov->iov_len - bp->b_resid;
	}
	(void) suword((int *)(&resultp->aio_errno), errno);
	(void) suword((int *)(&resultp->aio_return), retval);
}
