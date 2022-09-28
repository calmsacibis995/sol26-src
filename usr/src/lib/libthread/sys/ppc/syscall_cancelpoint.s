/*	Copyright (c) 1994 by Sun Microsystems, Inc.		*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/


	.ident	"@(#)syscall_cancelpoint.s	1.6	96/05/22 SMI"

	.file	"syscall_cancelpoint.s"

#include <sys/asm_linkage.h>

/*
 * This file lists all the libc(POSIX.1) and libposix4(POSIX.1b)
 * which have been declared as CANCELLATION POINTS in POSIX.1c.
 *
 * SYSCALL_CANCELPOINT() macro provides the required wrapper to
 * interpose any call defined in an library. It assumes followings:
 * 	1. libthread must ne linked before that library
 *	2. `newname` function should have been defined.
 * For example, if a function foo() declared in libc and its a
 * system call then define bar symbol also.
 * Then insert here:
 * 			SYSCALL_CANCELPOINT(foo, bar)
 *
 * This will interpose foo symbol here and
 * wrapper, after creating cancellation point, will call bar.
 * In many cases, bar may be _foo which is a private interface.
 */


/* C library -- read						*/
/* int read (int fildes, void *buf, unsigned nbyte);		*/

/* C library -- close						*/
/* int close (int fildes);					*/

/* C library -- open						*/
/* int open (const char *path, int oflag, [ mode_t mode ]);	*/
/* int open64 (const char *path, int oflag, [ mode_t mode ]);	*/

/* C library -- write						*/
/* int write (int fildes, void *buf, unsigned nbyte);		*/

/* C library -- fcntl (if cmd = F_SETLKW)			*/
/* int fcntl (int fildes, int cmd [, arg]);			*/

/* C library -- pause						*/
/* int pause (void);						*/

/* C library -- sigsuspend					*/
/* int sigsuspend (sigset_t *set);				*/

/* C library -- wait						*/
/* int wait (int *stat_loc);					*/

/* C library -- creat						*/
/* int creat (char *path, mode_t mode);				*/
/* int creat64 (char *path, mode_t mode);				*/

/* C library -- fsync						*/
/* int fsync (int fildes, void *buf, unsigned nbyte);		*/

/* C library -- sleep						*/
/* int sleep (unsigned sleep_tm);				*/
/* ALREADY A CANCELLATION POINT: IT CALLS siggsuspend()		*/

/* C library -- msync						*/
/* int msync (caddr_t addr, size_t  len, int flags);		*/

/* C library -- tcdrain						*/
/* int tcdrain (int fildes);					*/

/* C library -- waitpid						*/
/* int waitpid (pid_t pid, int *stat_loc, int options);		*/

/* C library -- system						*/
/* int system (const char *s);					*/
/* ALREADY A CANCELLATION POINT: IT CALLS waipid()		*/

/* POSIX.4 functions are defined as cancellation points in libposix4 */

/* POSIX.4 library -- sigtimedwait				*/
/* int sigtimedwait (const sigset_t *set, siginfo_t *info, 	*/
/*			const struct timespec *timeout);	*/

/* POSIX.4 library -- sigtimeinfo				*/
/* int sigtwaitinfo (const sigset_t *set, siginfo_t *info); 	*/

/* POSIX.4 library -- nanosleep					*/
/* int nanosleep (const struct timespec *rqtp, struct timespec *rqtp); */

/* POSIX.4 library -- sem_wait					*/
/* int sem_wait (sem_t *sp);					*/

/* POSIX.4 library -- mq_receive				*/
/* int mq_receive ();						*/

/* POSIX.4 library -- mq_send					*/
/* int mq_send ();						*/


#include "ppc/SYS_CANCEL.h"

	PRAGMA_WEAK(_ti_read, read)
	SYSCALL_CANCELPOINT(read, _read)

	PRAGMA_WEAK(_ti_close, close)
	SYSCALL_CANCELPOINT(close, _close)

	PRAGMA_WEAK(_ti_open, open)
	SYSCALL_CANCELPOINT(open, _open)

	PRAGMA_WEAK(_ti_open64, open64)
	SYSCALL_CANCELPOINT(open64, _open64)

	PRAGMA_WEAK(_ti_write, write)
	SYSCALL_CANCELPOINT(write, _write)

	SYSCALL_CANCELPOINT(_fcntl_cancel, _fcntl)

	PRAGMA_WEAK(_ti_pause, pause)
	SYSCALL_CANCELPOINT(pause, _pause)

	PRAGMA_WEAK(_ti_wait, wait)
	SYSCALL_CANCELPOINT(wait, _wait)

	PRAGMA_WEAK(_ti_creat, creat)
	SYSCALL_CANCELPOINT(creat, _creat)

	PRAGMA_WEAK(_ti_creat64, creat64)
	SYSCALL_CANCELPOINT(creat64, _creat64)

	PRAGMA_WEAK(_ti_fsync, fsync)
	SYSCALL_CANCELPOINT(fsync, _fsync)

	PRAGMA_WEAK(_ti_msync, msync)
	SYSCALL_CANCELPOINT(msync, _msync)

	PRAGMA_WEAK(_ti_tcdrain, tcdrain)
	SYSCALL_CANCELPOINT(tcdrain, _tcdrain)

	PRAGMA_WEAK(_ti_waitpid, waitpid)
	SYSCALL_CANCELPOINT(waitpid, _waitpid)

	PRAGMA_WEAK(_ti__nanosleep, __nanosleep)
	SYSCALL_CANCELPOINT(__nanosleep, _libc_nanosleep)

/*
 * End of syscall file
 */
