/*	Copyright (c) 1996 Sun Microsystems, Inc.	*/
/*	All Rights Reserved.				*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)mutex.c	1.56	96/07/24 SMI"

#ifdef __STDC__

#pragma weak mutex_init = _mutex_init
#pragma weak mutex_destroy = _mutex_destroy
#pragma weak mutex_lock = _mutex_lock
#pragma weak mutex_unlock = _mutex_unlock
#pragma weak mutex_trylock = _mutex_trylock

#pragma weak pthread_mutex_destroy = _mutex_destroy
#pragma weak pthread_mutex_lock = _mutex_lock
#pragma weak pthread_mutex_unlock = _mutex_unlock
#pragma weak pthread_mutex_trylock = _mutex_trylock
#pragma weak _pthread_mutex_destroy = _mutex_destroy
#pragma weak _pthread_mutex_lock = _mutex_lock
#pragma weak _pthread_mutex_unlock = _mutex_unlock
#pragma weak _pthread_mutex_trylock = _mutex_trylock

#pragma	weak _ti_mutex_lock = _mutex_lock
#pragma weak _ti_pthread_mutex_lock = _mutex_lock
#pragma	weak _ti_mutex_unlock = _mutex_unlock
#pragma	weak _ti_pthread_mutex_unlock = _mutex_unlock
#pragma weak _ti_mutex_destroy = _mutex_destroy
#pragma weak _ti_pthread_mutex_destroy = _mutex_destroy
#pragma	weak _ti_mutex_held = _mutex_held
#pragma	weak _ti_mutex_init = _mutex_init
#pragma	weak _ti_mutex_trylock = _mutex_trylock
#pragma	weak _ti_pthread_mutex_trylock = _mutex_trylock

#endif /* __STDC__ */

#include "libthread.h"
#include "tdb_agent.h"

#if defined(UTRACE) || defined(ITRACE)
#define	TRACE_NAME(x) (((x)->type & TRACE_TYPE) ? (x)->name : "<noname>")
#include <string.h>
#endif

/*
 * The number of mutex types - used to validate type in mutex_lock/unlock.
 */
#define	NO_OF_TYPES 2

#if defined(UTRACE) || defined(ITRACE)
#undef NO_OF_TYPES
#define	NO_OF_TYPES 3
#endif

/*
 * Mutex operations for variant types
 */
struct mutex_op {
	void	(*m_lock)();
	void	(*m_unlock)();
};

static	void _mutex_adaptive_lock();
static	void _mutex_adaptive_unlock();
static	void _mutex_lwp_lock();
static	void _mutex_lwp_unlock();

extern	void ___lwp_mutex_unlock();
extern	int _mutex_unlock_asm();

struct thread *_lock_owner(mutex_t *);

static struct mutex_op _mutex_op[] = {
	{
		_mutex_adaptive_lock,		/* 0 process-private */
		_mutex_adaptive_unlock,
	},
	{
		_mutex_lwp_lock,		/* 1 process-shared */
		_mutex_lwp_unlock,
	}
#if defined(UTRACE) || defined(ITRACE)
	/* */, {
		_mutex_adaptive_lock,		/* 3 suspend traced */
		_mutex_adaptive_unlock,
	},
	{
		_mutex_lwp_lock,		/* 4 shared traced */
		_mutex_lwp_unlock,
	}
#endif
};

/*
 * Check if a certain mutex is locked.
 */
_mutex_held(mutex_t *mp)
{
	return (LOCK_HELD(&mp->mutex_lockw));
}

/*ARGSUSED2*/
int
_mutex_init(mutex_t *mp, int type, void	*arg)
{
	int rc = 0;

	/* When more types are added, replace with bitmask validations */
	if (type != USYNC_THREAD && type != USYNC_PROCESS)
		return (EINVAL);

	if (type & USYNC_PROCESS) {
		mp->mutex_type = USYNC_PROCESS;
	} else
		mp->mutex_type = type;
	mp->mutex_magic = MUTEX_MAGIC;
	mp->mutex_waiters = 0;
	_lock_clear_adaptive(mp);
	if (__tdb_attach_stat != TDB_NOT_ATTACHED)
		_tdb_sync_obj_register((caddr_t) mp, MUTEX_MAGIC);

#if defined(UTRACE) || defined(ITRACE)
	if (arg) {
		mp->mutex_type |= TRACE_TYPE;
	}
#endif
	return (rc);

}

/*ARGSUSED*/
int
_mutex_destroy(mutex_t *mp)
{
	mp->mutex_magic = 0;
	_tdb_sync_obj_deregister((caddr_t) mp);
	return (0);
}

int
_mutex_lock(mutex_t *mp)
{
	TRACE_3(UTR_FAC_THREAD_SYNC, UTR_MUTEX_LOCK_START,
	    "mutex_lock start:name %s, addr 0x%x, type 0x%x",
	    TRACE_NAME(mp), mp, mp->mutex_type);
	if (!_lock_try_adaptive(mp))
		(*_mutex_op[(mp->mutex_type) % NO_OF_TYPES].m_lock)(mp);
	TRACE_3(UTR_FAC_THREAD_SYNC, UTR_MUTEX_LOCK_END,
	    "mutex_lock end:name %s, addr 0x%x, type 0x%x",
	    TRACE_NAME(mp), mp, mp->mutex_type);
	TRACE_3(UTR_FAC_THREAD_SYNC, UTR_CS_START,
	    "critical_section start:name %s, addr 0x%x, type 0x%x",
	    TRACE_NAME(mp), mp, mp->mutex_type);
	return (0);
}

int
_mutex_unlock(mutex_t *mp)
{
	TRACE_3(UTR_FAC_THREAD_SYNC, UTR_CS_END,
	    "critical section end:name %s, addr 0x%x, type 0x%x",
	    TRACE_NAME(mp), mp, mp->mutex_type);
	TRACE_3(UTR_FAC_THREAD_SYNC, UTR_MUTEX_UNLOCK_START,
	    "mutex_unlock start:name %s, addr 0x%x, type 0x%x",
	    TRACE_NAME(mp), mp, mp->mutex_type);
	if (_mutex_unlock_asm(mp) > 0)
		(*_mutex_op[mp->mutex_type % NO_OF_TYPES].m_unlock)(mp);
	TRACE_3(UTR_FAC_THREAD_SYNC, UTR_MUTEX_UNLOCK_END,
	    "mutex_unlock end:name %s, addr 0x%x, type 0x%x",
	    TRACE_NAME(mp), mp, mp->mutex_type);
	return (0);
}

int
_mutex_trylock(mutex_t *mp)
{
	TRACE_3(UTR_FAC_THREAD_SYNC, UTR_MUTEX_TRYLOCK_START,
	    "mutex_trylock start:name %s, addr 0x%x, type 0x%x",
	    TRACE_NAME(mp), mp, mp->mutex_type);
	if (_lock_try_adaptive(mp)) {
		return (0);
	} else {
		if (__td_event_report(curthread, TD_LOCK_TRY)) {
			curthread->t_td_evbuf->eventnum = TD_LOCK_TRY;
			tdb_event_lock_try();
		}
		return (EBUSY);
	}
}

void
_mutex_sema_unlock(mutex_t *mp)
{
	u_char waiters;

	ASSERT(curthread->t_nosig >= 2);
	if (_mutex_unlock_asm(mp) > 0) {
		if (_t_release((caddr_t)mp, &waiters, 0) > 0)
			mp->mutex_waiters = waiters;
	}
	_sigon();
}

void
_mutex_op_lock(mutex_t *mp)
{
	(*_mutex_op[mp->mutex_type].m_lock)(mp);
}

void
_mutex_op_unlock(mutex_t *mp)
{
	(*_mutex_op[mp->mutex_type].m_unlock)(mp);
}

#define	MUTEX_MAX_SPIN	100		/* how long to spin before blocking */
#define	MUTEX_CHECK_FREQ 10		/* frequency of checking lwp state */

static void
_mutex_adaptive_lock(mutex_t *mp)
{
	u_char waiters;
	struct thread *owner_t;
	short state;
	int spin_count = 0;

	while (!_lock_try_adaptive(mp)) {
		/* only check owner every so often, including first time */
		if ((spin_count++) % MUTEX_CHECK_FREQ == 0) {
			_sched_lock();
			mp->mutex_magic = MUTEX_MAGIC;
			if (__tdb_attach_stat != TDB_NOT_ATTACHED)
				_tdb_sync_obj_register((caddr_t) mp,
				    MUTEX_MAGIC);
			/* look at (real) onproc state of owner */
			owner_t = _lock_owner(mp);
			if (spin_count < MUTEX_MAX_SPIN && owner_t != NULL) {
				if (owner_t->t_state == TS_ONPROC &&
				    owner_t->t_lwpdata != NULL) {
					state = owner_t->t_lwpdata->sc_state;
					if (state == SC_ONPROC) {
						/*
						 * thread and lwp are ONPROC,
						 * so spin for a while
						 */
						_sched_unlock();
						continue;
					}
				}
			}
			/* may be a long wait, better block */
			waiters = mp->mutex_waiters;
			mp->mutex_waiters = 1;
			if (!_lock_try_adaptive(mp)) {
				curthread->t_flag &= ~T_WAITCV;
				_t_block((caddr_t)mp);
				_sched_unlock_nosig();
				_swtch(0);
				_sigon();
				spin_count = 0;
				continue;
			}
			mp->mutex_waiters = waiters;	/* got the lock */
			_sched_unlock();
			return;
		}
	}
}

static void
_mutex_adaptive_unlock(mutex_t *mp)
{
	u_char waiters;

	_sched_lock();
	if (_t_release((caddr_t)mp, &waiters, 0) > 0)
		mp->mutex_waiters = waiters;
	_sched_unlock();
}

static void
_mutex_lwp_lock(mutex_t *mp)
{
	mp->mutex_magic = MUTEX_MAGIC;
	if (__tdb_attach_stat != TDB_NOT_ATTACHED)
		_tdb_sync_obj_register((caddr_t) mp, MUTEX_MAGIC);
	_lwp_mutex_lock(mp);

	/* for libthread_db's benefit */
	*(uthread_t **) &mp->mutex_owner = curthread;
}

static void
_mutex_lwp_unlock(mutex_t *mp)
{
	/*
	 * This entry point should really be called something else -
	 * such as  ___lwp_mutex_wakeup(). It is a system call which
	 * does not clear the lock - all it does is to wakeup a waiter,
	 * if any.
	 */
	___lwp_mutex_unlock(mp);
}

/*
 * this is the _mutex_lock() routine that is used internally
 * by the thread's library.
 */
void
_lmutex_lock(mutex_t *mp)
{
	_sigoff();
	_mutex_lock(mp);
}

/*
 * this is the _mutex_trylock() that is used internally by the
 * thread's library.
 */
int
_lmutex_trylock(mutex_t *mp)
{
	_sigoff();
	if (_lock_try_adaptive(mp)) {
		_sigon();
		return (1);
	}
	return (0);
}

/*
 * this is the _mutex_unlock() that is used internally by the
 * thread's library.
 */
void
_lmutex_unlock(mutex_t *mp)
{
	_mutex_unlock(mp);
	_sigon();
}

#if defined(UTRACE) || defined(ITRACE)

/*
 * The following are called from trace() and should be kept identical
 * to the aboved defined mutex_lock() and mutex_unlock(). The following
 * routines have no calls to trace() - to prevent infinite recursion.
 */

int
trace_mutex_lock(mutex_t *mp)
{
	u_char waiters;
	int spin_count = 0;

	while (!_lock_try_adaptive(mp)) {
		if (mp->mutex_type & USYNC_PROCESS) {
			_lwp_mutex_lock(mp);
			return (0);
		}
		if ((spin_count++) % MUTEX_CHECK_FREQ == 0) {
			_trace_sched_lock();
			mp->mutex_magic = MUTEX_MAGIC;
			if (__tdb_attach_stat != TDB_NOT_ATTACHED)
				_tdb_sync_obj_register((caddr_t) mp,
				    MUTEX_MAGIC);
			/* look at (real) onproc state of owner */
			owner_t = _lock_owner(mp);
			if (owner_t != NULL && spin_count < MUTEX_MAX_SPIN) {
				if (owner_t->t_state == TS_ONPROC &&
				    owner_t->t_lwpdata != NULL) {
					state = owner_t->t_lwp_data->sc_state;
					if (state == SC_ONPROC) {
						/*
						 * thread and lwp are ONPROC,
						 * so spin
						 */
						_trace_sched_unlock();
						continue;
					}
				}
				waiters = mp->mutex_waiters;
				mp->mutex_waiters = 1;
				if (!_lock_try_adaptive(mp)) {
					curthread->t_flag &= ~T_WAITCV;
					_t_block((caddr_t)mp);
					_trace_sched_unlock_nosig();
					_swtch(0);
					_sigon();
					spin_count = 0;
					continue;
				}
				mp->mutex_waiters = waiters;
				_trace_sched_unlock();
				return (0);
			}
		}
	}
	return (0);
}

int
trace_mutex_unlock(mutex_t *mp)
{
	u_char waiters;

	if (_mutex_unlock_asm(mp) > 0) {
		if (mp->mutex_type & USYNC_PROCESS) {
			_lwp_mutex_unlock(mp);
		} else {
			_trace_sched_lock();
			if (_t_release((caddr_t)mp, &waiters, 0) > 0)
				mp->mutex_waiters = waiters;
			_trace_sched_unlock();
		}
	}
	return (0);
}
/*
 * mutex_tryenter above could have a trace point. Use this instead, inside
 * trace code.
 */
int
trace_mutex_trylock(mutex_t *mp)
{
	mp->mutex_magic = MUTEX_MAGIC;
	if (!_lock_try_adaptive(mp))
		return (0);
	else
		return (EBUSY);
}

int
trace_mutex_init(mutex_t *mp, int type, void *arg)
{
	if (type & USYNC_PROCESS) {
		mp->mutex_type = USYNC_PROCESS;
	} else
		mp->mutex_type = type;
	mp->mutex_magic = MUTEX_MAGIC;
	mp->mutex_waiters = 0;
	_lock_clear_adaptive(mp);
	return (0);
}

#endif
