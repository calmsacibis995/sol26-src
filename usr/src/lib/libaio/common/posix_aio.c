#pragma ident	"@(#)posix_aio.c	1.3	96/08/26 SMI"
/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 *
 * posix_aio.c implements the POSIX async. I/O
 * functions for libposix4
 *
 *	aio_read
 *	aio_write
 *	aio_error
 *	aio_return
 *	aio_suspend
 *	lio_listio
 *
 * the following are not supported yet but are kept here
 * for completeness
 *
 *	aio_fsync
 *	aio_cancel
 *
 * (also, the 64-bit versions for _LARGEFILE64_SOURCE)
 */

#include	<aio.h>
#include	<stdio.h>
#include	<errno.h>
#include	<libaio.h>
#include	<sys/time.h>
#include	<sys/lwp.h>
#include	<signal.h>

extern aio_req_t *_aio_hash_find(aio_result_t *);

/*
 * List I/O list head stuff
 */
static aio_lio_t *_lio_head_freelist = NULL;
static int _aio_lio_alloc(aio_lio_t **);
static int _aio_lio_delete(aio_lio_t *);

static int _aio_remove(aiocb_t *, aio_req_t *);

#ifdef _LARGEFILE64_SOURCE
static int _aio_remove64(aiocb64_t *, aio_req_t *);
#endif


int
__aio_read(aiocb_t *cb)
{
	aio_lio_t	*head = NULL;

	if (cb == NULL) {
		errno = EINVAL;
		return (-1);
	}

	cb->aio_lio_opcode = LIO_READ;
	return (_aio_rw(cb, AIO_KAIO, head));
}

int
__aio_write(aiocb_t *cb)
{
	aio_lio_t	*head = NULL;

	if (cb == NULL) {
		errno = EINVAL;
		return (-1);
	}
	cb->aio_lio_opcode = LIO_WRITE;
	return (_aio_rw(cb, AIO_KAIO, head));
}


int
__lio_listio(int mode, aiocb_t * const list[],
		int nent, struct sigevent *sig)
{
	int 		i, err;
	struct 		timeval tv;
	int 		aio_ufs = 0;
	int 		oerrno = 0;
	aio_lio_t	*head = NULL;
	aio_req_t	*reqp;
	int		state = 0;

	if (!_kaio_ok)
		_kaio_init();

	if (mode == LIO_WAIT)
		state = NOCHECK;
	else
		state = CHECK;

	for (i = 0; i < nent; i++) {
		if (list[i]) {
			if (list[i]->aio_lio_opcode != LIO_NOP) {
				list[i]->aio_resultp.aio_errno = EINPROGRESS;
				list[i]->aio_state = state;
			} else {
				list[i]->aio_state = NOCHECK;
			}
		}
	}

	if ((err = kaio(AIOLIO, mode, list, nent, sig)) == 0)
			return (0);
	oerrno = errno;
	if ((err == -1) && (errno == ENOTSUP)) {
		err = errno = 0;
		/*
		 * If LIO_WAIT, or signal required, allocate a list head.
		 */
		if ((mode == LIO_WAIT) || ((sig) && (sig->sigev_signo > 0)))
			_aio_lio_alloc(&head);
		if (head) {
			_lwp_mutex_lock(&head->lio_mutex);
			head->lio_mode = mode;
			if ((mode == LIO_NOWAIT) && (sig) &&
					(sig->sigev_notify != SIGEV_NONE) &&
					(sig->sigev_signo > 0)) {
				head->lio_signo = sig->sigev_signo;
				head->lio_sigval.sival_int =
					sig->sigev_value.sival_int;
			} else
				head->lio_signo = 0;
			head->lio_nent = head->lio_refcnt = nent;
			_lwp_mutex_unlock(&head->lio_mutex);
		}
		/*
		 * find UFS requests, errno == ENOTSUP,
		 */
		for (i = 0; i < nent; i++) {
			if (list[i] &&
				list[i]->aio_resultp.aio_errno == ENOTSUP) {
				if (list[i]->aio_lio_opcode == LIO_NOP) {
					if (head) {
						_lwp_mutex_lock(
							&head->lio_mutex);
						head->lio_nent--;
						head->lio_refcnt--;
						_lwp_mutex_unlock(
							&head->lio_mutex);
					}
					continue;
				}
				/*
				 * submit an AIO request with flags AIO_NO_KAIO
				 * to avoid the kaio() syscall in _aio_rw()
				 */
				err = _aio_rw(list[i], AIO_NO_KAIO, head);
				if (err != 0) {
					if (head) {
						_lwp_mutex_lock(
							&head->lio_mutex);
						head->lio_nent--;
						head->lio_refcnt--;
						_lwp_mutex_unlock(
							&head->lio_mutex);
					}
					list[i]->aio_resultp.aio_errno = EIO;
					errno = EIO;
					return (-1);
				} else
					aio_ufs++;

			} else {
				if (head) {
					_lwp_mutex_lock(&head->lio_mutex);
					head->lio_nent--;
					head->lio_refcnt--;
					_lwp_mutex_unlock(&head->lio_mutex);
				}
				continue;
			}
		}
	}
	if ((mode == LIO_WAIT) && ((oerrno == ENOTSUP) ||
		(oerrno == EINTR))) {
		/*
		 * call kaio(AIOWAIT) to get all outstanding
		 * kernel AIO requests
		 */
		if ((nent - aio_ufs) > 0) {
			while (kaio(AIOLIOWAIT, mode, list, nent, sig) == -1) {
				if (errno == EINTR)
					continue;
				break;
			}
		}
		if (head && head->lio_nent > 0) {
			_lwp_mutex_lock(&head->lio_mutex);
			while (head->lio_refcnt > 0) {
				errno = _lwp_cond_wait(&head->lio_cond_cv,
						&head->lio_mutex);
				if (errno && errno != EINTR) {
					_lwp_mutex_unlock(&head->lio_mutex);
					return (-1);
				}
			}
			_lwp_mutex_unlock(&head->lio_mutex);
		}
		return (0);
	}
	return (err);
}

int
__aio_suspend(aiocb_t * list[], int nent, const timespec_t *timo)
{
	struct aio_req	*reqp;
	aio_lio_t	*head = NULL;
	int		oerrno, err, i, aio_done, aio_outstanding;
	int		tot_outstanding = 0;
	struct timeval	curtime, end, wait;
	int		timedwait = 0, polledwait = 0;

	for (i = 0; i < nent; i++) {
		if (list[i] && list[i]->aio_state == CHECK)
			list[i]->aio_state = CHECKED;
	}

	if ((err = kaio(AIOSUSPEND, list, nent, timo, -1)) == 0)
		return (0);

	oerrno = errno;

	if (timo) {
		if ((timo->tv_sec > 0) || (timo->tv_nsec > 0)) {
			gettimeofday(&curtime, NULL);
			end.tv_sec = timo->tv_sec + curtime.tv_sec;
			end.tv_usec = (timo->tv_nsec  / 1000000)
					+ curtime.tv_usec;
			timedwait++;
		} else
			polledwait++;
	}

	aio_done = aio_outstanding = 0;

	while (1) {
		for (i = 0; i < nent; i++) {
			if (list[i] == NULL || list[i]->aio_state != USERAIO)
				continue;
			if (list[i]->aio_resultp.aio_errno == EINPROGRESS)
				aio_outstanding++;
			else
				aio_done++;
		}
		/*
		 * got some I/O's
		 */
		if (aio_done) {
			errno = 0;
			return (0);
		}
		/*
		 * No UFS I/O outstanding, return
		 * kaio(AIOSUSPEND) error status
		 */
		if (aio_outstanding == 0) {
			errno = oerrno;
			return (err);
		}
		if (polledwait) {
			errno = EAGAIN;
			return (-1);
		} else if (timedwait) {
			gettimeofday(&curtime, NULL);
			wait.tv_sec = end.tv_sec - curtime.tv_sec;
			wait.tv_usec = end.tv_usec - curtime.tv_usec;
			if (wait.tv_sec < 0 || (wait.tv_sec == 0 &&
				wait.tv_usec <= 0)) {
				errno = EAGAIN;
				return (-1);
			}
		}

	}
}

int
__aio_error(aiocb_t *cb)
{
	aio_req_t	*reqp;

	if (cb->aio_resultp.aio_errno == EINPROGRESS) {
		if (cb->aio_state == CHECK) {
			if ((kaio(AIOERROR, cb)) == EINVAL) {
				errno = EINVAL;
				return (-1);
			}
		} else if (cb->aio_state == CHECKED)
			cb->aio_state =  CHECK;
		return (cb->aio_resultp.aio_errno);
	}
	if (cb->aio_state == USERAIO) {
		if ((reqp = _aio_hash_find(&cb->aio_resultp)) == NULL) {
			errno = EINVAL;
			return (-1);
		}
		_aio_remove(cb, reqp);
	}
	return (cb->aio_resultp.aio_errno);
}

int
__aio_return(aiocb_t *cb)
{
	/*
	 * graceful detection of an invalid cb is not possible. a
	 * SIGSEGV will be generated if it is invalid.
	 */
	if (cb == NULL) {
		errno = EINVAL;
		exit(-1);
	}
	return (cb->aio_resultp.aio_return);
}
static int
_aio_remove(aiocb_t *cb, aio_req_t *reqp)
{
	aio_lio_t	*head;
	int		last = 0, remflg = 0;

	_aio_lock();
	cb->aio_state = NOCHECK;
	head = reqp->lio_head;
	if (head) {
		_lwp_mutex_lock(&head->lio_mutex);
		if (--head->lio_nent == 0)
			last = 1;
		_lwp_mutex_unlock(&head->lio_mutex);
		if (last)
			_aio_lio_delete(head);
	}
	_aio_hash_del(reqp->req_resultp);
	_aio_req_free(reqp);
	_aio_unlock();
}
int
_aio_lio_alloc(head)
aio_lio_t **head;
{
	aio_lio_t	*lio_head;

	_lwp_mutex_lock(&__lio_mutex);
	if (_lio_head_freelist == NULL) {
		lio_head = (aio_lio_t *)malloc(sizeof (aio_lio_t));
	} else {
		lio_head = _lio_head_freelist;
		_lio_head_freelist = lio_head->lio_next;
	}
	if (lio_head == NULL) {
		_lwp_mutex_unlock(&__lio_mutex);
		return (-1);
	}
	lio_head->lio_mode = 0;
	lio_head->lio_nent = 0;
	lio_head->lio_refcnt = 0;
	lio_head->lio_next = NULL;
	lio_head->lio_signo = 0;
	memset(&lio_head->lio_cond_cv, 0, sizeof (lwp_cond_t));
	memset(&lio_head->lio_mutex, 0, sizeof (mutex_t));
	*head = lio_head;
	_lwp_mutex_unlock(&__lio_mutex);
	return (0);
}
int
_aio_lio_delete(head)
aio_lio_t *head;
{
	aio_lio_t	*lio_head = head;

	_lwp_mutex_lock(&__lio_mutex);
	if (_lio_head_freelist == NULL) {
		_lio_head_freelist = lio_head;
	} else {
		_lio_head_freelist->lio_next  = lio_head;
	}
	_lwp_mutex_unlock(&__lio_mutex);
}
int
__aio_fsync(int op, aiocb_t *aiocbp)
{
	errno = ENOSYS;
	return (-1);
}

int
__aio_cancel(int fd, aiocb_t *aiocbp)
{
	errno = ENOSYS;
	return (-1);
}

#ifdef _LARGEFILE64_SOURCE

int
__aio_read64(aiocb64_t *cb)
{
	aio_lio_t	*head = NULL;

	if (cb == NULL) {
		errno = EINVAL;
		return (-1);
	}

	cb->aio_lio_opcode = LIO_READ;
	return (_aio_rw64(cb, AIO_KAIO, head));
}

int
__aio_write64(aiocb64_t *cb)
{
	aio_lio_t	*head = NULL;

	if (cb == NULL) {
		errno = EINVAL;
		return (-1);
	}
	cb->aio_lio_opcode = LIO_WRITE;
	return (_aio_rw64(cb, AIO_KAIO, head));
}


int
__lio_listio64(int mode, aiocb64_t * const list[],
		int nent, struct sigevent *sig)
{
	int 		i, err;
	struct 		timeval tv;
	int 		aio_ufs = 0;
	int 		oerrno = 0;
	aio_lio_t	*head = NULL;
	aio_req_t	*reqp;
	int		state = 0;

	if (!_kaio_ok)
		_kaio_init();

	if (mode == LIO_WAIT)
		state = NOCHECK;
	else
		state = CHECK;

	for (i = 0; i < nent; i++) {
		if (list[i]) {
			if (list[i]->aio_lio_opcode != LIO_NOP) {
				list[i]->aio_resultp.aio_errno = EINPROGRESS;
				list[i]->aio_state = state;
			} else {
				list[i]->aio_state = NOCHECK;
			}
		}
	}

	if ((err = kaio(AIOLIO64, mode, list, nent, sig)) == 0)
			return (0);
	oerrno = errno;
	if ((err == -1) && (errno == ENOTSUP)) {
		err = errno = 0;
		/*
		 * If LIO_WAIT, or signal required, allocate a list head.
		 */
		if ((mode == LIO_WAIT) || ((sig) && (sig->sigev_signo > 0)))
			_aio_lio_alloc(&head);
		if (head) {
			_lwp_mutex_lock(&head->lio_mutex);
			head->lio_mode = mode;
			if ((mode == LIO_NOWAIT) && (sig) &&
					(sig->sigev_notify != SIGEV_NONE) &&
					(sig->sigev_signo > 0)) {
				head->lio_signo = sig->sigev_signo;
				head->lio_sigval.sival_int =
					sig->sigev_value.sival_int;
			} else
				head->lio_signo = 0;
			head->lio_nent = head->lio_refcnt = nent;
			_lwp_mutex_unlock(&head->lio_mutex);
		}
		/*
		 * find UFS requests, errno == ENOTSUP,
		 */
		for (i = 0; i < nent; i++) {
			if (list[i] &&
				list[i]->aio_resultp.aio_errno == ENOTSUP) {
				if (list[i]->aio_lio_opcode == LIO_NOP) {
					if (head) {
						_lwp_mutex_lock(
							&head->lio_mutex);
						head->lio_nent--;
						head->lio_refcnt--;
						_lwp_mutex_unlock(
							&head->lio_mutex);
					}
					continue;
				}
				/*
				 * submit an AIO request with flags AIO_NO_KAIO
				 * to avoid the kaio() syscall in _aio_rw()
				 */
				err = _aio_rw64(list[i], AIO_NO_KAIO, head);
				if (err != 0) {
					if (head) {
						_lwp_mutex_lock(
							&head->lio_mutex);
						head->lio_nent--;
						head->lio_refcnt--;
						_lwp_mutex_unlock(
							&head->lio_mutex);
					}
					list[i]->aio_resultp.aio_errno = EIO;
					errno = EIO;
					return (-1);
				} else
					aio_ufs++;

			} else {
				if (head) {
					_lwp_mutex_lock(&head->lio_mutex);
					head->lio_nent--;
					head->lio_refcnt--;
					_lwp_mutex_unlock(&head->lio_mutex);
				}
				continue;
			}
		}
	}
	if ((mode == LIO_WAIT) && ((oerrno == ENOTSUP) ||
		(oerrno == EINTR))) {
		/*
		 * call kaio(AIOWAIT) to get all outstanding
		 * kernel AIO requests
		 */
		if ((nent - aio_ufs) > 0) {
			while (kaio(AIOLIOWAIT64, mode,
					list, nent, sig) == -1) {
				if (errno == EINTR)
					continue;
				break;
			}
		}
		if (head && head->lio_nent > 0) {
			_lwp_mutex_lock(&head->lio_mutex);
			while (head->lio_refcnt > 0) {
				errno = _lwp_cond_wait(&head->lio_cond_cv,
						&head->lio_mutex);
				if (errno && errno != EINTR) {
					_lwp_mutex_unlock(&head->lio_mutex);
					return (-1);
				}
			}
			_lwp_mutex_unlock(&head->lio_mutex);
		}
		return (0);
	}
	return (err);
}


int
__aio_suspend64(aiocb64_t * list[], int nent, const timespec_t *timo)
{
	struct aio_req	*reqp;
	aio_lio_t	*head = NULL;
	int		oerrno, err, i, aio_done, aio_outstanding;
	int		tot_outstanding = 0;
	struct timeval	curtime, end, wait;
	int		timedwait = 0, polledwait = 0;

	for (i = 0; i < nent; i++) {
		if (list[i] && list[i]->aio_state == CHECK)
			list[i]->aio_state = CHECKED;
	}

	if ((err = kaio(AIOSUSPEND64, list, nent, timo, -1)) == 0)
		return (0);

	oerrno = errno;

	if (timo) {
		if ((timo->tv_sec > 0) || (timo->tv_nsec > 0)) {
			gettimeofday(&curtime, NULL);
			end.tv_sec = timo->tv_sec + curtime.tv_sec;
			end.tv_usec = (timo->tv_nsec  / 1000000)
					+ curtime.tv_usec;
			timedwait++;
		} else
			polledwait++;
	}

	aio_done = aio_outstanding = 0;

	while (1) {
		for (i = 0; i < nent; i++) {
			if (list[i] == NULL || list[i]->aio_state != USERAIO)
				continue;
			if (list[i]->aio_resultp.aio_errno == EINPROGRESS)
				aio_outstanding++;
			else
				aio_done++;
		}
		/*
		 * got some I/O's
		 */
		if (aio_done) {
			errno = 0;
			return (0);
		}
		/*
		 * No UFS I/O outstanding, return
		 * kaio(AIOSUSPEND) error status
		 */
		if (aio_outstanding == 0) {
			errno = oerrno;
			return (err);
		}
		if (polledwait) {
			errno = EAGAIN;
			return (-1);
		} else if (timedwait) {
			gettimeofday(&curtime, NULL);
			wait.tv_sec = end.tv_sec - curtime.tv_sec;
			wait.tv_usec = end.tv_usec - curtime.tv_usec;
			if (wait.tv_sec < 0 || (wait.tv_sec == 0 &&
				wait.tv_usec <= 0)) {
				errno = EAGAIN;
				return (-1);
			}
		}

	}
}

int
__aio_error64(aiocb64_t *cb)
{
	aio_req_t	*reqp;

	if (cb->aio_resultp.aio_errno == EINPROGRESS) {
		if (cb->aio_state == CHECK) {
			if ((kaio(AIOERROR64, cb)) == EINVAL) {
				errno = EINVAL;
				return (-1);
			}
		} else if (cb->aio_state == CHECKED)
			cb->aio_state =  CHECK;
		return (cb->aio_resultp.aio_errno);
	}
	if (cb->aio_state == USERAIO) {
		if ((reqp = _aio_hash_find(&cb->aio_resultp)) == NULL) {
			errno = EINVAL;
			return (-1);
		}
		_aio_remove64(cb, reqp);
	}
	return (cb->aio_resultp.aio_errno);
}

int
__aio_return64(aiocb64_t *cb)
{
	/*
	 * graceful detection of an invalid cb is not possible. a
	 * SIGSEGV will be generated if it is invalid.
	 */
	if (cb == NULL) {
		errno = EINVAL;
		exit(-1);
	}
	return (cb->aio_resultp.aio_return);
}
static int
_aio_remove64(aiocb64_t *cb, aio_req_t *reqp)
{
	aio_lio_t	*head;
	int		last = 0, remflg = 0;

	_aio_lock();
	head = reqp->lio_head;
	if (head) {
		_lwp_mutex_lock(&head->lio_mutex);
		if (--head->lio_nent == 0)
			last = 1;
		cb->aio_state = NOCHECK;
		_lwp_mutex_unlock(&head->lio_mutex);
		if (last)
			_aio_lio_delete(head);
	}
	_aio_hash_del(reqp->req_resultp);
	_aio_req_free(reqp);
	_aio_unlock();
}
int
__aio_fsync64(int op, aiocb64_t *aiocbp)
{
	errno = ENOSYS;
	return (-1);
}

int
__aio_cancel64(int fd, aiocb64_t *aiocbp)
{
	errno = ENOSYS;
	return (-1);
}

#endif
