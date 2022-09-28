/*	Copyright (c) 1995 SMI	*/
/*	 All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)thread_interface.c	1.28	96/08/05	SMI"


/*
 * Inform libc and the run-time linker of the address of the threads interface.
 *
 * In a multi-threaded dynamic application, the run-time linker must insure
 * concurrency when performing such things as function (.plt) binding, and
 * dlopen and dlclose functions.  To be consistent with the threads mechanisms,
 * (and as light weight as possible), the threads .init routine provides for
 * selected mutex function addresses to be passed to ld.so.1.
 */
#include	"libthread.h"
#include	<thread.h>
#include	<sys/link.h>
#include	<unistd.h>

#include	"thr_int.h"

extern void	_libc_threads_interface(Thr_interface *);

/*
 * Static functions
 */
static	int _llrw_rdlock();
static	int _llrw_wrlock();
static	int _llrw_unlock();
static	int _bind_guard(int);
static	int _bind_clear(int);

static Thr_interface _ti_funcs[TI_MAX] = {
/* 01 */	{ TI_MUTEX_LOCK,	(int) _ti_mutex_lock },
/* 02 */	{ TI_MUTEX_UNLOCK,	(int) _ti_mutex_unlock },
/* 03 */	{ TI_LRW_RDLOCK,	(int) _llrw_rdlock },
/* 04 */	{ TI_LRW_WRLOCK,	(int) _llrw_wrlock },
/* 05 */	{ TI_LRW_UNLOCK,	(int) _llrw_unlock },
/* 06 */	{ TI_BIND_GUARD,	(int) _bind_guard },
/* 07 */	{ TI_BIND_CLEAR,	(int) _bind_clear },
/* 08 */	{ TI_LATFORK,		(int) _lpthread_atfork },
/* 09 */	{ TI_THRSELF,		(int) _ti_thr_self },
/* 10 */	{ TI_VERSION,		TI_V_CURRENT},
/* 11 */	{ TI_COND_BROAD,	(int) _ti_cond_broadcast },
/* 12 */	{ TI_COND_DESTROY,	(int) _ti_cond_destroy },
/* 13 */	{ TI_COND_INIT,		(int) _ti_cond_init },
/* 14 */	{ TI_COND_SIGNAL,	(int) _ti_cond_signal },
/* 15 */	{ TI_COND_TWAIT,	(int) _ti_cond_timedwait },
/* 16 */	{ TI_COND_WAIT,		(int) _ti_cond_wait },
/* 17 */	{ TI_FORK,		(int) _ti_fork },
/* 18 */	{ TI_FORK1,		(int) _ti_fork1 },
/* 19 */	{ TI_MUTEX_DEST,	(int) _ti_mutex_destroy },
/* 20 */	{ TI_MUTEX_HELD,	(int) _ti_mutex_held },
/* 21 */	{ TI_MUTEX_INIT,	(int) _ti_mutex_init },
/* 22 */	{ TI_MUTEX_TRYLCK,	(int) _ti_mutex_trylock },
/* 23 */	{ TI_ATFORK,		(int) _ti_pthread_atfork },
/* 24 */	{ TI_RW_RDHELD,		(int) _ti_rw_read_held },
/* 25 */	{ TI_RW_RDLOCK,		(int) _ti_rw_rdlock },
/* 26 */	{ TI_RW_WRLOCK,		(int) _ti_rw_wrlock },
/* 27 */	{ TI_RW_UNLOCK,		(int) _ti_rw_unlock },
/* 28 */	{ TI_TRYRDLOCK,		(int) _ti_rw_tryrdlock },
/* 29 */	{ TI_TRYWRLOCK,		(int) _ti_rw_trywrlock },
/* 30 */	{ TI_RW_WRHELD,		(int) _ti_rw_write_held },
/* 31 */	{ TI_RWLOCKINIT,	(int) _ti_rwlock_init },
/* 32 */	{ TI_SEM_HELD,		(int) _ti_sema_held },
/* 33 */	{ TI_SEM_INIT,		(int) _ti_sema_init },
/* 34 */	{ TI_SEM_POST,		(int) _ti_sema_post },
/* 35 */	{ TI_SEM_TRYWAIT,	(int) _ti_sema_trywait },
/* 36 */	{ TI_SEM_WAIT,		(int) _ti_sema_wait },
/* 37 */	{ TI_SIGACTION,		(int) _ti_sigaction },
/* 38 */	{ TI_SIGPROCMASK,	(int) _ti_sigprocmask },
/* 39 */	{ TI_SIGWAIT,		(int) _ti_sigwait },
/* 40 */	{ TI_SLEEP,		(int) _ti_sleep },
/* 41 */	{ TI_THR_CONT,		(int) _ti_thr_continue },
/* 42 */	{ TI_THR_CREATE,	(int) _ti_thr_create },
/* 43 */	{ TI_THR_ERRNOP,	(int) _ti_thr_errnop },
/* 44 */	{ TI_THR_EXIT,		(int) _ti_thr_exit },
/* 45 */	{ TI_THR_GETCONC,	(int) _ti_thr_getconcurrency },
/* 46 */	{ TI_THR_GETPRIO,	(int) _ti_thr_getprio },
/* 47 */	{ TI_THR_GETSPEC,	(int) _ti_thr_getspecific },
/* 48 */	{ TI_THR_JOIN,		(int) _ti_thr_join },
/* 49 */	{ TI_THR_KEYCREAT,	(int) _ti_thr_keycreate },
/* 50 */	{ TI_THR_KILL,		(int) _ti_thr_kill },
/* 51 */	{ TI_THR_MAIN,		(int) _ti_thr_main },
/* 52 */	{ TI_THR_SETCONC,	(int) _ti_thr_setconcurrency },
/* 53 */	{ TI_THR_SETPRIO,	(int) _ti_thr_setprio },
/* 54 */	{ TI_THR_SETSPEC,	(int) _ti_thr_setspecific },
/* 55 */	{ TI_THR_SIGSET,	(int) _ti_thr_sigsetmask },
/* 56 */	{ TI_THR_STKSEG,	(int) _ti_thr_stksegment },
/* 57 */	{ TI_THR_SUSPEND,	(int) _ti_thr_suspend },
/* 58 */	{ TI_THR_YIELD,		(int) _ti_thr_yield },
/* 59 */	{ TI_CLOSE,		(int) _ti_close },
/* 60 */	{ TI_CREAT,		(int) _ti_creat },
/* 61 */	{ TI_FCNTL,		(int) _ti_fcntl },
/* 62 */	{ TI_FSYNC,		(int) _ti_fsync },
/* 63 */	{ TI_MSYNC,		(int) _ti_msync },
/* 64 */	{ TI_OPEN,		(int) _ti_open },
/* 65 */	{ TI_PAUSE,		(int) _ti_pause },
/* 66 */	{ TI_READ,		(int) _ti_read },
/* 67 */	{ TI_SIGSUSPEND,	(int) _ti_sigsuspend },
/* 68 */	{ TI_TCDRAIN,		(int) _ti_tcdrain },
/* 69 */	{ TI_WAIT,		(int) _ti_wait },
/* 70 */	{ TI_WAITPID,		(int) _ti_waitpid },
/* 71 */	{ TI_WRITE,		(int) _ti_write },
/* 72 */	{ TI_PCOND_BROAD,	(int) _ti_pthread_cond_broadcast },
/* 73 */	{ TI_PCOND_DEST,	(int) _ti_pthread_cond_destroy },
/* 74 */	{ TI_PCOND_INIT,	(int) _ti_pthread_cond_init },
/* 75 */	{ TI_PCOND_SIGNAL,	(int) _ti_pthread_cond_signal },
/* 76 */	{ TI_PCOND_TWAIT,	(int) _ti_pthread_cond_timedwait },
/* 77 */	{ TI_PCOND_WAIT,	(int) _ti_pthread_cond_wait },
/* 78 */	{ TI_PCONDA_DEST,	(int) _ti_pthread_condattr_destroy },
/* 79 */	{ TI_PCONDA_GETPS,	(int) _ti_pthread_condattr_getpshared },
/* 80 */	{ TI_PCONDA_INIT,	(int) _ti_pthread_condattr_init },
/* 81 */	{ TI_PCONDA_SETPS,	(int) _ti_pthread_condattr_setpshared },
/* 82 */	{ TI_PMUTEX_DEST,	(int) _ti_pthread_mutex_destroy },
/* 83 */	{ TI_PMUTEX_GPC,
			(int) _ti_pthread_mutex_getprioceiling },
/* 84 */	{ TI_PMUTEX_INIT,	(int) _ti_pthread_mutex_init },
/* 85 */	{ TI_PMUTEX_LOCK,	(int) _ti_pthread_mutex_lock },
/* 86 */	{ TI_PMUTEX_SPC,
			(int) _ti_pthread_mutex_setprioceiling },
/* 87 */	{ TI_PMUTEX_TRYL,	(int) _ti_pthread_mutex_trylock },
/* 88 */	{ TI_PMUTEX_UNLCK,	(int) _ti_pthread_mutex_unlock },
/* 89 */	{ TI_PMUTEXA_DEST,	(int) _ti_pthread_mutexattr_destroy },
/* 90 */	{ TI_PMUTEXA_GPC,
			(int) _ti_pthread_mutexattr_getprioceiling },
/* 91 */	{ TI_PMUTEXA_GP,
			(int) _ti_pthread_mutexattr_getprotocol },
/* 92 */	{ TI_PMUTEXA_GPS,
			(int) _ti_pthread_mutexattr_getpshared },
/* 93 */	{ TI_PMUTEXA_INIT,	(int) _ti_pthread_mutexattr_init },
/* 94 */	{ TI_PMUTEXA_SPC,
			(int) _ti_pthread_mutexattr_setprioceiling },
/* 95 */	{ TI_PMUTEXA_SP,
			(int) _ti_pthread_mutexattr_setprotocol },
/* 96 */	{ TI_PMUTEXA_SPS,
			(int) _ti_pthread_mutexattr_setpshared },
/* 97 */	{ TI_THR_MINSTACK,	(int) _ti_thr_min_stack },
/* 98 */	{ TI_SIGTIMEDWAIT,	(int) _ti_sigtimedwait },
/* 99 */	{ TI_ALARM,		(int) _ti_alarm },
/* 100 */	{ TI_SETITIMER,		(int) _ti_setitimer },
/* 103 */	{ TI_SIGPENDING,	(int) _ti_sigpending },
/* 104 */	{ TI__NANOSLEEP,	(int) _ti__nanosleep },
/* 105 */	{ TI_OPEN64,		(int) _ti_open64 },
/* 106 */	{ TI_CREAT64,		(int) _ti_creat64 },
/* 00 */	{ TI_NULL,		0 }
};

/*
 * _thr_libthread() is used to identify the link order
 * of libc.so vs. libthread.so.  Their is a copy of each in
 * both libraries.  They return the following:
 *
 *	libc:_thr_libthread(): returns 0
 *	libthread:_thr_libthread(): returns 1
 *
 * A call to this routine can be used to determine whether or
 * not the libc threads interface needs to be initialized or not.
 */
int
_thr_libthread()
{
	return (1);
}

static int
_llrw_rdlock(rwlock_t * _lrw_lock)
{
	return (_lrw_rdlock(_lrw_lock));
}

static int
_llrw_wrlock(rwlock_t * _lrw_lock)
{
	return (_lrw_wrlock(_lrw_lock));
}

static int
_llrw_unlock(rwlock_t * _lrw_lock)
{
	return (_lrw_unlock(_lrw_lock));
}

#ifdef BUILD_STATIC
#pragma	weak	_ld_concurrency
#pragma	weak	_libc_threads_interface
#endif

void
_set_rtld_interface()
{
	void (*	fptr)();

	if ((fptr = _ld_concurrency) != 0)
		(* fptr)(_ti_funcs);
}

void
_set_libc_interface()
{
	void (*	fptr)();
	if ((fptr = _libc_threads_interface) != 0)
		(*fptr)(_ti_funcs);
}


void
_unset_rtld_interface()
{
	void (* fptr)();

	if ((fptr = _ld_concurrency) != 0)
		(* fptr)((Thr_interface *)0);
}

void
_unset_libc_interface()
{
	void (* fptr)();

	if ((fptr = _libc_threads_interface) != 0)
		(* fptr)((Thr_interface *)0);
}

/*
 * When ld.so.1 calls one of the mutex functions while binding a .plt its
 * possible that the function itself may require further .plt binding.  To
 * insure that this binding does not cause recursion we set the t_rtldbind
 * flags within the current thread.
 *
 * Note, because we may have .plt bindings occurring prior to %g7 being
 * initialized we must allow for _curthread() == 0.
 *
 * Args:
 *	bindflags:	value of flag(s) to be set in the t_rtldbind field
 *
 * Returns:
 *	0:	if setting the flag(s) results in no change to the t_rtldbind
 *		value.  ie: all the flags were already set.
 *	1:	if a flag(or flags) were not set they have been set.
 */
static int
_bind_guard(int bindflags)
{
	uthread_t 	*thr;
	extern int _nthreads;

	if (_nthreads == 0) /* .init processing - do not need to acquire lock */
		return (0);
	if ((thr = _curthread()) == NULL)
		/*
		 * thr == 0 implies the thread is a bound thread on its
		 * way to die in _lwp_terminate(), just before calling
		 * _lwp_exit(). Should acquire lock in this case - so return 1.
		 */
		return (1);
	else if ((thr->t_rtldbind & bindflags) == bindflags)
		return (0);
	else {
		thr->t_rtldbind |= bindflags;
		return (1);
	}
}

/*
 * Args:
 *	bindflags:	value of flags to be cleared from the t_rtldbind field
 * Returns:
 *	resulting value of t_rtldbind
 *
 * Note: if ld.so.1 needs to query t_rtldbind it can pass in a value
 *	 of '0' to clear_bind() and examine the return code.
 */
static int
_bind_clear(int bindflags)
{
	uthread_t *	thr;
	if (_totalthreads > 0 && (thr = _curthread()) != NULL)
		return (thr->t_rtldbind &= ~bindflags);
	else
		return (0);
}
