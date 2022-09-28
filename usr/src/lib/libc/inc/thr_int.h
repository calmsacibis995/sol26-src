/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#if ! defined(_THR_INT_H)
#define _THR_INT_H

#pragma	ident	"@(#)thr_int.h	1.5	96/07/18 SMI"


#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/*
 * Thread/Libc/rtld Interface
 */
#define	TI_NULL		0	/* (void) last entry */
#define	TI_MUTEX_LOCK	1	/* _mutex_lock() address */
#define	TI_MUTEX_UNLOCK	2	/* _mutex_unlock() address */
#define	TI_LRW_RDLOCK	3	/* _llrw_rdlock() address */
#define	TI_LRW_WRLOCK	4	/* _llrw_wrlock() address */
#define	TI_LRW_UNLOCK	5	/* _llrw_unlock() address */
#define	TI_BIND_GUARD	6	/* _bind_guard() address */
#define	TI_BIND_CLEAR	7	/* _bind_clear() address */
#define	TI_LATFORK	8	/* _lpthread_atfork() */
#define	TI_THRSELF	9	/* _thr_self() address */
#define	TI_VERSION	10	/* current version of ti_interface */
#define	TI_COND_BROAD	11	/* _cond_broadcast() address */
#define	TI_COND_DESTROY	12	/* _cond_destroy() address */
#define	TI_COND_INIT	13	/* _cond_init() address */
#define	TI_COND_SIGNAL	14	/* _cond_signal() address */
#define	TI_COND_TWAIT	15	/* _cond_timedwait() address */
#define	TI_COND_WAIT	16	/* _cond_wait() address */
#define	TI_FORK		17	/* _fork() address */
#define	TI_FORK1	18	/* _fork1() address */
#define	TI_MUTEX_DEST	19	/* _mutex_destroy() address */
#define	TI_MUTEX_HELD	20	/* _mutex_held() address */
#define	TI_MUTEX_INIT	21	/* _mutex_init() address */
#define	TI_MUTEX_TRYLCK	22	/* _mutex_trylock() address */
#define	TI_ATFORK	23	/* _pthread_atfork() */
#define	TI_RW_RDHELD	24	/* _rw_read_held() address */
#define	TI_RW_RDLOCK	25	/* _rw_rdlock() address */
#define	TI_RW_WRLOCK	26	/* _rw_wrlock() address */
#define	TI_RW_UNLOCK	27	/* _rw_unlock() address */
#define	TI_TRYRDLOCK	28	/* _rw_tryrdlock() address */
#define	TI_TRYWRLOCK	29	/* _rw_trywrlock() address */
#define	TI_RW_WRHELD	30	/* _rw_write_held() address */
#define	TI_RWLOCKINIT	31	/* _rwlock_init() address */
#define	TI_SEM_HELD	32	/* _sema_held() address */
#define	TI_SEM_INIT	33	/* _sema_init() address */
#define	TI_SEM_POST	34	/* _sema_post() address */
#define	TI_SEM_TRYWAIT	35	/* _sema_trywait() address */
#define	TI_SEM_WAIT	36	/* _sema_wait() address */
#define	TI_SIGACTION	37	/* _sigaction() address */
#define	TI_SIGPROCMASK	38	/* _sigprocmask() address */
#define	TI_SIGWAIT	39	/* _sigwait() address */
#define	TI_SLEEP	40	/* _sleep() address */
#define	TI_THR_CONT	41	/* _thr_continue() address */
#define	TI_THR_CREATE	42	/* _thr_create() address */
#define	TI_THR_ERRNOP	43	/* _thr_errnop() address */
#define	TI_THR_EXIT	44	/* _thr_exit() address */
#define	TI_THR_GETCONC	45	/* _thr_getconcurrency() address */
#define	TI_THR_GETPRIO	46	/* _thr_getprio() address */
#define	TI_THR_GETSPEC	47	/* _thr_getspecific() address */
#define	TI_THR_JOIN	48	/* _thr_join() address */
#define	TI_THR_KEYCREAT	49	/* _thr_keycreate() address */
#define	TI_THR_KILL	50	/* _thr_kill() address */
#define	TI_THR_MAIN	51	/* _thr_main() address */
#define	TI_THR_SETCONC	52	/* _thr_setconcurrency() address */
#define	TI_THR_SETPRIO	53	/* _thr_setprio() address */
#define	TI_THR_SETSPEC	54	/* _thr_setspecific() address */
#define	TI_THR_SIGSET	55	/* _thr_sigsetmask() address */
#define	TI_THR_STKSEG	56	/* _thr_stksegment() address */
#define	TI_THR_SUSPEND	57	/* _thr_suspend() address */
#define	TI_THR_YIELD	58	/* _thr_yield() address */
#define	TI_CLOSE	59	/* _close() address */
#define	TI_CREAT	60	/* _creat() address */
#define	TI_FCNTL	61	/* _fcntl() address */
#define	TI_FSYNC	62	/* _fsync() address */
#define	TI_MSYNC	63	/* _msync() address */
#define	TI_OPEN		64	/* _open() address */
#define	TI_PAUSE	65	/* _pause() address */
#define	TI_READ		66	/* _read() address */
#define	TI_SIGSUSPEND	67	/* _sigsuspend() address */
#define	TI_TCDRAIN	68	/* _tcdrain() address */
#define	TI_WAIT		69	/* _wait() address */
#define	TI_WAITPID	70	/* _waitpid() address */
#define	TI_WRITE	71	/* _write() address */
#define	TI_PCOND_BROAD	72	/* _pthread_cond_broadcast() address */
#define	TI_PCOND_DEST	73	/* _pthread_cond_destroy() address */
#define	TI_PCOND_INIT	74	/* _pthread_cond_init() address */
#define	TI_PCOND_SIGNAL	75	/* _pthread_cond_signal() address */
#define	TI_PCOND_TWAIT	76	/* _pthread_cond_timedwait() address */
#define	TI_PCOND_WAIT	77	/* _pthread_cond_wait() address */
#define	TI_PCONDA_DEST	78	/* _pthread_condattr_destroy() address */
#define	TI_PCONDA_GETPS	79	/* _pthread_condattr_getpshared() address */
#define	TI_PCONDA_INIT	80	/* _pthread_condattr_init() address */
#define	TI_PCONDA_SETPS	81	/* _pthread_condattr_setpshared() address */
#define	TI_PMUTEX_DEST	82	/* _pthread_mutex_destroy() address */
#define	TI_PMUTEX_GPC	83	/* _pthread_mutex_getprioceiling() address */
#define	TI_PMUTEX_INIT	84	/* _pthread_mutex_init() address */
#define	TI_PMUTEX_LOCK	85	/* _pthread_mutex_lock() address */
#define	TI_PMUTEX_SPC	86	/* _pthread_mutex_setprioceiling() address */
#define	TI_PMUTEX_TRYL	87	/* _pthread_mutex_trylock() address */
#define	TI_PMUTEX_UNLCK	88	/* _pthread_mutex_unlock() address */
#define	TI_PMUTEXA_DEST	89	/* _pthread_mutexattr_destory() address */
#define	TI_PMUTEXA_GPC	90	/* _pthread_mutexattr_getprioceiling() */
#define	TI_PMUTEXA_GP	91	/* _pthread_mutexattr_getprotocol() address */
#define	TI_PMUTEXA_GPS	92	/* _pthread_mutexattr_getpshared() address */
#define	TI_PMUTEXA_INIT	93	/* _pthread_mutexattr_init() address */
#define	TI_PMUTEXA_SPC	94	/* _pthread_mutexattr_setprioceiling() */
#define	TI_PMUTEXA_SP	95	/* _pthread_mutexattr_setprotocol() address */
#define	TI_PMUTEXA_SPS	96	/* _pthread_mutexattr_setpshared() address */
#define	TI_THR_MINSTACK	97	/* _thr_min_stack() address */
#define	TI_SIGTIMEDWAIT	98	/* __sigtimedwait() address */
#define	TI_ALARM	99	/* _alarm() address */
#define	TI_SETITIMER	100	/* _setitimer() address */
#define	TI_SIGLONGJMP	101	/* _siglongjmp() address */
#define	TI_SIGSETJMP	102	/* _sigsetjmp() address */
#define	TI_SIGPENDING	103	/* _sigpending() address */
#define	TI__NANOSLEEP	104	/* __nanosleep() address */
#define	TI_OPEN64	105	/* _open64() address */
#define	TI_CREAT64	106	/* _creat64() address */
#define	TI_RWLCKDESTROY	107	/* _rwlock_destroy() address */
#define	TI_SEMADESTROY	108	/* _sema_destroy() address */

#define	TI_MAX		109

#define	TI_V_NONE	0		/* ti_version versions */
#define	TI_V_CURRENT	1		/* current version of threads */
					/*	interface. */
#define	TI_V_NUM	2

/*
 * Threads Interface communication structure for threads library
 */
typedef struct {
	int	ti_tag;
	union {
		int	ti_val;
		int (*	ti_func)();
	} ti_un;
} Thr_interface;

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* ! defined(_THR_INT_H) */
