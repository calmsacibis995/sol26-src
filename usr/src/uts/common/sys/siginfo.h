/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/
/*
 * Copyright (c) 1993-1996, by Sun Microsystems, Inc.
 * Al rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_SIGINFO_H
#define	_SYS_SIGINFO_H

#pragma ident	"@(#)siginfo.h	1.39	96/06/28 SMI"	/* SVr4.0 1.20 */

#include <sys/feature_tests.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if (!defined(_POSIX_C_SOURCE) && !defined(_XPG4_2)) || \
	(_POSIX_C_SOURCE > 2) || defined(__EXTENSIONS__)

union sigval {
	int	sival_int;	/* integer value */
	void	*sival_ptr;	/* pointer value */
};

#else 				/* needed in siginfo_t structure */

union __sigval {
	int	__sival_int;	/* integer value */
	void	*__sival_ptr;	/* pointer value */
};

#endif

#if (!defined(_POSIX_C_SOURCE) && !defined(_XPG4_2)) || \
	(_POSIX_C_SOURCE > 2) || defined(__EXTENSIONS__)

struct sigevent {
	int		sigev_notify;	/* notification mode */
	union {
		int	_sigev_signo;	/* signal number */
		void	(*_sigev_notify_function)(union sigval);
	} _sigev_un;
	union sigval	sigev_value;	/* signal value */
	int		_sigev_pad1;
	void		*_sigev_notify_attributes;
	int		_sigev_pad2;
};
#define	sigev_signo	_sigev_un._sigev_signo

/* values of sigev_notify */
#define	SIGEV_NONE	1		/* no notification */
#define	SIGEV_SIGNAL	2		/* queued signal notification */
#define	SIGEV_THREAD	3		/* call back from another thread */

#endif /* !defined(_POSIX_C_SOURCE) && !defined(_XPG4_2)) ... */

#if !defined(_POSIX_C_SOURCE) || (_POSIX_C_SOURCE > 2) || \
	defined(__EXTENSIONS__)
/*
 * negative signal codes are reserved for future use for user generated
 * signals
 */

#define	SI_FROMUSER(sip)	((sip)->si_code <= 0)
#define	SI_FROMKERNEL(sip)	((sip)->si_code > 0)

#define	SI_NOINFO	32767	/* no signal information */
#define	SI_USER		0	/* user generated signal via kill() */
#define	SI_LWP		(-1)	/* user generated signal via lwp_kill() */
#define	SI_QUEUE	(-2)	/* user generated signal via sigqueue() */
#define	SI_TIMER	(-3)	/* from timer expiration */
#define	SI_ASYNCIO	(-4)	/* from asynchronous I/O completion */
#define	SI_MESGQ	(-5)	/* from message arrival */
#endif /* !defined(_POSIX_C_SOURCE) || (_POSIX_C_SOURCE > 2)... */

#if !defined(_POSIX_C_SOURCE) || defined(__XPG4_2) || defined(__EXTENSIONS__)
/*
 * Get the machine dependent signal codes (SIGILL, SIGFPE, SIGSEGV, and
 * SIGBUS) from <sys/machsig.h>
 */

#include <sys/machsig.h>

/*
 * SIGTRAP signal codes
 */

#define	TRAP_BRKPT	1	/* breakpoint trap */
#define	TRAP_TRACE	2	/* trace trap */
#define	TRAP_RWATCH	3	/* read access watchpoint trap */
#define	TRAP_WWATCH	4	/* write access watchpoint trap */
#define	TRAP_XWATCH	5	/* execute access watchpoint trap */
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	NSIGTRAP	5
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

/*
 * SIGCLD signal codes
 */

#define	CLD_EXITED	1	/* child has exited */
#define	CLD_KILLED	2	/* child was killed */
#define	CLD_DUMPED	3	/* child has coredumped */
#define	CLD_TRAPPED	4	/* traced child has stopped */
#define	CLD_STOPPED	5	/* child has stopped on signal */
#define	CLD_CONTINUED	6	/* stopped child has continued */

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	NSIGCLD		6
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

/*
 * SIGPOLL signal codes
 */

#define	POLL_IN		1	/* input available */
#define	POLL_OUT	2	/* output possible */
#define	POLL_MSG	3	/* message available */
#define	POLL_ERR	4	/* I/O error */
#define	POLL_PRI	5	/* high priority input available */
#define	POLL_HUP	6	/* device disconnected */

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	NSIGPOLL	6
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#endif /* !defined(_POSIX_C_SOURCE) || defined(_XPG4_2) ... */

#if (!defined(_POSIX_C_SOURCE) && !defined(_XPG4_2)) || defined(__EXTENSIONS__)
/*
 * SIGPROF signal codes
 */

#define	PROF_SIG	1	/* have to set code non-zero */
#define	NSIGPROF	1

#endif /* (!defined(_POSIX_C_SOURCE) && !defined(_XPG4_2)) || ... */

#if !defined(_POSIX_C_SOURCE) || (_POSIX_C_SOURCE > 2) || \
	defined(__EXTENSIONS__)

#define	SI_MAXSZ	128
#define	SI_PAD		((SI_MAXSZ / sizeof (int)) - 3)

/*
 * We need <sys/time.h> for the declaration of timestruc_t.  However,
 * since * inclusion of <sys/time.h> results in XPG4v2 namespace
 * pollution, the definition for timestruct_t has been included here
 * if _XPG4_2 is defined, otherwise, include <sys/time.h>.
 */

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)

#include <sys/time.h>
#else

#ifndef	_TIME_T
#define	_TIME_T
typedef	long    time_t;		/* time of day in seconds */
#endif /* _TIME_T */

#ifndef	_TIMESPEC_T
#define	_TIMESPEC_T
typedef	struct	_timespec {
	time_t		_tv_sec;	/* seconds */
	long		_tv_nsec;	/* and nanoseconds */
} timespec_t;
#endif /* _TIMESPEC_T */

#ifndef	_TIMESTRUC_T
#define	_TIMESTRUC_T
typedef	struct _timespec  timestruc_t;	/* definition per SVr4 */
#endif	/* _TIMESTRUC_T */

#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#if !defined(_XPG4_2) && !defined(_POSIX_C_SOURCE) || defined(__EXTENSIONS__)
typedef struct siginfo { 		/* pollutes POSIX/XOPEN namespace */
#else
typedef struct {
#endif
	int	si_signo;			/* signal from signal.h	*/
	int 	si_code;			/* code from above	*/
	int	si_errno;			/* error from errno.h	*/

	union {

		int	__pad[SI_PAD];		/* for future growth	*/

		struct {			/* kill(), SIGCLD, siqqueue() */
			pid_t	__pid;		/* process ID		*/
			union {
				struct {
					uid_t	__uid;
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
					union sigval	__value;
#else
					union __sigval	__value;
#endif
				} __kill;
				struct {
					clock_t __utime;
					int	__status;
					clock_t __stime;
				} __cld;
			} __pdata;
		} __proc;

		struct {	/* SIGSEGV, SIGBUS, SIGILL, SIGTRAP, SIGFPE */
			void 	*__addr;	/* faulting address	*/
			int	__trapno;	/* illegal trap number	*/
			caddr_t	__pc;		/* instruction address	*/
		} __fault;

		struct {			/* SIGPOLL, SIGXFSZ	*/
		/* fd not currently available for SIGPOLL */
			int	__fd;		/* file descriptor	*/
			long	__band;
		} __file;

		struct {			/* SIGPROF */
			caddr_t	__faddr;	/* last fault address	*/
			timestruc_t __tstamp;	/* real time stamp	*/
			short	__syscall;	/* current syscall	*/
			char	__nsysarg;	/* number of arguments	*/
			char	__fault;	/* last fault type	*/
			long	__sysarg[8];	/* syscall arguments	*/
			long	__mstate[17];	/* exactly fills struct	*/
		} __prof;

	} __data;

} siginfo_t;

/*
 * XXX -- internal version is identical to siginfo_t but without the padding.
 * This must be maintained in sync with it.
 */

#if !defined(_POSIX_C_SOURCE) && !defined(_XPG4_2) || defined(__EXTENSIONS__)

typedef struct k_siginfo {

	int	si_signo;			/* signal from signal.h	*/
	int 	si_code;			/* code from above	*/
	int	si_errno;			/* error from errno.h	*/

	union {
		struct {			/* kill(), SIGCLD, siqqueue() */
			pid_t	__pid;		/* process ID		*/
			union {
				struct {
					uid_t	__uid;
					union sigval	__value;
				} __kill;
				struct {
					clock_t __utime;
					int	__status;
					clock_t __stime;
				} __cld;
			} __pdata;
		} __proc;

		struct {	/* SIGSEGV, SIGBUS, SIGILL, SIGTRAP, SIGFPE */
			void 	*__addr;	/* faulting address	*/
			int	__trapno;	/* illegal trap number	*/
			caddr_t	__pc;		/* instruction address	*/
		} __fault;

		struct {			/* SIGPOLL, SIGXFSZ	*/
		/* fd not currently available for SIGPOLL */
			int	__fd;		/* file descriptor	*/
			long	__band;
		} __file;

		struct {			/* SIGPROF */
			caddr_t	__faddr;	/* last fault address	*/
			timestruc_t __tstamp;	/* real time stamp	*/
			short	__syscall;	/* current syscall	*/
			char	__nsysarg;	/* number of arguments	*/
			char	__fault;	/* last fault type	*/
			/* these are omitted to keep k_siginfo_t small	*/
			/* long	__sysarg[8]; */
			/* long	__mstate[17]; */
		} __prof;

	} __data;

} k_siginfo_t;

typedef struct sigqueue {
	struct sigqueue	*sq_next;
	k_siginfo_t	sq_info;
	void		(*sq_func)(struct sigqueue *); /* destructor function */
	void		*sq_backptr;	/* pointer to the data structure */
					/* associated by sq_func()	*/
} sigqueue_t;

/*  indication whether to queue the signal or not */
#define	SI_CANQUEUE(c)	((c) <= SI_QUEUE)

#endif /* !defined(_POSIX_C_SOURCE) && !defined(_XPG4_2) || ... */

#define	si_pid		__data.__proc.__pid
#define	si_status	__data.__proc.__pdata.__cld.__status
#define	si_stime	__data.__proc.__pdata.__cld.__stime
#define	si_utime	__data.__proc.__pdata.__cld.__utime
#define	si_uid		__data.__proc.__pdata.__kill.__uid
#define	si_value	__data.__proc.__pdata.__kill.__value
#define	si_addr		__data.__fault.__addr
#define	si_trapno	__data.__fault.__trapno
#define	si_trapafter	__data.__fault.__trapno
#define	si_pc		__data.__fault.__pc
#define	si_fd		__data.__file.__fd
#define	si_band		__data.__file.__band
#define	si_tstamp	__data.__prof.__tstamp
#define	si_syscall	__data.__prof.__syscall
#define	si_nsysarg	__data.__prof.__nsysarg
#define	si_sysarg	__data.__prof.__sysarg
#define	si_fault	__data.__prof.__fault
#define	si_faddr	__data.__prof.__faddr
#define	si_mstate	__data.__prof.__mstate

#endif /* !defined(_POSIX_C_SOURCE) || (_POSIX_C_SOURCE > 2) ... */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SIGINFO_H */
