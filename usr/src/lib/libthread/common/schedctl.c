/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)schedctl.c	1.5	96/08/05 SMI"

#include "libthread.h"
#include <unistd.h>
#include <door.h>
#include <errno.h>

u_int	_sc_dontfork = 0;
cond_t	_sc_dontfork_cv = DEFAULTCV;
mutex_t	_sc_lock = DEFAULTMUTEX;

static door_info_t sc_info;
static int	sc_did = -1;
static u_int	sc_flags = 0;

/*
 * sc_list is a list of threads with non-NULL t_lwpdata pointers.  We
 * keep track of this so we can clear the pointers in the child of a
 * fork.  The list is protected by _schedlock.
 */
static uthread_t *sc_list = NULL;

extern void (*__door_server_func)(door_info_t *);
extern void (*__thr_door_server_func)(door_info_t *);
static void	_sc_door_create_server(door_info_t *);
static void	_sc_door_func(void);
static void	_schedctl_start(void);
extern int	_door_create(void (*)(), void *, u_int);
extern int	_door_info(int, door_info_t *);
extern int	_door_bind(int);
extern int	_door_return(char *, size_t, door_desc_t *, size_t, caddr_t);
extern int	_door_unbind(void);

extern pid_t	__door_create_pid;


void
_sc_init()
{
	/*
	 * One-time-only setup for scheduler activations.  This is called
	 * when t0 is initialized and from the child of a fork1.
	 */
	sc_flags = SC_STATE | SC_BLOCK;
	__thr_door_server_func = _sc_door_create_server;
	sc_did = _door_create(_schedctl_start, NULL, DOOR_PRIVATE);
	if (sc_did >= 0) {
		__door_create_pid = getpid();
		if (_door_info(sc_did, &sc_info) == 0) {
			_new_lwp(NULL, (void (*)())_sc_door_func, 1);
		}
	}
	_sc_setup();
}

void
_sc_setup()
{
	sc_shared_t	*addr;
	uint_t		flags;
	uthread_t	*t = curthread;
	door_info_t	info;

	if (ISBOUND(t) && !IDLETHREAD(t))
		flags = SC_STATE;
	else
		flags = sc_flags;

	/*
	 * Check if the door id has been messed with.
	 */
	if (sc_did >= 0 && _door_info(sc_did, &info) != 0 ||
	    info.di_uniquifier != sc_info.di_uniquifier) {
		sc_did = -1;
	}

	/*
	 * Need protect against fork().  We don't want to get an address
	 * back from _lwp_schedctl, then fork, and have a bad address in
	 * the child.  We use a counter and a cv so we don't have to hold
	 * _schedlock across the system call.
	 */
	_lmutex_lock(&_sc_lock);
	++_sc_dontfork;
	_lmutex_unlock(&_sc_lock);

	if (_lwp_schedctl(flags, sc_did, &addr) != 0)
		t->t_lwpdata = NULL;		/* error */
	else
		t->t_lwpdata = addr;

	_lmutex_lock(&_sc_lock);
	if (t->t_lwpdata != NULL) {
		/*
		 * Add thread to list of threads with non-NULL
		 * t_lwpdata pointers.
		 */
		_sched_lock();
		if (sc_list == NULL)
			sc_list = t->t_scforw = t->t_scback = t;
		else {
			t->t_scforw = sc_list;
			t->t_scback = sc_list->t_scback;
			sc_list = t;
			t->t_scback->t_scforw = t;
			t->t_scforw->t_scback = t;
		}
		_sched_unlock();
	}
	if (--_sc_dontfork == 0)
		_cond_signal(&_sc_dontfork_cv);	/* wake up waiting forkers */
	_lmutex_unlock(&_sc_lock);
}

/*
 * Substitute new thread for current one in list of threads with non-NULL
 * t_lwpdata pointers.
 */
void
_sc_switch(uthread_t *next)
{
	uthread_t	*t = curthread;

	ASSERT(MUTEX_HELD(&_schedlock));
	ASSERT(t != next);

	next->t_lwpdata = t->t_lwpdata;
	t->t_lwpdata = NULL;
	if (t->t_scforw != t) {
		next->t_scforw = t->t_scforw;
		next->t_scforw->t_scback = next;
	} else {
		next->t_scforw = next;
	}
	if (t->t_scback != t) {
		next->t_scback = t->t_scback;
		next->t_scback->t_scforw = next;
	} else {
		next->t_scback = next;
	}
	if (sc_list == t)
		sc_list = next;
}

/*
 * Remove thread from list.
 */
void
_sc_exit()
{
	uthread_t *t = curthread;

	ASSERT(MUTEX_HELD(&_schedlock));
	t->t_lwpdata = NULL;
	if (t->t_scforw == t)
		sc_list = NULL;
	else {
		t->t_scforw->t_scback = t->t_scback;
		t->t_scback->t_scforw = t->t_scforw;
		if (sc_list == t)
			sc_list = t->t_scforw;
	}
}

/*
 * Cleanup on fork or fork1.
 */
void
_sc_cleanup()
{
	uthread_t *t, *first;

	ASSERT(MUTEX_HELD(&_schedlock));

	/* close old door */
	if (sc_did != -1)
		close(sc_did);

	sc_did = -1;

	/*
	 * Reset pointers to shared data.  Thread list is protected
	 * by _schedlock.
	 */
	if (sc_list) {
		first = t = sc_list;
		do {
			t->t_lwpdata = NULL;
		} while ((t = t->t_scforw) != first);
		sc_list = NULL;
	}
}

static void
_sc_door_func()
{
	door_info_t info;

	if (sc_did >= 0 && _door_info(sc_did, &info) == 0 &&
	    info.di_uniquifier == sc_info.di_uniquifier &&
	    _door_bind(sc_did) == 0) {
		(void) _door_return(NULL, 0, NULL, 0, NULL);
	} else
		_age();
}

static void
_sc_door_create_server(door_info_t *dip)
{
	if (dip == NULL || sc_did == -1 ||
	    dip->di_uniquifier != sc_info.di_uniquifier)
		(*__door_server_func)(dip);
	else
		_new_lwp(NULL, (void (*)())_sc_door_func, 1);
}

int
__thr_door_unbind()
{
	door_info_t info;

	if (_door_info(DOOR_QUERY, &info) != 0 ||
	    info.di_uniquifier != sc_info.di_uniquifier)
		return (_door_unbind());
	else {
		curthread->t_errno = EBADF;
		return (-1);
	}
}

static void
_schedctl_start()
{
	_sched_lock_nosig();
	if (!_nrunnable || (_nrunnable - _nidlecnt) <= _naging)
		if (_sigwaitingset)
			_sigwaiting_disabled();
	_sched_unlock_nosig();
	_age();
}
