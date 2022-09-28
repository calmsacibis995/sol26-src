/*
 * Copyright (c) 1993, by Sun Microsystems, Inc.
 */

#pragma ident   "@(#)aio.c 1.25     96/10/23 SMI"

/*
 * Kernel asynchronous I/O.
 * This is only for raw devices now (as of Nov. 1993).
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/fs/snode.h>
#include <sys/unistd.h>
#include <sys/cmn_err.h>
#include <vm/as.h>
#include <vm/faultcode.h>
#include <sys/sysmacros.h>
#include <sys/procfs.h>
#include <sys/kmem.h>
#include <sys/autoconf.h>
#include <sys/ddi_impldefs.h>
#include <sys/sunddi.h>
#include <sys/aio_impl.h>
#include <sys/debug.h>
#include <sys/param.h>

typedef struct kaio_args {
	int	opcode;			/* kaio call type */
} kaio_args_t;

typedef struct arw_args {
	int 	opcode;			/* kaio call type */
	int 	fdes;			/* file descriptor */
	char 	*bufp;			/* buffer pointer */
	int 	bufsize;		/* buffer size */
	offset_t offset;		/* offset */
	aio_result_t *resultp; 	/* pointer to user's resultp */
} arw_args_t;

typedef struct aiorw_args {
	int 	opcode;			/* kaio call type */
	aiocb_t *aiocb;
} aiorw_args_t;

typedef struct alio_args {
	int opcode;
	int mode;			/* LIO_WAIT or LIO_NOWAIT */
	aiocb_t **aiocb;		/* pointer to a list of aiocb's */
	int nent;			/* number of entries in list */
	struct sigevent *sigev;		/* signal notification when done */
} alio_args_t;

typedef struct aiowait_args {
	int 		opcode;		/* kaio call type */
	struct timeval	*timeout;	/* timeout struct */
	int 		flag;
} aiowait_args_t;

typedef struct aiosuspend_args {
	int 		opcode;		/* kaio call type */
	aiocb_t		**aiocb;
	int		nent;
	struct timespec	*timeout;
	int		flag;
} aiosuspend_args_t;

typedef struct aioerror_args {
	int 		opcode;		/* kaio call type */
	aiocb_t 	*cb;
} aioerror_args_t;

#ifdef _LARGEFILE64_SOURCE

typedef struct aiorw_args64 {
	int 		opcode;		/* kaio call type */
	aiocb64_t 	*aiocb;
} aiorw_args64_t;

typedef struct alio64_args {
	int 		opcode;		/* kaio call type */
	int 		mode;		/* LIO_WAIT or LIO_NOWAIT */
	aiocb64_t 	**aiocb;	/* pointer to a list of aiocb's */
	int		nent;		/* number of entries in list */
	struct sigevent *sigev;		/* signal notification when done */
} alio64_args_t;

typedef struct aiosuspend64_args {
	int 		opcode;		/* kaio call type */
	aiocb64_t	**aiocb;
	int		nent;
	struct timespec	*timeout;
	int		flag;
} aiosuspend64_args_t;

typedef struct aioerror64_args {
	int 		opcode;		/* kaio call type */
	aiocb64_t 	*cb;
} aioerror64_args_t;

#endif

extern	int freemem;
extern	int desfree;

/*
 * external entry point.
 */
static int kaio(kaio_args_t *, rval_t *);

/*
 * implementation specific functions (private)
 */
static int arw(arw_args_t *, int);
static int aiorw(aiorw_args_t *, int);
static int alio(alio_args_t *);
static int aliowait(alio_args_t *);
static int aiowait(aiowait_args_t *, rval_t *);
static int aiosuspend(aiosuspend_args_t *, rval_t *);
static int aioerror(aioerror_args_t *);
static int aionotify(void);
static int aioinit(void);
static int aiostart(void);
static void alio_cleanup(aio_t *, aiocb_t **, int);
static int (*check_vp(struct vnode *, int))(void);
static void lio_set_error(aio_req_t *);
static aio_t *aio_aiop_alloc();
static int aio_req_alloc(aio_req_t **, aio_result_t *);
static int aio_lio_alloc(aio_lio_t **);
static aio_req_t *aio_req_done(aio_result_t *);
static aio_req_t *aio_req_remove(aio_req_t *);
static int aio_req_find(aio_result_t *, aio_req_t **);
static int aio_hash_insert(struct aio_req_t *, aio_t *);
static int aio_req_setup(aio_req_t **, aio_t *, aiocb_t *, aio_result_t *);
static int aio_cleanup_thread(aio_t *);
static aio_lio_t *aio_list_get(aio_result_t *);
static void lio_set_uerror(aio_result_t *, int);

#ifdef _LARGEFILE64_SOURCE
static int aiorw64(aiorw_args64_t *, int);
static int alio64(alio64_args_t *);
static int aliowait64(alio64_args_t *);
static int aiosuspend64(aiosuspend64_args_t *, rval_t *);
static int aioerror64(aioerror64_args_t *);
static void alio64_cleanup(aio_t *, aiocb64_t **, int);
static int aio_req_setup64(aio_req_t **, aio_t *, aiocb64_t *, aio_result_t *);
#endif

/*
 * implementation specific functions (external)
 */
void aio_req_free(aio_req_t *, aio_t *);

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>
#include <sys/syscall.h>


static struct sysent kaio_sysent = {
	7,
	SE_NOUNLOAD,			/* not unloadable once loaded */
	kaio
};

/*
 * Module linkage information for the kernel.
 */
extern struct mod_ops mod_syscallops;

static struct modlsys modlsys = {
	&mod_syscallops,
	"kernel Async I/O",
	&kaio_sysent
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlsys, NULL
};

_init()
{
	int retval;

	if ((retval = mod_install(&modlinkage)) != 0)
		return (retval);

	return (0);
}

_fini()
{
	register int retval;

	retval = mod_remove(&modlinkage);

	return (retval);
}

_info(modinfop)
	struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}

static int
kaio(
	register kaio_args_t *uap,
	rval_t *rvp)
{
	switch (uap->opcode & ~AIO_POLL_BIT) {
	case AIOREAD:
		return (arw((arw_args_t *)uap, FREAD));
	case AIOWRITE:
		return (arw((arw_args_t *)uap, FWRITE));
	case AIOWAIT:
		return (aiowait((aiowait_args_t *)uap, rvp));
	case AIONOTIFY:
		return (aionotify());
	case AIOINIT:
		return (aioinit());
	case AIOSTART:
		return (aiostart());
	case AIOLIO:
		return (alio((alio_args_t *)uap));
	case AIOLIOWAIT:
		return (aliowait((alio_args_t *)uap));
	case AIOSUSPEND:
		return (aiosuspend((aiosuspend_args_t *)uap, rvp));
	case AIOERROR:
		return (aioerror((aioerror_args_t *)uap));
	case AIOAREAD:
		return (aiorw((aiorw_args_t *)uap, FREAD));
	case AIOAWRITE:
		return (aiorw((aiorw_args_t *)uap, FWRITE));

#ifdef _LARGEFILE64_SOURCE
	case AIOLIO64:
		return (alio64((alio64_args_t *)uap));
	case AIOLIOWAIT64:
		return (aliowait64((alio64_args_t *)uap));
	case AIOSUSPEND64:
		return (aiosuspend64((aiosuspend64_args_t *)uap, rvp));
	case AIOERROR64:
		return (aioerror64((aioerror64_args_t *)uap));
	case AIOAREAD64:
		return (aiorw64((aiorw_args64_t *)uap, FREAD));
	case AIOAWRITE64:
		return (aiorw64((aiorw_args64_t *)uap, FWRITE));
#endif

	default:
		return (EINVAL);
	}
}

/*
 * wake up LWPs in this process that are sleeping in
 * aiowait().
 */
static int
aionotify(void)
{
	aio_t	*aiop;

	aiop = curproc->p_aio;
	if (aiop == NULL)
		return (0);

	mutex_enter(&aiop->aio_mutex);
	aiop->aio_notifycnt++;
	cv_broadcast(&aiop->aio_waitcv);
	mutex_exit(&aiop->aio_mutex);

	return (0);
}

static int
aiowait(
	aiowait_args_t *uap,
	rval_t *rvp)
{
	int 		error = 0;
	aio_t		*aiop;
	aio_req_t	*reqp;
	struct timeval 	wait_time, now;
	long		ticks = 0;
	int 		status;
	int		blocking;

	aiop = curproc->p_aio;
	if (aiop == NULL)
		return (EINVAL);

	/*
	 * don't block if caller wants to poll, blocking should
	 * be set to zero.
	 */
	if (((int)uap->timeout == -1) || (uap->timeout == NULL))
		blocking = 0;
	else if (uap->timeout) {
		if (copyin((caddr_t)uap->timeout, (caddr_t)&wait_time,
		    sizeof (wait_time)))
			return (EFAULT);
		if (wait_time.tv_sec > 0 || wait_time.tv_usec > 0) {
			if (error = itimerfix(&wait_time)) {
				return (error);
			}
			blocking = 1;
			uniqtime(&now);
			timevaladd(&wait_time, &now);
			ticks = hzto(&wait_time);
			ticks += lbolt;
		} else
			blocking = 0;
	} else
		blocking = 1;

	/*
	 * doneq was empty, however, caller might want to block until
	 * some IO is done.
	 */
	mutex_enter(&aiop->aio_mutex);
	for (;;) {
		/* process requests on poll queue */
		if (aiop->aio_pollq) {
			mutex_exit(&aiop->aio_mutex);
			aio_cleanup(0);
			mutex_enter(&aiop->aio_mutex);
		}
		if (reqp = aio_req_remove(NULL)) {
			rvp->r_val1 = (int)reqp->aio_req_resultp;
			break;
		}
		/* user-level done queue might not be empty */
		if (aiop->aio_notifycnt > 0) {
			aiop->aio_notifycnt--;
			rvp->r_val1 = 1;
			error = 0;
			break;
		}
		/* don't block if no outstanding aio */
		if (aiop->aio_outstanding == 0) {
			error = EINVAL;
			break;
		}
		if (blocking) {
			if (ticks)
				status = cv_timedwait_sig(&aiop->aio_waitcv,
				    &aiop->aio_mutex, ticks);
			else
				status = cv_wait_sig(&aiop->aio_waitcv,
					    &aiop->aio_mutex);

			if (status > 0)
				/* check done queue again */
				continue;
			else if (status == 0)
				/* interrupted by a signal */
				error = EINTR;
			else if (status == -1)
				/* timer expired */
				error = ETIME;
		}
		break;
	}
	mutex_exit(&aiop->aio_mutex);
	if (reqp) {
		aphysio_cleanup(reqp);
		mutex_enter(&aiop->aio_mutex);
		aio_req_free(reqp, aiop);
		mutex_exit(&aiop->aio_mutex);
	}
	return (error);
}

static int
aiosuspend(
	aiosuspend_args_t *uap,
	rval_t *rvp)
{
	int 		error = 0;
	aio_t		*aiop;
	aio_req_t	*reqp, *found, *next;
	struct timespec	wait_time, now;
	aiocb_t		*scbp[_AIO_LISTIO_MAX];
	caddr_t		cbplist;
	aiocb_t		*cbp, **ucbp;
	long		ticks = 0;
	int 		rv;
	int		blocking;
	int		nent;
	int		i, ssize;
	int		lio_dynamic_flg;	/* kmem_alloc'ed list of cb's */

	aiop = curproc->p_aio;
	if (aiop == NULL || uap->nent <= 0)
		return (EINVAL);

	if (uap->nent > _AIO_LISTIO_MAX) {
		ssize = (sizeof (aiocb_t *) * uap->nent);
		cbplist = kmem_alloc(ssize, KM_NOSLEEP);
		if (cbplist == NULL)
			return (EINVAL);
		lio_dynamic_flg = 1;
	} else {
		cbplist = (caddr_t)scbp;
		lio_dynamic_flg = 0;
	}

	/*
	 * first copy the timeval struct into the kernel.
	 * if the caller is polling, the caller will not
	 * block and "blocking" should be zero.
	 */
	if (uap->timeout) {
		if (copyin((caddr_t)uap->timeout, (caddr_t)&wait_time,
		    sizeof (wait_time))) {
			error = EFAULT;
			goto done;
		}

		if (wait_time.tv_sec > 0 || wait_time.tv_nsec > 0) {
			if (error = itimerspecfix(&wait_time)) {
				goto done;
			}
			blocking = 1;
			gethrestime(&now);
			timespecadd(&wait_time, &now);
			ticks = timespectohz(&wait_time, now);
			ticks += lbolt;
		} else
			blocking = 0;
	} else
		blocking = 1;

	/* get the array of aiocb's you're waiting for */
	if (copyin((caddr_t)uap->aiocb, (caddr_t)cbplist,
	    (sizeof (aiocb_t *) * uap->nent))) {
		error = EFAULT;
		goto done;
	}

	error = 0;
	found = NULL;
	nent = uap->nent;
	mutex_enter(&aiop->aio_mutex);
	for (;;) {
		/* push requests on poll queue to done queue */
		if (aiop->aio_pollq) {
			mutex_exit(&aiop->aio_mutex);
			aio_cleanup(0);
			mutex_enter(&aiop->aio_mutex);
		}
		/* check for requests on done queue */
		if (aiop->aio_doneq) {
			ucbp = (aiocb_t **)cbplist;
			for (i = 0; i < nent; i++, ucbp++) {
				if ((cbp = *ucbp) == NULL)
					continue;
				if (reqp = aio_req_done(&cbp->aio_resultp)) {
					reqp->aio_req_next = found;
					found = reqp;
					continue;
				}
				if (aiop->aio_doneq == NULL)
					break;
			}
			if (found)
				break;
		}
		if (aiop->aio_notifycnt > 0) {
			/*
			 * nothing on the kernel's queue. the user
			 * has notified the kernel that it has items
			 * on a user-level queue.
			 */
			aiop->aio_notifycnt--;
			rvp->r_val1 = 1;
			error = 0;
			break;
		}
		/* don't block if nothing is outstanding */
		if (aiop->aio_outstanding == 0) {
			error = EAGAIN;
			break;
		}
		if (blocking) {
			if (ticks)
				rv = cv_timedwait_sig(&aiop->aio_waitcv,
					    &aiop->aio_mutex, ticks);
			else
				rv = cv_wait_sig(&aiop->aio_waitcv,
					    &aiop->aio_mutex);
			if (rv > 0)
				/* check done queue again */
				continue;
			else if (rv == 0)
				/* interrupted by a signal */
				error = EINTR;
			else if (rv == -1)
				/* timer expired */
				error = ETIME;
		}
		break;
	}
	mutex_exit(&aiop->aio_mutex);
	for (reqp = found; reqp != NULL; reqp = next) {
		/*
		 * force a aio_copyout_result()
		 */
		reqp->aio_req_flags &= ~AIO_POLL;
		next = reqp->aio_req_next;
		aphysio_cleanup(reqp);
		mutex_enter(&aiop->aio_mutex);
		aio_req_free(reqp, aiop);
		mutex_exit(&aiop->aio_mutex);
	}
done:
	if (lio_dynamic_flg)
		kmem_free(cbplist, ssize);
	return (error);
}

/*
 * initialize aio by allocating an aio_t struct for this
 * process.
 */
static int
aioinit(void)
{
	proc_t *p = curproc;
	aio_t *aiop;
	mutex_enter(&p->p_lock);
	if ((aiop = p->p_aio) == NULL) {
		aiop = aio_aiop_alloc();
		p->p_aio = aiop;
	}
	mutex_exit(&p->p_lock);
	if (aiop == NULL)
		return (ENOMEM);
	return (0);
}

/*
 * start a special thread that will cleanup after aio requests
 * that are preventing a segment from being unmapped. as_unmap()
 * blocks until all phsyio to this segment is completed. this
 * doesn't happen until all the pages in this segment are not
 * SOFTLOCKed. Some pages will be SOFTLOCKed when there are aio
 * requests still outstanding. this special thread will make sure
 * that these SOFTLOCKed pages will eventually be SOFTUNLOCKed.
 *
 * this function will return an error if the process has only
 * one LWP. the assumption is that the caller is a separate LWP
 * that remains blocked in the kernel for the life of this process.
 */
static int
aiostart()
{
	proc_t *p = curproc;
	aio_t *aiop;
	int first, error = 0;

	if (p->p_lwpcnt == 1)
		return (EDEADLK);
	mutex_enter(&p->p_lock);
	if ((aiop = p->p_aio) == NULL)
		error = EINVAL;
	else {
		first = aiop->aio_ok;
		if (aiop->aio_ok == 0)
			aiop->aio_ok = 1;
	}
	mutex_exit(&p->p_lock);
	if (error == 0 && first == 0) {
		return (aio_cleanup_thread(aiop));
		/* should never return */
	}
	return (error);
}

/*
 * Asynchronous list IO. A chain of aiocb's are copied in
 * one at a time. If the aiocb is invalid, it is skipped.
 * For each aiocb, the appropriate driver entry point is
 * called. Optimize for the common case where the list
 * of requests is to the same file descriptor.
 *
 * One possible optimization is to define a new driver entry
 * point that supports a list of IO requests. Whether this
 * improves performance depends somewhat on the driver's
 * locking strategy. Processing a list could adversely impact
 * the driver's interrupt latency.
 */
static int
alio(alio_args_t	*uap)
{
	file_t		*fp;
	file_t		*prev_fp = 0;
	struct vnode	*vp;
	aio_lio_t	*head;
	aio_req_t	*reqp;
	aio_t		*aiop;
	aiocb_t		*scbp[_AIO_LISTIO_MAX];
	caddr_t		cbplist;
	aiocb_t		*cbp, **ucbp;
	aiocb_t		cb;
	aiocb_t		*aiocb = &cb;
	struct sigevent sigev;
	sigqueue_t	*sqp;
	int		(*aio_func)();
	int		mode;
	int		nent;
	int		error = 0;
	int		i, ssize;
	int		lio_dynamic_flg; /* kmem_alloc()'ed list of cb's */
	int		aio_notsupported = 0;

	aiop = curproc->p_aio;
	if (aiop == NULL || uap->nent <= 0)
		return (EINVAL);

	nent = uap->nent;

	if (nent > _AIO_LISTIO_MAX) {
		ssize = (sizeof (aiocb_t *) * nent);
		cbplist = kmem_alloc(ssize, KM_NOSLEEP);
		if (cbplist == NULL)
			return (EINVAL);
		lio_dynamic_flg  = 1;
	} else {
		cbplist = (caddr_t)scbp;
		lio_dynamic_flg = 0;
	}
	ucbp = (aiocb_t **)cbplist;


	if (copyin((caddr_t)uap->aiocb, (caddr_t)cbplist,
	    sizeof (aiocb_t *) * nent)) {
		error = EFAULT;
		goto done;
	}

	if (uap->sigev) {
		if (copyin((caddr_t)uap->sigev, (caddr_t)&sigev,
		    sizeof (struct sigevent))) {
			error = EFAULT;
			goto done;
		}
	}

	/*
	 * a list head should be allocated if notification is
	 * enabled for this list.
	 */
	head = NULL;
	if ((uap->mode == LIO_WAIT) || uap->sigev) {
		mutex_enter(&aiop->aio_mutex);
		error = aio_lio_alloc(&head);
		mutex_exit(&aiop->aio_mutex);
		if (error) {
			goto done;
		}
		head->lio_nent = uap->nent;
		head->lio_refcnt = uap->nent;
		if (uap->sigev && (sigev.sigev_notify == SIGEV_SIGNAL) &&
		    (sigev.sigev_signo > 0 && sigev.sigev_signo < NSIG)) {
			sqp = (sigqueue_t *)kmem_zalloc(sizeof (sigqueue_t),
				    KM_NOSLEEP);
			if (sqp == NULL) {
				error = EAGAIN;
				goto done;
			}
			sqp->sq_func = NULL;
			sqp->sq_next = NULL;
			sqp->sq_info.si_code = SI_QUEUE;
			sqp->sq_info.si_pid = curproc->p_pid;
			sqp->sq_info.si_uid = curproc->p_cred->cr_uid;
			sqp->sq_info.si_signo = sigev.sigev_signo;
			sqp->sq_info.si_value = sigev.sigev_value;
			head->lio_sigqp = sqp;
		} else
			head->lio_sigqp = NULL;
	}

	for (i = 0; i < nent; i++, ucbp++) {

		cbp = *ucbp;
		/* skip entry if it can't be copied. */
		if (cbp == NULL || copyin((caddr_t)cbp, (caddr_t)aiocb,
		    sizeof (aiocb_t))) {
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			continue;
		}

		/* skip if opcode for aiocb is LIO_NOP */

		mode = aiocb->aio_lio_opcode;
		if (mode == LIO_NOP) {
			cbp = NULL;
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			continue;
		}

		/* increment file descriptor's ref count. */
		if ((fp = getf(aiocb->aio_fildes)) == NULL) {
			lio_set_uerror(&cbp->aio_resultp, EBADF);
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			continue;
		}

		/*
		 * check the permission of the partition
		 */
		mode = aiocb->aio_lio_opcode;
		if ((fp->f_flag & mode) == 0) {
			releasef(aiocb->aio_fildes);
			lio_set_uerror(&cbp->aio_resultp, EBADF);
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			continue;
		}

		/*
		 * common case where requests are to the same fd.
		 * for UFS, need to set ENOTSUP
		 */
		if (fp != prev_fp) {
			vp = fp->f_vnode;
			aio_func = check_vp(vp, mode);
			if (aio_func == NULL) {
				prev_fp = NULL;
				releasef(aiocb->aio_fildes);
				lio_set_uerror(&cbp->aio_resultp, ENOTSUP);
				aio_notsupported++;
				if (head) {
					mutex_enter(&aiop->aio_mutex);
					head->lio_nent--;
					head->lio_refcnt--;
					mutex_exit(&aiop->aio_mutex);
				}
				continue;
			} else
				prev_fp = fp;
		}
		if (error = aio_req_setup(&reqp, aiop, aiocb,
					&cbp->aio_resultp)) {
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent = i;
				head->lio_refcnt = 0;
				mutex_exit(&aiop->aio_mutex);
			}
			goto done;
		}

		reqp->aio_req_lio = head;

		/*
		 * send the request to driver.
		 */
		error = (*aio_func)((VTOS(vp))->s_dev,
			(aio_req_t *)&reqp->aio_req, CRED());
		/*
		 * the fd's ref count is not decremented until the IO has
		 * completed unless there was an error.
		 */
		if (error) {
			releasef(aiocb->aio_fildes);
			lio_set_error(reqp);
			lio_set_uerror(&cbp->aio_resultp, error);
			if (error == ENOTSUP) {
				aio_notsupported++;
				if (head) {
					mutex_enter(&aiop->aio_mutex);
					head->lio_nent--;
					head->lio_refcnt--;
					mutex_exit(&aiop->aio_mutex);
				}
			}
		}
	}

	if (aio_notsupported) {
		error = ENOTSUP;
	} else if (uap->mode == LIO_WAIT) {
		mutex_enter(&aiop->aio_mutex);
		while (head->lio_refcnt > 0) {
			if (!cv_wait_sig(&head->lio_notify, &aiop->aio_mutex)) {
				mutex_exit(&aiop->aio_mutex);
				error = EINTR;
				goto done;
			}
		}
		mutex_exit(&aiop->aio_mutex);
		alio_cleanup(aiop, (aiocb_t **)cbplist, nent);
	}
done:
	if (lio_dynamic_flg)
		kmem_free(cbplist, ssize);
	return (error);
}

/*
 * Asynchronous list IO.
 * If list I/O is called with LIO_WAIT it can still return
 * before all the I/O's are completed if a signal is caught
 * or if the list include UFS I/O requests. If this happens,
 * libaio will call aliowait() to wait for the I/O's to
 * complete
 */
static int
aliowait(alio_args_t	*uap)
{
	aio_lio_t	*head;
	aio_t		*aiop;
	aiocb_t		*scbp[_AIO_LISTIO_MAX];
	caddr_t		cbplist;
	aiocb_t		*cbp, **ucbp;
	int		nent;
	int		error = 0;
	int		i, ssize;
	int		lio_dynamic_flg; /* kmem_alloc()'ed list of cb's */

	aiop = curproc->p_aio;
	if (aiop == NULL || uap->nent <= 0)
		return (EINVAL);

	nent = uap->nent;

	if (nent > _AIO_LISTIO_MAX) {
		ssize = (sizeof (aiocb_t *) * nent);
		cbplist = kmem_alloc(ssize, KM_NOSLEEP);
		if (cbplist == NULL)
			return (EINVAL);
		lio_dynamic_flg = 1;
	} else {
		cbplist = (caddr_t)scbp;
		lio_dynamic_flg = 0;
	}
	ucbp = (aiocb_t **)cbplist;

	if (copyin((caddr_t)uap->aiocb, (caddr_t)cbplist,
	    sizeof (aiocb_t *) * nent)) {
		error = EFAULT;
		goto done;
	}
	/*
	 * To find the list head, we go through the
	 * list of aiocb structs, find the request
	 * its for, then get the list head that reqp
	 * points to
	 */
	head = NULL;

	for (i = 0; i < nent; i++) {
		if ((cbp = *ucbp++) == NULL)
			continue;
		if (head = aio_list_get(&cbp->aio_resultp))
			break;
	}

	if (head == NULL) {
		error = EINVAL;
		goto done;
	}

	mutex_enter(&aiop->aio_mutex);
	while (head->lio_refcnt > 0) {
		if (!cv_wait_sig(&head->lio_notify, &aiop->aio_mutex)) {
			mutex_exit(&aiop->aio_mutex);
			error = EINTR;
			goto done;
		}
	}
	mutex_exit(&aiop->aio_mutex);
	alio_cleanup(aiop, (aiocb_t **)cbplist, nent);
done:
	if (lio_dynamic_flg)
		kmem_free(cbplist, ssize);
	return (error);
}

aio_lio_t *
aio_list_get(resultp)
aio_result_t *resultp;
{
	aio_lio_t	*head = NULL;
	aio_t		*aiop;
	aio_req_t 	**bucket;
	aio_req_t 	*reqp;
	int		index;

	aiop = curproc->p_aio;
	if (aiop == NULL)
		return (NULL);

	if (resultp) {
		index = AIO_HASH(resultp);
		bucket = &aiop->aio_hash[index];
		for (reqp = *bucket; reqp != NULL;
			reqp = reqp->aio_hash_next) {
			if (reqp->aio_req_resultp == resultp) {
				head = reqp->aio_req_lio;
				return (head);
			}
		}
	}
	return (NULL);
}


static void
lio_set_uerror(resultp, error)
aio_result_t *resultp;
int error;
{
	/*
	 * the resultp field is a pointer to where the
	 * error should be written out to the user's
	 * aiocb.
	 */
	(void) suword((int *)(&resultp->aio_errno), error);
	(void) suword((int *)(&resultp->aio_return), -1);

}
/*
 * do cleanup completion for all requests in list. memory for
 * each request is also freed.
 */
static void
alio_cleanup(aio_t *aiop, aiocb_t **cbp, int nent)
{
	int i;
	aio_req_t *reqp;
	aio_result_t *resultp;

	for (i = 0; i < nent; i++) {
		if (cbp[i] == NULL)
			continue;
		resultp = &cbp[i]->aio_resultp;
		mutex_enter(&aiop->aio_mutex);
		reqp = aio_req_done(resultp);
		mutex_exit(&aiop->aio_mutex);
		if (reqp != NULL) {
			aphysio_cleanup(reqp);
			mutex_enter(&aiop->aio_mutex);
			aio_req_free(reqp, aiop);
			mutex_exit(&aiop->aio_mutex);
		}
	}
}

/*
 * write out the results for an aio request that is
 * done.
 */
static int
aioerror(aioerror_args_t *uap)
{
	aio_result_t *resultp;
	aio_t *aiop;
	aio_req_t *reqp;
	int retval;

	aiop = curproc->p_aio;
	if (aiop == NULL || uap->cb == NULL)
		return (EINVAL);
	resultp = &uap->cb->aio_resultp;
	mutex_enter(&aiop->aio_mutex);
	retval = aio_req_find(resultp, &reqp);
	mutex_exit(&aiop->aio_mutex);
	if (retval == 0) {
		/*
		 * force a aio_copyout_result()
		 */
		reqp->aio_req_flags &= ~AIO_POLL;
		aphysio_cleanup(reqp);
		mutex_enter(&aiop->aio_mutex);
		aio_req_free(reqp, aiop);
		mutex_exit(&aiop->aio_mutex);
		return (0);
	} else if (retval == 1)
		return (EINPROGRESS);
	else if (retval == 2)
		return (EINVAL);
	return (0);
}

/*
 * solaris version of asynchronous read and write
 */
static int
arw(
	arw_args_t 	*uap,
	int		mode)
{
	file_t		*fp;
	int		error;
	struct vnode	*vp;
	aio_req_t	*reqp;
	aio_t		*aiop;
	int		(*aio_func)();
	struct snode *sp;
	struct vnode *cvp;
	struct snode *csp;	/* common snode ptr */
	aiocb_t	aiocb;

	aiop = curproc->p_aio;
	if (aiop == NULL)
		return (EINVAL);

	if ((fp = getf(uap->fdes)) == NULL) {
		return (EBADF);
	}

	/*
	 * check the permission of the partition
	 */
	if ((fp->f_flag & mode) == 0) {
		releasef(uap->fdes);
		return (EBADF);
	}

	vp = fp->f_vnode;
	aio_func = check_vp(vp, mode);
	if (aio_func == NULL) {
		releasef(uap->fdes);
		return (ENOTSUP);
	}

	aiocb.aio_fildes = uap->fdes;
	aiocb.aio_buf = uap->bufp;
	aiocb.aio_nbytes = uap->bufsize;
	aiocb.aio_offset = uap->offset;
	aiocb.aio_sigevent.sigev_notify = 0;
	error = aio_req_setup(&reqp, aiop, &aiocb, uap->resultp);
	if (error) {
		releasef(uap->fdes);
		return (error);
	}
	/*
	 * enable polling on this request if the opcode has
	 * the AIO poll bit set
	 */
	if (uap->opcode & AIO_POLL_BIT)
		reqp->aio_req_flags |= AIO_POLL;
	/*
	 * Pass the dip through to aphysio via the aio_req struct
	 */
	sp = VTOS(vp);
	cvp = sp->s_commonvp;
	csp = VTOS(cvp);
	reqp->aio_req_pm_dip = csp->s_dip;
	/*
	 * send the request to driver.
	 */
	error = (*aio_func)((VTOS(vp))->s_dev,
			(struct aio_req *)&reqp->aio_req, CRED());
	/*
	 * the fd is stored in the aio_req_t by aio_req_setup(), and
	 * is released by the aio_cleanup_thread() when the IO has
	 * completed.
	 */
	if (error) {
		releasef(uap->fdes);
		mutex_enter(&aiop->aio_mutex);
		aio_req_free(reqp, aiop);
		aiop->aio_pending--;
		mutex_exit(&aiop->aio_mutex);
	}
	return (error);
}

/*
 * posix version of asynchronous read and write
 */
static int
aiorw(
	aiorw_args_t	*uap,
	int		mode)
{
	aiocb_t		aiocb;
	file_t		*fp;
	int		error;
	struct vnode	*vp;
	aio_req_t	*reqp;
	aio_t		*aiop;
	int		(*aio_func)();
	aio_result_t	*resultp;
	struct snode *sp;
	struct vnode *cvp;
	struct snode *csp;	/* common snode ptr */

	aiop = curproc->p_aio;
	if (aiop == NULL)
		return (EINVAL);

	if (copyin((caddr_t)uap->aiocb, &aiocb, sizeof (aiocb_t)))
		return (EFAULT);


	if ((fp = getf(aiocb.aio_fildes)) == NULL) {
		return (EBADF);
	}

	/*
	 * check the permission of the partition
	 */

	if ((fp->f_flag & mode) == 0) {
		releasef(aiocb.aio_fildes);
		return (EBADF);
	}

	vp = fp->f_vnode;
	aio_func = check_vp(vp, mode);
	if (aio_func == NULL) {
		releasef(aiocb.aio_fildes);
		return (ENOTSUP);
	}

	resultp = &(uap->aiocb->aio_resultp);
	error = aio_req_setup(&reqp, aiop, &aiocb, resultp);
	if (error) {
		releasef(aiocb.aio_fildes);
		return (error);
	}
	/*
	 * enable polling on this request if the opcode has
	 * the AIO poll bit set
	 */
	if (uap->opcode & AIO_POLL_BIT)
		reqp->aio_req_flags |= AIO_POLL;
	/*
	 * Pass the dip through to aphysio via the aio_req struct
	 */
	sp = VTOS(vp);
	cvp = sp->s_commonvp;
	csp = VTOS(cvp);
	reqp->aio_req_pm_dip = csp->s_dip;
	/*
	 * send the request to driver.
	 */
	error = (*aio_func)((VTOS(vp))->s_dev,
			(struct aio_req *)&reqp->aio_req, CRED());
	/*
	 * the fd is stored in the aio_req_t by aio_req_setup(), and
	 * is released by the aio_cleanup_thread() when the IO has
	 * completed.
	 */
	if (error) {
		releasef(aiocb.aio_fildes);
		mutex_enter(&aiop->aio_mutex);
		aio_req_free(reqp, aiop);
		aiop->aio_pending--;
		mutex_exit(&aiop->aio_mutex);
	}
	return (error);
}

/*
 * set error for a list IO entry that failed.
 */
static void
lio_set_error(aio_req_t *reqp)
{
	aio_t *aiop = curproc->p_aio;

	if (aiop == NULL)
		return;

	mutex_enter(&aiop->aio_mutex);
	aiop->aio_pending--;
	aiop->aio_outstanding--;
	/* request failed, AIO_DONE set to aviod physio cleanup. */
	reqp->aio_req_flags |= AIO_DONE;
	if (reqp->aio_req_lio != NULL)
		reqp->aio_req_lio->lio_refcnt--;
	mutex_exit(&aiop->aio_mutex);
}

/*
 * check if a specified request is done, and remove it from
 * the done queue. otherwise remove anybody from the done queue
 * if NULL is specified.
 */
static aio_req_t *
aio_req_done(aio_result_t *resultp)
{
	aio_req_t **bucket;
	aio_req_t *ent;
	aio_t *aiop = curproc->p_aio;
	int index;

	ASSERT(MUTEX_HELD(&aiop->aio_mutex));

	if (resultp) {
		index = AIO_HASH(resultp);
		bucket = &aiop->aio_hash[index];
		for (ent = *bucket; ent != NULL; ent = ent->aio_hash_next) {
			if (ent->aio_req_resultp == resultp) {
				if ((ent->aio_req_flags & AIO_PENDING) == 0) {
					return (aio_req_remove(ent));
				}
				return (NULL);
			}
		}
		/* no match, resultp is invalid */
		return (NULL);
	}
	return (aio_req_remove(NULL));
}

/*
 * determine if a user-level resultp pointer is associated with an
 * active IO request. Zero is returned when the request is done,
 * and the request is removed from the done queue. Only when the
 * return value is zero, is the "reqp" pointer valid. One is returned
 * when the request is inprogress. Two is returned when the request
 * is invalid.
 */
static int
aio_req_find(aio_result_t *resultp, aio_req_t **reqp)
{
	aio_req_t **bucket;
	aio_req_t *ent;
	aio_t *aiop = curproc->p_aio;
	int index;

	ASSERT(MUTEX_HELD(&aiop->aio_mutex));

	index = AIO_HASH(resultp);
	bucket = &aiop->aio_hash[index];
	for (ent = *bucket; ent != NULL; ent = ent->aio_hash_next) {
		if (ent->aio_req_resultp == resultp) {
			if ((ent->aio_req_flags & AIO_PENDING) == 0) {
				*reqp = aio_req_remove(ent);
				return (0);
			}
			return (1);
		}
	}
	/* no match, resultp is invalid */
	return (2);
}

/*
 * remove a request from the done queue.
 */
static aio_req_t *
aio_req_remove(aio_req_t *reqp)
{
	aio_t *aiop = curproc->p_aio;
	aio_req_t *head;

	if (reqp) {
		if (reqp->aio_req_next == reqp)
			/* only one request on queue */
			aiop->aio_doneq = NULL;
		else {
			reqp->aio_req_next->aio_req_prev = reqp->aio_req_prev;
			reqp->aio_req_prev->aio_req_next = reqp->aio_req_next;
			if (reqp == aiop->aio_doneq)
				aiop->aio_doneq = reqp->aio_req_next;
		}
		return (reqp);
	}
	if (aiop->aio_doneq) {
		head = aiop->aio_doneq;
		if (head == head->aio_req_next) {
			/* only one request on queue */
			aiop->aio_doneq = NULL;
		} else {
			head->aio_req_prev->aio_req_next = head->aio_req_next;
			head->aio_req_next->aio_req_prev = head->aio_req_prev;
			aiop->aio_doneq = head->aio_req_next;
		}
		return (head);
	}
	return (NULL);
}

static int
aio_req_setup(
	aio_req_t	**reqpp,
	aio_t 		*aiop,
	aiocb_t 	*arg,
	aio_result_t 	*resultp)
{
	aio_req_t 	*reqp;
	sigqueue_t	*sqp;
	struct uio 	*uio;

	struct sigevent *sigev;
	int error;

	mutex_enter(&aiop->aio_mutex);
	/*
	 * get an aio_reqp from the free list or allocate one
	 * from dynamic memory.
	 */
	if (error = aio_req_alloc(&reqp, resultp)) {
		mutex_exit(&aiop->aio_mutex);
		return (error);
	}
	aiop->aio_pending++;
	aiop->aio_outstanding++;
	mutex_exit(&aiop->aio_mutex);
	/*
	 * initialize aio request.
	 */
	reqp->aio_req_flags = AIO_PENDING;
	reqp->aio_req_fd = arg->aio_fildes;
	uio = reqp->aio_req.aio_uio;
	uio->uio_iovcnt = 1;
	uio->uio_iov->iov_base = (caddr_t)arg->aio_buf;
	uio->uio_iov->iov_len = arg->aio_nbytes;
	uio->uio_loffset = arg->aio_offset;
	sigev = &arg->aio_sigevent;
	if ((sigev->sigev_notify == SIGEV_SIGNAL) &&
	    (sigev->sigev_signo > 0 && sigev->sigev_signo < NSIG)) {
		sqp = (sigqueue_t *) kmem_zalloc(sizeof (sigqueue_t),
			    KM_NOSLEEP);
		if (sqp == NULL)
			return (EAGAIN);
		sqp->sq_func = NULL;
		sqp->sq_next = NULL;
		sqp->sq_info.si_code = SI_QUEUE;
		sqp->sq_info.si_pid = curproc->p_pid;
		sqp->sq_info.si_uid = curproc->p_cred->cr_uid;
		sqp->sq_info.si_signo = sigev->sigev_signo;
		sqp->sq_info.si_value.sival_int =
				sigev->sigev_value.sival_int;
		reqp->aio_req_sigqp = sqp;
	} else
		reqp->aio_req_sigqp = NULL;
	*reqpp = reqp;
	return (0);
}

/*
 * Allocate p_aio struct.
 */
static aio_t *
aio_aiop_alloc()
{
	aio_t	*aiop;
	char name[32];

	ASSERT(MUTEX_HELD(&curproc->p_lock));

	aiop = kmem_zalloc(sizeof (struct aio), KM_NOSLEEP);
	if (aiop) {
		sprintf(name, "aio done mutex %8d",
			(int)curproc->p_pidp->pid_id);

		mutex_init(&aiop->aio_mutex, name, MUTEX_DEFAULT,
			DEFAULT_WT);
	}
	return (aiop);
}

/*
 * Allocate an aio_req struct.
 */
static int
aio_req_alloc(aio_req_t **nreqp, aio_result_t *resultp)
{
	aio_req_t *reqp;
	aio_t *aiop = curproc->p_aio;

	ASSERT(MUTEX_HELD(&aiop->aio_mutex));

	if ((reqp = aiop->aio_free) != NULL) {
		reqp->aio_req_flags = 0;
		aiop->aio_free = reqp->aio_req_next;
	} else {
		/*
		 * Check whether memory is getting tight.
		 * This is a temporary mechanism to avoid memory
		 * exhaustion by a single process until we come up
		 * with a per process solution such as setrlimit().
		 */
		if (freemem < desfree)
			return (EAGAIN);

		reqp = kmem_zalloc(sizeof (struct aio_req_t), KM_NOSLEEP);
		if (reqp == NULL)
			return (EAGAIN);
		reqp->aio_req.aio_uio = &(reqp->aio_req_uio);
		reqp->aio_req.aio_uio->uio_iov = &(reqp->aio_req_iov);
		reqp->aio_req.aio_private = reqp;
	}

	reqp->aio_req_resultp = resultp;
	if (aio_hash_insert(reqp, aiop)) {
		reqp->aio_req_next = aiop->aio_free;
		aiop->aio_free = reqp;
		return (EINVAL);
	}
	*nreqp = reqp;
	return (0);
}

/*
 * Allocate an aio_lio_t struct.
 */
static int
aio_lio_alloc(aio_lio_t **head)
{
	aio_lio_t *liop;
	aio_t *aiop = curproc->p_aio;

	ASSERT(MUTEX_HELD(&aiop->aio_mutex));

	if ((liop = aiop->aio_lio_free) != NULL) {
		aiop->aio_lio_free = liop->lio_next;
	} else {
		/*
		 * Check whether memory is getting tight.
		 * This is a temporary mechanism to avoid memory
		 * exhaustion by a single process until we come up
		 * with a per process solution such as setrlimit().
		 */
		if (freemem < desfree)
			return (EAGAIN);

		liop = kmem_zalloc(sizeof (aio_lio_t), KM_NOSLEEP);
		if (liop == NULL)
			return (EAGAIN);
	}
	*head = liop;
	return (0);
}

/*
 * this is a special per-process thread that is only activated if
 * the process is unmapping a segment with outstanding aio. normally,
 * the process will have completed the aio before unmapping the
 * segment. If the process does unmap a segment with outstanding aio,
 * this special thread will guarentee that the locked pages due to
 * aphysio() are released, thereby permitting the segment to be
 * unmapped.
 */
static int
aio_cleanup_thread(aio_t *aiop)
{
	proc_t *p = curproc;
	struct as *as = p->p_as;
	aio_req_t *doneqtail, *pollqtail;
	int poked = 0;
	kcondvar_t *cvp;

	sigfillset(&curthread->t_hold);
	for (;;) {
		/*
		 * a segment is being unmapped. force all done requests
		 * to do the proper aphysio cleanup. the most expedient
		 * way is to put the doneq entries on the pollq and then
		 * call aio_cleanup() which will cleanup all entries on
		 * the pollq.
		 */
		mutex_enter(&aiop->aio_mutex);
		if (AS_ISUNMAPWAIT(as) && aiop->aio_doneq) {
			if (aiop->aio_pollq) {
				doneqtail = aiop->aio_doneq->aio_req_prev;
				pollqtail = aiop->aio_pollq->aio_req_prev;
				/* attach doneq to front of pollq */
				pollqtail->aio_req_next = aiop->aio_doneq;
				doneqtail->aio_req_next = aiop->aio_pollq;
				aiop->aio_doneq->aio_req_prev = pollqtail;
				aiop->aio_pollq->aio_req_prev = doneqtail;
			}
			aiop->aio_pollq = aiop->aio_doneq;
			aiop->aio_doneq = NULL;
		}
		aiop->aio_flags |= AIO_CLEANUP;
		mutex_exit(&aiop->aio_mutex);
		aio_cleanup(0);
		/*
		 * thread should block on the cleanupcv while
		 * AIO_CLEANUP is set.
		 */
		cvp = &aiop->aio_cleanupcv;
		mutex_enter(&aiop->aio_mutex);
		mutex_enter(&as->a_contents);
		if (aiop->aio_pollq == NULL) {
			/*
			 * AIO_CLEANUP determines when the cleanup thread
			 * should be active. this flag is only set when
			 * the cleanup thread is awakened by as_unmap().
			 * the flag is cleared when the blocking as_unmap()
			 * that originally awakened us is allowed to
			 * complete. as_unmap() blocks when trying to
			 * unmap a segment that has SOFTLOCKed pages. when
			 * the segment's pages are all SOFTUNLOCKed,
			 * as->a_flags & AS_UNMAPWAIT should be zero. The flag
			 * shouldn't be cleared right away if the cleanup thread
			 * was interrupted because the process is forking.
			 * this happens when cv_wait_sig() returns zero,
			 * because it was awakened by a pokelwps(), if
			 * the process is not exiting, it must be forking.
			 */
			if (AS_ISUNMAPWAIT(as) == 0 && !poked) {
				aiop->aio_flags &= ~AIO_CLEANUP;
				cvp = &as->a_cv;
			}
			mutex_exit(&aiop->aio_mutex);
			if (poked) {
				if (aiop->aio_pending == 0) {
					if (p->p_flag & EXITLWPS)
						break;
					else if (p->p_flag &
					    (HOLDFORK|HOLDFORK1|HOLDWATCH)) {
						/*
						 * hold LWP until it
						 * is continued.
						 */
						mutex_exit(&as->a_contents);
						mutex_enter(&p->p_lock);
						stop(PR_SUSPENDED,
						    SUSPEND_NORMAL);
						mutex_exit(&p->p_lock);
						poked = 0;
						continue;
					}
				}
				cv_wait(cvp, &as->a_contents);
			} else {
				poked = !cv_wait_sig(cvp, &as->a_contents);
			}
		} else
			mutex_exit(&aiop->aio_mutex);

		mutex_exit(&as->a_contents);
	}
exit:
	mutex_exit(&as->a_contents);
	ASSERT((curproc->p_flag & EXITLWPS));
	return (0);
}

/*
 * save a reference to a user's outstanding aio in a hash list.
 */
static int
aio_hash_insert(
	aio_req_t *aio_reqp,
	aio_t *aiop)
{
	int index;
	aio_result_t *resultp = aio_reqp->aio_req_resultp;
	aio_req_t *current;
	aio_req_t **nextp;

	index = AIO_HASH(resultp);
	nextp = &aiop->aio_hash[index];
	while ((current = *nextp) != NULL) {
		if (current->aio_req_resultp == resultp)
			return (DUPLICATE);
		nextp = &current->aio_hash_next;
	}
	*nextp = aio_reqp;
	aio_reqp->aio_hash_next = NULL;
	return (0);
}

static int
(*check_vp(struct vnode *vp, int mode))(void)
{

	struct snode    *sp = VTOS(vp);
	dev_t		dev = sp->s_dev;
	struct cb_ops  	*cb;
	major_t		major;
	int		(*aio_func)();

	major = getmajor(dev);

	/*
	 * return NULL for requests to files and STREAMs so
	 * that libaio takes care of them.
	 */
	if (vp->v_type == VCHR) {
		/* no stream device for kaio */
		if (STREAMSTAB(major)) {
			return (NULL);
		}
	} else {
		return (NULL);
	}

	/*
	 * Check old drivers which do not have async I/O entry points.
	 */
	if (devopsp[major]->devo_rev < 3)
		return (NULL);

	cb = devopsp[major]->devo_cb_ops;

	/*
	 * No support for mt-unsafe drivers.
	 */
	if (!(cb->cb_flag & D_MP))
		return (NULL);

	if (cb->cb_rev < 1)
		return (NULL);

	/*
	 * Check whether this device is a block device.
	 * Kaio is not supported for devices like tty.
	 */
	if (cb->cb_strategy == nodev || cb->cb_strategy == NULL)
		return (NULL);

	if (mode & FREAD)
		aio_func = cb->cb_aread;
	else
		aio_func = cb->cb_awrite;

	/*
	 * Do we need this ?
	 * nodev returns ENXIO anyway.
	 */
	if (aio_func == nodev)
		return (NULL);

	smark(sp, SACC);
	return (aio_func);
}

#ifdef _LARGEFILE64_SOURCE

/*
 * posix version of asynchronous read and write
 */
static int
aiorw64(
	aiorw_args64_t	*uap,
	int		mode)
{
	aiocb64_t	aiocb;
	file_t		*fp;
	int		error;
	struct vnode	*vp;
	aio_req_t	*reqp;
	aio_t		*aiop;
	aio_result_t	*resultp;
	int		(*aio_func)();
	struct snode 	*sp;
	struct vnode 	*cvp;
	struct snode 	*csp;	/* common snode ptr */

	aiop = curproc->p_aio;
	if (aiop == NULL)
		return (EINVAL);

	if (copyin((caddr_t)uap->aiocb, &aiocb, sizeof (aiocb_t)))
		return (EFAULT);

	if ((fp = getf(aiocb.aio_fildes)) == NULL) {
		return (EBADF);
	}

	/*
	 * check the permission of the partition
	 */
	if ((fp->f_flag & mode) == 0) {
		releasef(aiocb.aio_fildes);
		return (EBADF);
	}

	vp = fp->f_vnode;
	aio_func = check_vp(vp, mode);
	if (aio_func == NULL) {
		releasef(aiocb.aio_fildes);
		return (ENOTSUP);
	}

	resultp = &(uap->aiocb->aio_resultp);
	error = aio_req_setup64(&reqp, aiop, &aiocb, resultp);
	if (error) {
		releasef(aiocb.aio_fildes);
		return (error);
	}
	/*
	 * enable polling on this request if the opcode has
	 * the AIO poll bit set
	 */
	if (uap->opcode & AIO_POLL_BIT)
		reqp->aio_req_flags |= AIO_POLL;
	/*
	 * Pass the dip through to aphysio via the aio_req struct
	 */
	sp = VTOS(vp);
	cvp = sp->s_commonvp;
	csp = VTOS(cvp);
	reqp->aio_req_pm_dip = csp->s_dip;
	/*
	 * send the request to driver.
	 */
	error = (*aio_func)((VTOS(vp))->s_dev,
			(struct aio_req *)&reqp->aio_req, CRED());
	/*
	 * the fd is stored in the aio_req_t by aio_req_setup(), and
	 * is released by the aio_cleanup_thread() when the IO has
	 * completed.
	 */
	if (error) {
		releasef(aiocb.aio_fildes);
		mutex_enter(&aiop->aio_mutex);
		aio_req_free(reqp, aiop);
		aiop->aio_pending--;
		mutex_exit(&aiop->aio_mutex);
	}
	return (error);
}

static int
aiosuspend64(
	aiosuspend64_args_t *uap,
	rval_t *rvp)
{
	int 		error = 0;
	aio_t		*aiop;
	aio_req_t	*reqp, *found, *next;
	struct timespec	wait_time, now;
	aiocb64_t	*scbp[_AIO_LISTIO_MAX];
	caddr_t		cbplist;
	aiocb64_t	*cbp, **ucbp;
	long		ticks = 0;
	int 		rv;
	int		blocking;
	int		nent;
	int		i, ssize;
	int		lio_dynamic_flg;	/* kmem_alloc'ed list of cb's */

	aiop = curproc->p_aio;
	if (aiop == NULL || uap->nent <= 0)
		return (EINVAL);

	if (uap->nent > _AIO_LISTIO_MAX) {
		ssize = (sizeof (aiocb64_t *) * uap->nent);
		cbplist = kmem_alloc(ssize, KM_NOSLEEP);
		if (cbplist == NULL)
			return (EINVAL);
		lio_dynamic_flg = 1;
	} else {
		cbplist = (caddr_t)scbp;
		lio_dynamic_flg = 0;
	}

	/*
	 * first copy the timeval struct into the kernel.
	 * if the caller is polling, the caller will not
	 * block and "blocking" should be zero.
	 */
	if (uap->timeout) {
		if (copyin((caddr_t)uap->timeout, (caddr_t)&wait_time,
		    sizeof (wait_time))) {
			error = EFAULT;
			goto done;
		}

		if (wait_time.tv_sec > 0 || wait_time.tv_nsec > 0) {
			if (error = itimerspecfix(&wait_time)) {
				goto done;
			}
			blocking = 1;
			gethrestime(&now);
			timespecadd(&wait_time, &now);
			ticks = timespectohz(&wait_time, now);
			ticks += lbolt;
		} else
			blocking = 0;
	} else
		blocking = 1;

	/* get the array of aiocb's you're waiting for */
	if (copyin((caddr_t)uap->aiocb, (caddr_t)cbplist,
	    (sizeof (aiocb64_t *) * uap->nent))) {
		error = EFAULT;
		goto done;
	}

	error = 0;
	found = NULL;
	nent = uap->nent;
	mutex_enter(&aiop->aio_mutex);
	for (;;) {
		/* push requests on poll queue to done queue */
		if (aiop->aio_pollq) {
			mutex_exit(&aiop->aio_mutex);
			aio_cleanup(0);
			mutex_enter(&aiop->aio_mutex);
		}
		/* check for requests on done queue */
		if (aiop->aio_doneq) {
			ucbp = (aiocb64_t **)cbplist;
			for (i = 0; i < nent; i++, ucbp++) {
				if ((cbp = *ucbp) == NULL)
					continue;
				if (reqp = aio_req_done(&cbp->aio_resultp)) {
					reqp->aio_req_next = found;
					found = reqp;
					continue;
				}
				if (aiop->aio_doneq == NULL)
					break;
			}
			if (found)
				break;
		}
		if (aiop->aio_notifycnt > 0) {
			/*
			 * nothing on the kernel's queue. the user
			 * has notified the kernel that it has items
			 * on a user-level queue.
			 */
			aiop->aio_notifycnt--;
			rvp->r_val1 = 1;
			error = 0;
			break;
		}
		/* don't block if nothing is outstanding */
		if (aiop->aio_outstanding == 0) {
			error = EAGAIN;
			break;
		}
		if (blocking) {
			if (ticks)
				rv = cv_timedwait_sig(&aiop->aio_waitcv,
					    &aiop->aio_mutex, ticks);
			else
				rv = cv_wait_sig(&aiop->aio_waitcv,
					    &aiop->aio_mutex);
			if (rv > 0)
				/* check done queue again */
				continue;
			else if (rv == 0)
				/* interrupted by a signal */
				error = EINTR;
			else if (rv == -1)
				/* timer expired */
				error = ETIME;
		}
		break;
	}
	mutex_exit(&aiop->aio_mutex);
	for (reqp = found; reqp != NULL; reqp = next) {
		/*
		 * force a aio_copyout_result()
		 */
		reqp->aio_req_flags &= ~AIO_POLL;
		next = reqp->aio_req_next;
		aphysio_cleanup(reqp);
		mutex_enter(&aiop->aio_mutex);
		aio_req_free(reqp, aiop);
		mutex_exit(&aiop->aio_mutex);
	}
done:
	if (lio_dynamic_flg)
		kmem_free(cbplist, ssize);
	return (error);
}

/*
 * Asynchronous list IO. A chain of aiocb's are copied in
 * one at a time. If the aiocb is invalid, it is skipped.
 * For each aiocb, the appropriate driver entry point is
 * called. Optimize for the common case where the list
 * of requests is to the same file descriptor.
 *
 * One possible optimization is to define a new driver entry
 * point that supports a list of IO requests. Whether this
 * improves performance depends somewhat on the driver's
 * locking strategy. Processing a list could adversely impact
 * the driver's interrupt latency.
 */
static int
alio64(alio64_args_t	*uap)
{
	file_t		*fp;
	file_t		*prev_fp = 0;
	struct vnode	*vp;
	aio_lio_t	*head;
	aio_req_t	*reqp;
	aio_t		*aiop;
	aiocb64_t	*scbp[_AIO_LISTIO_MAX];
	caddr_t		cbplist;
	aiocb64_t	*cbp, **ucbp;
	aiocb64_t	cb;
	aiocb64_t	*aiocb = &cb;
	struct sigevent sigev;
	sigqueue_t	*sqp;
	int		(*aio_func)();
	int		mode;
	int		nent;
	int		error = 0;
	int		i, ssize;
	int		lio_dynamic_flg; /* kmem_alloc()'ed list of cb's */
	int		aio_notsupported = 0;

	aiop = curproc->p_aio;
	if (aiop == NULL || uap->nent <= 0)
		return (EINVAL);

	nent = uap->nent;

	if (nent > _AIO_LISTIO_MAX) {
		ssize = (sizeof (aiocb64_t *) * nent);
		cbplist = kmem_alloc(ssize, KM_NOSLEEP);
		if (cbplist == NULL)
			return (EINVAL);
		lio_dynamic_flg  = 1;
	} else {
		cbplist = (caddr_t)scbp;
		lio_dynamic_flg = 0;
	}
	ucbp = (aiocb64_t **)cbplist;

	if (copyin((caddr_t)uap->aiocb, (caddr_t)cbplist,
	    sizeof (aiocb64_t *) * nent)) {
		error = EFAULT;
		goto done;
	}

	if (uap->sigev) {
		if (copyin((caddr_t)uap->sigev, (caddr_t)&sigev,
		    sizeof (struct sigevent))) {
			error = EFAULT;
			goto done;
		}
	}

	/*
	 * a list head should be allocated if notification is
	 * enabled for this list.
	 */
	head = NULL;
	if ((uap->mode == LIO_WAIT) || uap->sigev) {
		mutex_enter(&aiop->aio_mutex);
		error = aio_lio_alloc(&head);
		mutex_exit(&aiop->aio_mutex);
		if (error) {
			goto done;
		}
		head->lio_nent = uap->nent;
		head->lio_refcnt = uap->nent;
		if (uap->sigev && (sigev.sigev_notify == SIGEV_SIGNAL) &&
		    (sigev.sigev_signo > 0 && sigev.sigev_signo < NSIG)) {
			sqp = (sigqueue_t *)kmem_zalloc(sizeof (sigqueue_t),
				    KM_NOSLEEP);
			if (sqp == NULL) {
				error = EAGAIN;
				goto done;
			}
			sqp->sq_func = NULL;
			sqp->sq_next = NULL;
			sqp->sq_info.si_code = SI_QUEUE;
			sqp->sq_info.si_pid = curproc->p_pid;
			sqp->sq_info.si_uid = curproc->p_cred->cr_uid;
			sqp->sq_info.si_signo = sigev.sigev_signo;
			sqp->sq_info.si_value.sival_int =
					sigev.sigev_value.sival_int;
			head->lio_sigqp = sqp;
		} else
			head->lio_sigqp = NULL;
	}

	for (i = 0; i < nent; i++, ucbp++) {

		cbp = *ucbp;
		/* skip entry if it can't be copied. */
		if (cbp == NULL || copyin((caddr_t)cbp, (caddr_t)aiocb,
		    sizeof (aiocb64_t))) {
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			continue;
		}

		/* skip if opcode for aiocb is LIO_NOP */

		mode = aiocb->aio_lio_opcode;
		if (mode == LIO_NOP) {
			cbp = NULL;
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			continue;
		}

		/* increment file descriptor's ref count. */
		if ((fp = getf(aiocb->aio_fildes)) == NULL) {
			lio_set_uerror(&cbp->aio_resultp, EBADF);
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			continue;
		}

		/*
		 * check the permission of the partition
		 */
		mode = aiocb->aio_lio_opcode;
		if ((fp->f_flag & mode) == 0) {
			releasef(aiocb->aio_fildes);
			lio_set_uerror(&cbp->aio_resultp, EBADF);
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent--;
				head->lio_refcnt--;
				mutex_exit(&aiop->aio_mutex);
			}
			continue;
		}

		/*
		 * common case where requests are to the same fd.
		 * for UFS, need to set ENOTSUP
		 */
		if (fp != prev_fp) {
			vp = fp->f_vnode;
			aio_func = check_vp(vp, mode);
			if (aio_func == NULL) {
				prev_fp = NULL;
				releasef(aiocb->aio_fildes);
				lio_set_uerror(&cbp->aio_resultp, ENOTSUP);
				aio_notsupported++;
				if (head) {
					mutex_enter(&aiop->aio_mutex);
					head->lio_nent--;
					head->lio_refcnt--;
					mutex_exit(&aiop->aio_mutex);
				}
				continue;
			} else
				prev_fp = fp;
		}
		if (error = aio_req_setup64(&reqp, aiop, aiocb,
				    &cbp->aio_resultp)) {
			if (head) {
				mutex_enter(&aiop->aio_mutex);
				head->lio_nent = i;
				head->lio_refcnt = 0;
				mutex_exit(&aiop->aio_mutex);
			}
			goto done;
		}

		reqp->aio_req_lio = head;

		/*
		 * send the request to driver.
		 */
		error = (*aio_func)((VTOS(vp))->s_dev,
			(aio_req_t *)&reqp->aio_req, CRED());
		/*
		 * the fd's ref count is not decremented until the IO has
		 * completed unless there was an error.
		 */
		if (error) {
			releasef(aiocb->aio_fildes);
			lio_set_error(reqp);
			lio_set_uerror(&cbp->aio_resultp, error);
			if (error == ENOTSUP) {
				aio_notsupported++;
				if (head) {
					mutex_enter(&aiop->aio_mutex);
					head->lio_nent--;
					head->lio_refcnt--;
					mutex_exit(&aiop->aio_mutex);
				}
			}
		}
	}

	if (aio_notsupported) {
		error = ENOTSUP;
	} else if (uap->mode == LIO_WAIT) {
		mutex_enter(&aiop->aio_mutex);
		while (head->lio_refcnt > 0) {
			if (!cv_wait_sig(&head->lio_notify, &aiop->aio_mutex)) {
				mutex_exit(&aiop->aio_mutex);
				error = EINTR;
				goto done;
			}
		}
		mutex_exit(&aiop->aio_mutex);
		alio64_cleanup(aiop, (aiocb64_t **)cbplist, nent);
	}
done:
	if (lio_dynamic_flg)
		kmem_free(cbplist, ssize);
	return (error);
}

/*
 * Asynchronous list IO.
 * If list I/O is called with LIO_WAIT it can still return
 * before all the I/O's are completed if a signal is caught
 * or if the list include UFS I/O requests. If this happens,
 * libaio will call aliowait() to wait for the I/O's to
 * complete
 */
static int
aliowait64(alio64_args_t *uap)
{
	aio_lio_t	*head;
	aio_t		*aiop;
	aiocb64_t	*scbp[_AIO_LISTIO_MAX];
	caddr_t		cbplist;
	aiocb64_t	*cbp, **ucbp;
	int		nent;
	int		error = 0;
	int		i, ssize;
	int		lio_dynamic_flg; /* kmem_alloc()'ed list of cb's */

	aiop = curproc->p_aio;
	if (aiop == NULL || uap->nent <= 0)
		return (EINVAL);

	nent = uap->nent;

	if (nent > _AIO_LISTIO_MAX) {
		ssize = (sizeof (aiocb64_t *) * nent);
		cbplist = kmem_alloc(ssize, KM_NOSLEEP);
		if (cbplist == NULL)
			return (EINVAL);
		lio_dynamic_flg = 1;
	} else {
		cbplist = (caddr_t)scbp;
		lio_dynamic_flg = 0;
	}
	ucbp = (aiocb64_t **)cbplist;

	if (copyin((caddr_t)uap->aiocb, (caddr_t)cbplist,
	    sizeof (aiocb64_t *) * nent)) {
		error = EFAULT;
		goto done;
	}
	/*
	 * To find the list head, we go through the
	 * list of aiocb structs, find the request
	 * its for, then get the list head that reqp
	 * points to
	 */
	head = NULL;

	for (i = 0; i < nent; i++) {
		if ((cbp = *ucbp++) == NULL)
			continue;
		if (head = aio_list_get(&cbp->aio_resultp))
			break;
	}

	if (head == NULL) {
		error = EINVAL;
		goto done;
	}

	mutex_enter(&aiop->aio_mutex);
	while (head->lio_refcnt > 0) {
		if (!cv_wait_sig(&head->lio_notify, &aiop->aio_mutex)) {
			mutex_exit(&aiop->aio_mutex);
			error = EINTR;
			goto done;
		}
	}
	mutex_exit(&aiop->aio_mutex);
	alio64_cleanup(aiop, (aiocb64_t **)cbplist, nent);
done:
	if (lio_dynamic_flg)
		kmem_free(cbplist, ssize);
	return (error);
}

/*
 * do cleanup completion for all requests in list. memory for
 * each request is also freed.
 */
static void
alio64_cleanup(aio_t *aiop, aiocb64_t **cbp, int nent)
{
	int i;
	aio_req_t *reqp;
	aio_result_t *resultp;

	for (i = 0; i < nent; i++) {
		if (cbp[i] == NULL)
			continue;
		resultp = &cbp[i]->aio_resultp;
		mutex_enter(&aiop->aio_mutex);
		reqp = aio_req_done(resultp);
		mutex_exit(&aiop->aio_mutex);
		if (reqp != NULL) {
			aphysio_cleanup(reqp);
			mutex_enter(&aiop->aio_mutex);
			aio_req_free(reqp, aiop);
			mutex_exit(&aiop->aio_mutex);
		}
	}
}

/*
 * write out the results for an aio request that is
 * done.
 */
static int
aioerror64(aioerror64_args_t *uap)
{
	aio_result_t *resultp;
	aio_t *aiop;
	aio_req_t *reqp;
	int retval;

	aiop = curproc->p_aio;
	if (aiop == NULL || uap->cb == NULL)
		return (EINVAL);
	resultp = &uap->cb->aio_resultp;
	mutex_enter(&aiop->aio_mutex);
	retval = aio_req_find(resultp, &reqp);
	mutex_exit(&aiop->aio_mutex);
	if (retval == 0) {
		/*
		 * force a aio_copyout_result()
		 */
		reqp->aio_req_flags &= ~AIO_POLL;
		aphysio_cleanup(reqp);
		mutex_enter(&aiop->aio_mutex);
		aio_req_free(reqp, aiop);
		mutex_exit(&aiop->aio_mutex);
		return (0);
	} else if (retval == 1)
		return (EINPROGRESS);
	else if (retval == 2)
		return (EINVAL);
	return (0);
}

static int
aio_req_setup64(
	aio_req_t	**reqpp,
	aio_t 		*aiop,
	aiocb64_t 	*arg,
	aio_result_t 	*resultp)
{
	aio_req_t 	*reqp;
	struct uio 	*uio;
	sigqueue_t 	*sqp;
	struct sigevent *sigev;
	int error;

	mutex_enter(&aiop->aio_mutex);
	/*
	 * get an aio_reqp from the free list or allocate one
	 * from dynamic memory.
	 */
	if (error = aio_req_alloc(&reqp, resultp)) {
		mutex_exit(&aiop->aio_mutex);
		return (error);
	}
	aiop->aio_pending++;
	aiop->aio_outstanding++;
	mutex_exit(&aiop->aio_mutex);
	/*
	 * initialize aio request.
	 */
	reqp->aio_req_flags = AIO_PENDING;
	reqp->aio_req_fd = arg->aio_fildes;
	uio = reqp->aio_req.aio_uio;
	uio->uio_iovcnt = 1;
	uio->uio_iov->iov_base = (caddr_t)arg->aio_buf;
	uio->uio_iov->iov_len = arg->aio_nbytes;
	uio->uio_loffset = arg->aio_offset;
	sigev = &arg->aio_sigevent;
	if ((sigev->sigev_notify == SIGEV_SIGNAL) &&
	    (sigev->sigev_signo > 0 && sigev->sigev_signo < NSIG)) {
		sqp = (sigqueue_t *) kmem_zalloc(sizeof (sigqueue_t),
					    KM_NOSLEEP);
		if (sqp == NULL)
			return (EAGAIN);
		sqp->sq_func = NULL;
		sqp->sq_next = NULL;
		sqp->sq_info.si_code = SI_QUEUE;
		sqp->sq_info.si_pid = curproc->p_pid;
		sqp->sq_info.si_uid = curproc->p_cred->cr_uid;
		sqp->sq_info.si_signo = sigev->sigev_signo;
		sqp->sq_info.si_value.sival_int =
					sigev->sigev_value.sival_int;
		reqp->aio_req_sigqp = sqp;
	} else
		reqp->aio_req_sigqp = NULL;
	*reqpp = reqp;
	return (0);
}

#endif
