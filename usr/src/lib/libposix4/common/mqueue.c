/*	Copyright (c) 1993 SMI	*/
/*	 All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)mqueue.c	1.11	95/11/22	SMI"


#include <mqueue.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <pthread.h>
#include "mqlib.h"
#include "pos4obj.h"


static void mq_init(mqhdr_t *mqhp);
static int  mq_getmsg(mqhdr_t *mqhp, char *msgp, uint_t *prio);
static void mq_putmsg(mqhdr_t *mqhp, const char *msgp,
					int len, uint_t prio);

mqd_t
mq_open(const char *name, int oflag, /* mode_t mode, mq_attr *attr */ ...)
{
	va_list	ap;
	mode_t	mode;
	struct	mq_attr *attr;
	int	lfd;
	int	fd;
	int	err;
	int	cr_flag = 0;
	ulong_t	total_size;
	ulong_t	msgsize;
	long	maxmsg;
	ulong_t	temp;
	mqdes_t	*mqdp;
	mqhdr_t	*mqhp;
	struct mq_dn	*mqdnp;


	/* acquire MSGQ lock to have atomic operation */
	if ((lfd = __pos4obj_lock(name, MQ_LOCK_TYPE)) < 0)
		return ((mqd_t)-1);

	va_start(ap, oflag);
	/* filter oflag to have READ/WRITE/CREATE modes only */
	oflag = oflag & (O_RDONLY|O_WRONLY|O_RDWR|O_CREAT|O_EXCL|O_NONBLOCK);
	if ((oflag & O_CREAT) != 0) {
		mode = va_arg(ap, mode_t);
		attr = va_arg(ap, struct mq_attr *);
	}
	va_end(ap);

	if ((fd = __pos4obj_open(name, MQ_PERM_TYPE, oflag,
	    mode, &cr_flag)) < 0)
		goto out;

	/* closing permission file */
	close(fd);

	/* Try to open/create data file */
	if (cr_flag) {
		cr_flag = PFILE_CREATE;
		if (attr == NULL) {
			maxmsg = MQ_MAXMSG;
			msgsize = MQ_MAXSIZE;
		} else if ((attr->mq_maxmsg <= 0) ||
					(attr->mq_msgsize <= 0)) {
			goto out;

		} else {
			maxmsg = attr->mq_maxmsg;
			msgsize = attr->mq_msgsize;
		}

		/* adjust for message size at word boundary */
		temp = (msgsize + sizeof (char *) - 1) & MQ_SIZEMASK;

		total_size = sizeof (mqhdr_t) +
			maxmsg * (temp + sizeof (msghdr_t)) +
			2 * MQ_MAXPRIO * sizeof (msghdr_t *) +
			BT_BITOUL(MQ_MAXPRIO - 1) + 1;

		/*
		 * data file is opened with read/write to those
		 * who have read or write permission
		 */
		mode = mode | (mode & 0444) >> 1 | (mode & 0222) << 1;
		if ((fd = __pos4obj_open(name, MQ_DATA_TYPE,
			(O_RDWR|O_CREAT|O_EXCL), mode, &err)) < 0)
			goto out;

		cr_flag |= DFILE_CREATE | DFILE_OPEN;

		/* force permissions to avoid umask effect */
		if (fchmod(fd, mode) < 0)
			goto out;

		if (ftruncate(fd, total_size) < 0)
			goto out;

	} else {
		if ((fd = __pos4obj_open(name, MQ_DATA_TYPE,
					O_RDWR, 0666, &err)) < 0)
			goto out;

		cr_flag = DFILE_OPEN;
		if (read(fd, &total_size, sizeof (ulong_t)) <= 0)
			goto out;

		/* Message queue has not been initialized yet */
		if (total_size == 0) {
			errno = ENOENT;
			goto out;
		}
	}

	if ((mqdp = (mqdes_t *)malloc(sizeof (mqdes_t))) == NULL) {
		errno = ENOMEM;
		goto out;
	}
	cr_flag |= ALLOC_MEM;

	if ((mqhp = (mqhdr_t *)mmap(0, total_size, PROT_READ|PROT_WRITE,
	    MAP_SHARED, fd, 0)) == MAP_FAILED)
		goto out;
	cr_flag |= DFILE_MMAP;

	/* closing data file */
	close(fd);
	cr_flag &= ~DFILE_OPEN;

	/*
	 * create, unlink, size, mmap, and close description file
	 * all for a flag word in anonymous shared memory
	 */
	if ((fd = __pos4obj_open(name, MQ_DSCN_TYPE, O_RDWR | O_CREAT,
	    0666, &err)) < 0)
		goto out;
	cr_flag |= DFILE_OPEN;
	(void) __pos4obj_unlink(name, MQ_DSCN_TYPE);
	if (ftruncate(fd, sizeof (struct mq_dn)) < 0)
		goto out;
	if ((mqdnp = (struct mq_dn *)mmap(0, sizeof (struct mq_dn),
	    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED)
		goto out;
	close(fd);
	cr_flag &= ~DFILE_OPEN;

	/*
	 * we follow the same strategy as filesystem open() routine,
	 * where fcntl.h flags are changed to flags defined in file.h.
	 */
	mqdp->mqd_flags = (oflag - FOPEN) & (FREAD|FWRITE);
	mqdnp->mqdn_flags = (oflag - FOPEN) & (FNONBLOCK);

	/* new message queue requires initialization */
	if ((cr_flag & DFILE_CREATE) != 0) {
		/* message queue header has to be initialized */
		_mutex_init(&mqhp->mq_lock, USYNC_PROCESS, 0);
		_mutex_lock(&mqhp->mq_lock);
		mqhp->mq_maxprio = MQ_MAXPRIO;
		mqhp->mq_maxsz = msgsize;
		mqhp->mq_maxmsg = maxmsg;
		mq_init(mqhp);
		_cond_init(&mqhp->mq_send, USYNC_PROCESS, 0);
		_cond_init(&mqhp->mq_recv, USYNC_PROCESS, 0);
		mqhp->mq_magic = MQ_MAGIC;
		mqhp->mq_totsize = total_size;
		_mutex_unlock(&mqhp->mq_lock);
	}
	mqdp->mqd_mq = mqhp;
	mqdp->mqd_mqdn = mqdnp;
	mqdp->mqd_magic = MQ_MAGIC;
	__pos4obj_unlock(lfd);
	return ((mqd_t)mqdp);

out:
	err = errno;
	if ((cr_flag & DFILE_OPEN) != 0)
		close(fd);
	if ((cr_flag & DFILE_CREATE) != 0)
		(void) __pos4obj_unlink(name, MQ_DATA_TYPE);
	if ((cr_flag & PFILE_CREATE) != 0)
		(void) __pos4obj_unlink(name, MQ_PERM_TYPE);
	if ((cr_flag & ALLOC_MEM) != 0)
		free((void *)mqdp);
	if ((cr_flag & DFILE_MMAP) != 0)
		munmap((void *)mqhp, total_size);
	errno = err;
	__pos4obj_unlock(lfd);
	return (mqd_t)(-1);
}

int
mq_close(mqd_t mqdes)
{
	register mqdes_t *mqdp = (mqdes_t *)mqdes;
	register mqhdr_t *mqhp;
	register struct mq_dn *mqdnp;

	if (mqdp == NULL || (int)mqdp == -1 || mqdp->mqd_magic != MQ_MAGIC) {
		errno = EBADF;
		return (-1);
	}

	mqhp = mqdp->mqd_mq;
	mqdnp = mqdp->mqd_mqdn;

	_mutex_lock(&mqhp->mq_lock);
	if (mqhp->mq_des == mqdp && mqhp->mq_sigid.sn_pid == getpid()) {
		/* Notification is set for this descriptor, remove it */
		__signotify(SN_CANCEL, NULL, &mqhp->mq_sigid);
		mqhp->mq_sigid.sn_pid = 0;
		mqhp->mq_des = 0;
	}
	_mutex_unlock(&mqhp->mq_lock);

	/* Invalidate the descriptor before freeing it */
	mqdp->mqd_magic = 0;
	free(mqdp);

	(void) munmap((caddr_t)mqdnp, sizeof (struct mq_dn));
	return (munmap((caddr_t)mqhp, mqhp->mq_totsize));
}

int
mq_unlink(const char *name)
{
	int lfd;
	int err;

	if ((lfd = __pos4obj_lock(name, MQ_LOCK_TYPE)) < 0)
		return (-1);

	err = __pos4obj_unlink(name, MQ_PERM_TYPE);

	if (err == 0 || (err == -1 && errno == EEXIST)) {
		errno = 0;
		err = __pos4obj_unlink(name, MQ_DATA_TYPE);
	}

	__pos4obj_unlink(name, MQ_LOCK_TYPE);
	__pos4obj_unlock(lfd);
	return (err);

}

static void
__mq_send_cleanup(mqhdr_t *mqhp)
{
	_mutex_unlock(&mqhp->mq_lock);
}

int
mq_send(mqd_t mqdes, const char *msg_ptr, size_t msg_len, uint_t msg_prio)
{
	register mqdes_t *mqdp = (mqdes_t *)mqdes;
	register mqhdr_t *mqhp;
	int wait_status;

	if (mqdp == NULL || (int)mqdp == -1 ||
		(mqdp->mqd_magic != MQ_MAGIC) ||
		((mqdp->mqd_flags & FWRITE) == 0)) {
		errno = EBADF;
		return (-1);
	}

	mqhp = mqdp->mqd_mq;

	if (msg_prio >= mqhp->mq_maxprio) {
		errno = EINVAL;
		return (-1);
	}
	if (msg_len > mqhp->mq_maxsz) {
		errno = EMSGSIZE;
		return (-1);
	}

	_mutex_lock(&mqhp->mq_lock);
	while (mqhp->mq_count == mqhp->mq_maxmsg) {

		/* Non block case, return error */
		if ((mqdp->mqd_mqdn->mqdn_flags & O_NONBLOCK) != 0) {
			_mutex_unlock(&mqhp->mq_lock);
			errno = EAGAIN;
			return (-1);
		}
		pthread_cleanup_push(__mq_send_cleanup, mqhp);
		wait_status = _cond_wait(&mqhp->mq_send, &mqhp->mq_lock);
		pthread_cleanup_pop(0);
		if (wait_status == EINTR) {
			_mutex_unlock(&mqhp->mq_lock);
			errno = EINTR;
			return (-1);
		}
	}
	mq_putmsg(mqhp, msg_ptr, msg_len, msg_prio);
	mqhp->mq_count++;

	/*
	 * If notification is registered and no of messages in the queue
	 * are one more than no of receiver then queue is assumed to be
	 * non-empty
	 */
	if ((mqhp->mq_sigid.sn_pid != 0) &&
			(mqhp->mq_count == (mqhp->mq_waiters + 1))) {
		/*
		 * send signal to the process which registered it
		 */
		__signotify(SN_SEND, NULL, &mqhp->mq_sigid);
		mqhp->mq_sigid.sn_pid = 0;
		mqhp->mq_des = 0;
	}

	/* wake up any receiver, if it is blocked */
	_cond_signal(&mqhp->mq_recv);

	_mutex_unlock(&mqhp->mq_lock);

	return (0);
}

static void
__mq_receive_cleanup(mqhdr_t *mqhp)
{
	mqhp->mq_waiters--;
	_mutex_unlock(&mqhp->mq_lock);
}

ssize_t
mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len, uint_t *msg_prio)
{
	register mqdes_t *mqdp = (mqdes_t *)mqdes;
	register mqhdr_t *mqhp;
	ssize_t	msg_size;
	int wait_status;

	if (mqdp == NULL || (int)mqdp == -1 ||
		(mqdp->mqd_magic != MQ_MAGIC) ||
		((mqdp->mqd_flags & FREAD) == 0)) {
		errno = EBADF;
		return (ssize_t)(-1);
	}

	mqhp = mqdp->mqd_mq;

	if (msg_len < mqhp->mq_maxsz) {
		errno = EMSGSIZE;
		return (ssize_t)(-1);
	}

	_mutex_lock(&mqhp->mq_lock);
	while (mqhp->mq_count == 0) {

		/* Non block case, return error */
		if ((mqdp->mqd_mqdn->mqdn_flags & O_NONBLOCK) != 0) {
			_mutex_unlock(&mqhp->mq_lock);
			errno = EAGAIN;
			return (ssize_t)(-1);
		}
		mqhp->mq_waiters++;
		pthread_cleanup_push(__mq_receive_cleanup, mqhp);
		wait_status = _cond_wait(&mqhp->mq_recv, &mqhp->mq_lock);
		pthread_cleanup_pop(0);
		mqhp->mq_waiters--;
		if (wait_status == EINTR) {
			_mutex_unlock(&mqhp->mq_lock);
			errno = EINTR;
			return (ssize_t)(-1);
		}
	}
	msg_size = mq_getmsg(mqhp, msg_ptr, msg_prio);
	mqhp->mq_count--;

	/* wake up any sender, if it is blocked */
	_cond_signal(&mqhp->mq_send);

	_mutex_unlock(&mqhp->mq_lock);

	return (msg_size);
}

int
mq_notify(mqd_t mqdes, const struct sigevent *notification)
{
	register mqdes_t *mqdp = (mqdes_t *)mqdes;
	register mqhdr_t *mqhp;
	siginfo_t mq_siginfo;
	int ret = 0;

	if (mqdp == NULL || (int)mqdp == -1 || mqdp->mqd_magic != MQ_MAGIC) {
		errno = EBADF;
		return (-1);
	}

	mqhp = mqdp->mqd_mq;

	_mutex_lock(&mqhp->mq_lock);

	if (notification == NULL) {
		if (mqhp->mq_des == mqdp &&
				mqhp->mq_sigid.sn_pid == getpid()) {
			/*
			 * Remove signotify_id if Q is registered with
			 * this process
			 */
			__signotify(SN_CANCEL, NULL, &mqhp->mq_sigid);
			mqhp->mq_sigid.sn_pid = 0;
			mqhp->mq_des = 0;
		} else {
			/*
			 * if registered with another process or mqdes
			 */
			_mutex_unlock(&mqhp->mq_lock);
			errno = EBUSY;
			ret = -1;
		}
	} else {
		/*
		 * Register notification with this process.
		 */

		mq_siginfo.si_signo = notification->sigev_signo;
		mq_siginfo.si_value = notification->sigev_value;
		mq_siginfo.si_code = SI_MESGQ;

		/*
		 * Either notification is not present, or if
		 * notification is already present, but the process
		 * which registered notification does not exist then
		 * kernel can register notification for current process.
		 */

		if (__signotify(SN_PROC, &mq_siginfo, &mqhp->mq_sigid) < 0) {
			_mutex_unlock(&mqhp->mq_lock);
			errno = EBUSY;
			ret = -1;
		}
		mqhp->mq_des = mqdp;
	}

	_mutex_unlock(&mqhp->mq_lock);

	return (ret);
}

int
mq_setattr(mqd_t mqdes, const struct mq_attr *mqstat, struct mq_attr *omqstat)
{

	register mqdes_t *mqdp = (mqdes_t *)mqdes;
	register mqhdr_t *mqhp;
	uint_t	flag = 0;

	if (mqdp == 0 || (int)mqdp == -1 || mqdp->mqd_magic != MQ_MAGIC) {
		errno = EBADF;
		return (-1);
	}

	/* store current attributes */
	if (omqstat != NULL) {
		mqhp = mqdp->mqd_mq;

		omqstat->mq_flags = mqdp->mqd_mqdn->mqdn_flags;
		omqstat->mq_maxmsg = mqhp->mq_maxmsg;
		omqstat->mq_msgsize = mqhp->mq_maxsz;
		omqstat->mq_curmsgs = mqhp->mq_count;
	}

	/* set description attributes */
	if ((mqstat->mq_flags & O_NONBLOCK) != 0)
		flag = FNONBLOCK;
	mqdp->mqd_mqdn->mqdn_flags = flag;

	return (0);
}

int
mq_getattr(mqd_t mqdes, struct mq_attr *mqstat)
{
	register mqdes_t *mqdp = (mqdes_t *)mqdes;
	register mqhdr_t *mqhp;

	if (mqdp == NULL || (int)mqdp == -1 || mqdp->mqd_magic != MQ_MAGIC) {
		errno = EBADF;
		return (-1);
	}

	mqhp = mqdp->mqd_mq;

	mqstat->mq_flags = mqdp->mqd_mqdn->mqdn_flags;
	mqstat->mq_maxmsg = mqhp->mq_maxmsg;
	mqstat->mq_msgsize = mqhp->mq_maxsz;
	mqstat->mq_curmsgs = mqhp->mq_count;

	return (0);
}


#define	MQ_PTR(m, n)	((msghdr_t *)((int)m + (int)n))
#define	HEAD_PTR(m, n)	(msghdr_t **)((int)m + (int)(m->mq_headpp + n))
#define	TAIL_PTR(m, n)	(msghdr_t **)((int)m + (int)(m->mq_tailpp + n))


static void
mq_init(mqhdr_t *mqhp)
{
	int	i;
	ulong_t	temp;
	msghdr_t *currentp;
	msghdr_t *nextp;

	/*
	 * mqhp->mq_count = 0;
	 * mqhp->mq_flag = 0;
	 * mqhp->mq_waiters = 0;
	 * mqhp->mq_curmaxprio = 0;
	 * mqhp->mq_des = NULL;
	 * mqhp->mq_sigid.sn_pid = 0;
	 * mqhp->mq_sigid.sn_index = 0;
	 *
	 * Since message Q header is created in MSGQ file using
	 * ftruncate, it will be initialized to ZERO by kernel
	 * so we do not have to initialize above fields
	 *
	 * In the message queue, we store the relative offset
	 * since pointers will be shared among different processes.
	 */

	mqhp->mq_headpp = (msghdr_t **)sizeof (mqhdr_t);
	mqhp->mq_tailpp = mqhp->mq_headpp + mqhp->mq_maxprio;
	mqhp->mq_maskp = (ulong_t *)(mqhp->mq_tailpp + mqhp->mq_maxprio);
	mqhp->mq_freep = (msghdr_t *)(mqhp->mq_maskp +
					BT_BITOUL(mqhp->mq_maxprio - 1) + 1);

	currentp = mqhp->mq_freep;
	MQ_PTR(mqhp, currentp)->msg_next = (msghdr_t *)0;

	i = 1;
	temp = (mqhp->mq_maxsz + sizeof (char *) - 1) & MQ_SIZEMASK;
	while (i++ < mqhp->mq_maxmsg) {
		nextp = (msghdr_t *)((int)(currentp + 1) + temp);
		MQ_PTR(mqhp, currentp)->msg_next = nextp;
		MQ_PTR(mqhp, nextp)->msg_next = (msghdr_t *)0;
		currentp = nextp;
	}
}

static	int	hibit(ulong_t i);
static	void	bt_gethighbit(ulong_t *, int, int *);

static int
mq_getmsg(mqhdr_t *mqhp, char *msgp, uint_t *msg_prio)
{

	msghdr_t *currentp;
	msghdr_t *curbuf;
	msghdr_t **headpp;
	msghdr_t **tailpp;
	int	hi;
	ulong_t	*bitmask;

	/* find out head/tail pointer for the 'maxprio' based queue */
	headpp = HEAD_PTR(mqhp, mqhp->mq_curmaxprio);
	tailpp = TAIL_PTR(mqhp, mqhp->mq_curmaxprio);

	if (msg_prio != NULL)
		*msg_prio = mqhp->mq_curmaxprio;

	/* XXX - headp should not be zero */

	/* remove the message from the prio based queue */
	currentp = *headpp;
	curbuf = MQ_PTR(mqhp, currentp);

	if ((*headpp = curbuf->msg_next) == NULL) {
		*tailpp = NULL;

		bitmask = (ulong_t *)(MQ_PTR(mqhp, mqhp->mq_maskp));
		/* this max priority is over, search for next lower */
		BT_CLEAR(bitmask, mqhp->mq_curmaxprio);
		bt_gethighbit(bitmask, BT_BITOUL(mqhp->mq_curmaxprio), &hi);
		mqhp->mq_curmaxprio = hi;
	}

	/* copy the buffer up to no. of bytes present in message */
	memcpy(msgp, (char *)(curbuf + 1), curbuf->msg_len);

	/* put back the current buffer in free pool */
	curbuf->msg_next = mqhp->mq_freep;
	mqhp->mq_freep = currentp;

	return (curbuf->msg_len);
}



static void
mq_putmsg(mqhdr_t *mqhp, const char *msgp, int len, uint_t prio)
{
	msghdr_t *currentp;
	msghdr_t *curbuf;
	msghdr_t **headpp;
	msghdr_t **tailpp;
	ulong_t *bitmask;

	/* XXX - freep should not be zero, as count is < maxmsg */
	currentp = mqhp->mq_freep;
	curbuf = MQ_PTR(mqhp, currentp);

	/* remove the current message from free list */
	mqhp->mq_freep = curbuf->msg_next;
	curbuf->msg_next = NULL;

	/* copy the buffer */
	memcpy((char *)(curbuf + 1), msgp, len);
	curbuf->msg_len = len;

	/* find out head/tail pointer for the 'prio' based queue */
	headpp = HEAD_PTR(mqhp, prio);
	tailpp = TAIL_PTR(mqhp, prio);

	if (*tailpp == NULL) {
		/* This is the first message to be put in this queue */
		*headpp = currentp;
		*tailpp = currentp;

		/* set priority mask, and curmaxprio if highest */
		bitmask = (ulong_t *)(MQ_PTR(mqhp, mqhp->mq_maskp));
		BT_SET(bitmask, prio);

		if (prio > mqhp->mq_curmaxprio)
			mqhp->mq_curmaxprio = prio;
	} else {
		MQ_PTR(mqhp, *tailpp)->msg_next = currentp;
		*tailpp = currentp;
	}


}

/*
 * Find highest one bit set.
 * Returns bit number of highest bit that is set, otherwise returns 0.
 * High order bit is 31.
 */
static int
hibit(ulong_t word)
{
	unsigned int	h, i;
	ulong_t		mask;

	h = 0;
	mask = (ulong_t)(~0);
	for (i = BT_NBIPUL/2; i != 0; i >>= 1) {
		mask <<= i;
		if (word & mask) {
			h += i;
		} else {
			word <<= i;
		}
	}
	return (h);
}

/*
 * COPIED FROM uts/common/os/bitmap.c
 * Find highest order bit that is on, and is within or below
 * the word specified by wx.
 */
void
bt_gethighbit(mapp, wx, bitposp)
	register ulong_t	*mapp;
	register int		wx;
	int			*bitposp;
{
	register ulong_t	word;

	while ((word = mapp[wx]) == 0) {
		wx--;
		if (wx < 0) {
			*bitposp = -1;
			return;
		}
	}
	*bitposp = wx << BT_ULSHIFT | hibit(word);
}
