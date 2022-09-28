/*	Copyright (c) 1993 SMI	*/
/*	 All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)sem.c	1.5	95/11/22	SMI"

#ifdef __STDC__
#pragma weak	sem_open = _sem_open
#pragma weak	sem_close = _sem_close
#pragma weak	sem_unlink = _sem_unlink
#pragma weak	sem_init = _sem_init
#pragma weak	sem_destroy = _sem_destroy
#pragma weak	sem_wait = _sem_wait
#pragma weak	sem_trywait = _sem_trywait
#pragma weak	sem_post = _sem_post
#pragma weak	sem_getvalue = _sem_getvalue
#pragma weak	_sema_init
#pragma weak	_sema_destroy
#pragma weak	_sema_post
#pragma weak	_sema_trywait
#pragma weak	_sema_wait_cancel
#endif	/* __STDC__ */

#include "synonyms.h"
#include <sys/types.h>
#include <semaphore.h>
#include <synch.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "pos4obj.h"

typedef	struct	semaddr {
	struct	semaddr	*sad_next;	/* next in the link */
	char		sad_name[PATH_MAX + 1]; /* name of sem object */
	sem_t		*sad_addr;	/* mmapped address of semaphore */
	ino_t		sad_inode;	/* inode # of the mmapped file */
} semaddr_t;

static	semaddr_t	*semheadp = NULL;
static	mutex_t		semlock = DEFAULTMUTEX;	/* mutex to protect link list */

sem_t *
_sem_open(const char *name, int oflag, /* mode_t mode, int value */ ...)
{
	va_list	ap;
	mode_t	crmode;
	sem_t	*sem;
	struct	stat statbuf;
	semaddr_t *next;
	int	fd;
	int	lfd;
	int	err;
	int	cr_flag;
	uint_t	value;


	/* acquire semaphore lock to have atomic operation */
	if ((lfd = __pos4obj_lock(name, SEM_LOCK_TYPE)) < 0)
		return (SEM_FAILED);

	va_start(ap, oflag);
	/* modify oflag to have RDWR and filter CREATE mode only */
	oflag = (oflag & (O_CREAT|O_EXCL)) | (O_RDWR);
	if ((oflag & O_CREAT) != 0) {
		crmode = va_arg(ap, mode_t);
		value = va_arg(ap, uint_t);
	}
	va_end(ap);

	if ((fd = __pos4obj_open(name, SEM_DATA_TYPE,
				oflag, crmode, &cr_flag)) < 0)
		goto out;

	if (cr_flag)
		cr_flag = DFILE_CREATE | DFILE_OPEN;
	else
		cr_flag = DFILE_OPEN;

	/* find out inode # for the opened file */
	if (fstat(fd, &statbuf) < 0)
		goto out;

	/* if created, acquire total_size in the file */
	if ((cr_flag & DFILE_CREATE) != 0) {
		if (ftruncate(fd, sizeof (sem_t)) < 0)
			goto out;

	} else {
		_mutex_lock(&semlock);
		/*
		 * if this semaphore has already been opened, inode
		 * will indicate then return the same semaphore address
		 */
		for (next = semheadp; next != NULL; next = next->sad_next) {
			if (statbuf.st_ino == next->sad_inode &&
				strcmp(name, next->sad_name) == 0) {

				__close_nc(fd);
				_mutex_unlock(&semlock);
				__pos4obj_unlock(lfd);
				return ((sem_t *)next->sad_addr);
			}
		}
		_mutex_unlock(&semlock);
	}


	/* new sem descriptor to be allocated and new address to be mapped */
	if ((next = (semaddr_t *)malloc(sizeof (semaddr_t))) == NULL) {
		errno = ENOMEM;
		goto out;
	}

	cr_flag |= ALLOC_MEM;

	sem = (sem_t *)mmap(0, sizeof (sem_t), PROT_READ|PROT_WRITE,
							MAP_SHARED, fd, 0);
	__close_nc(fd);
	cr_flag &= ~DFILE_OPEN;

	if (sem == MAP_FAILED)
		goto out;

	cr_flag |= DFILE_MMAP;

	/* add to the list pointed by semheadp */
	next->sad_next = semheadp;
	semheadp = next;
	next->sad_addr = sem;
	next->sad_inode = statbuf.st_ino;
	strcpy(next->sad_name, name);


	/* initialize it using libthread semaphore init */
	if ((cr_flag & DFILE_CREATE) != 0) {
		if ((err = _sema_init((sema_t *)sem, value,
		    USYNC_PROCESS, 0)) != 0) {
			errno = err;
			goto out;
		}
	}

	__pos4obj_unlock(lfd);
	return (sem);

out:
	err = errno;
	if ((cr_flag & DFILE_OPEN) != 0)
		__close_nc(fd);
	if ((cr_flag & DFILE_CREATE) != 0)
		(void) __pos4obj_unlink(name, SEM_DATA_TYPE);
	if ((cr_flag & ALLOC_MEM) != 0)
		free((caddr_t)next);
	if ((cr_flag & DFILE_MMAP) != 0)
		munmap((caddr_t)sem, sizeof (sem_t));

	errno = err;
	__pos4obj_unlock(lfd);
	return (SEM_FAILED);
}


int
_sem_close(sem_t *sem)
{
	semaddr_t	**next;
	semaddr_t	*freeit;

	_mutex_lock(&semlock);
	for (next = &semheadp; (freeit = *next) != NULL;
	    next = &(freeit->sad_next)) {
		if (freeit->sad_addr == sem) {
			*next = freeit->sad_next;
			free((caddr_t)freeit);
			_mutex_unlock(&semlock);
			return (munmap((caddr_t)sem, sizeof (sem_t)));
		}
	}
	_mutex_unlock(&semlock);
	errno = EINVAL;
	return (-1);
}


int
_sem_unlink(const char *name)
{
	int	lfd;
	int	err;

	if ((lfd = __pos4obj_lock(name, SEM_LOCK_TYPE)) < 0)
		return (-1);

	err =  __pos4obj_unlink(name, SEM_DATA_TYPE);

	__pos4obj_unlink(name, SEM_LOCK_TYPE);
	__pos4obj_unlock(lfd);
	return (err);
}

int
_sem_init(sem_t *sem, int pshared, uint_t value)
{
	int	err;
	char	type = USYNC_THREAD;

	if (pshared != 0)
		type = USYNC_PROCESS;

	/* call to libthread sema_init */
	err = _sema_init((sema_t *)sem, value, type, 0);
	if (err != 0) {
		errno = err;
		return (-1);
	}
	return (0);
}


int
_sem_destroy(sem_t *sem)
{
	int	err;

	/* call to libthread sema_destroy */
	err = _sema_destroy((sema_t *)sem);
	if (err != 0) {
		errno = err;
		return (-1);
	}
	return (0);
}


int
_sem_post(sem_t *sem)
{
	int	err;

	/* call to libthread sema_post */
	err = _sema_post((sema_t *)sem);
	if (err != 0) {
		errno = err;
		return (-1);
	}
	return (0);
}


int
_sem_wait(sem_t *sem)
{
	int	err;

	/* call to libthread sema_wait */
	err = _sema_wait_cancel((sema_t *)sem);
	if (err != 0) {
		errno = err;
		return (-1);
	}
	return (0);
}


int
_sem_trywait(sem_t *sem)
{
	int	err;

	/* call to libthread sema_trywait */
	err = _sema_trywait((sema_t *)sem);
	if (err != 0) {
		if (err == EBUSY)
			errno = EAGAIN;
		else
			errno = err;
		return (-1);
	}
	return (0);
}


int
_sem_getvalue(sem_t *sem, int *sval)
{
	*sval = sem->sem_count;
	return (0);
}
