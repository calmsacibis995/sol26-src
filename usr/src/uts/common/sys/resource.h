/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1996 Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_RESOURCE_H
#define	_SYS_RESOURCE_H

#pragma ident	"@(#)resource.h	1.21	96/06/03 SMI"	/* SVr4.0 1.11 */

#include <sys/feature_tests.h>

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Process priority specifications
 */

#define	PRIO_PROCESS	0
#define	PRIO_PGRP	1
#define	PRIO_USER	2


/*
 * Resource limits
 */

#define	RLIMIT_CPU	0		/* cpu time in milliseconds */
#define	RLIMIT_FSIZE	1		/* maximum file size */
#define	RLIMIT_DATA	2		/* data size */
#define	RLIMIT_STACK	3		/* stack size */
#define	RLIMIT_CORE	4		/* core file size */
#define	RLIMIT_NOFILE	5		/* file descriptors */
#define	RLIMIT_VMEM	6		/* maximum mapped memory */
#define	RLIMIT_AS	RLIMIT_VMEM

#define	RLIM_NLIMITS	7		/* number of resource limits */

/*
 * The definitions of the following types and constants differ between the
 * regular and large file compilation environments.
 */
#if _FILE_OFFSET_BITS == 32

typedef unsigned long	rlim_t;

#define	RLIM_INFINITY	0x7fffffff
#define	RLIM_SAVED_MAX	0x7ffffffe
#define	RLIM_SAVED_CUR	0x7ffffffd

#else

typedef u_longlong_t	rlim_t;

#define	RLIM_INFINITY	((rlim_t) -3)
#define	RLIM_SAVED_MAX	((rlim_t) -2)
#define	RLIM_SAVED_CUR	((rlim_t) -1)

#endif

struct rlimit {
	rlim_t	rlim_cur;		/* current limit */
	rlim_t	rlim_max;		/* maximum value for rlim_cur */
};

/* transitional large file interface versions */
#ifdef	_LARGEFILE64_SOURCE

typedef u_longlong_t	rlim64_t;

#define	RLIM64_INFINITY		((rlim64_t) -3)
#define	RLIM64_SAVED_MAX	((rlim64_t) -2)
#define	RLIM64_SAVED_CUR	((rlim64_t) -1)

struct rlimit64 {
	rlim64_t	rlim_cur;	/* current limit */
	rlim64_t	rlim_max;	/* maximum value for rlim_cur */
};

#endif

#ifdef _KERNEL

extern struct rlimit64 rlimits[];
extern rlim64_t	rlim_infinity_map[];

#if defined(__STDC__)

extern int	rlimit(int, rlim64_t, rlim64_t);

#else

extern int	rlimit();

#endif	/* __STDC__ */

#else

#include <sys/time.h>

#define	RUSAGE_SELF	0
#define	RUSAGE_CHILDREN	-1

struct	rusage {
	struct timeval ru_utime;	/* user time used */
	struct timeval ru_stime;	/* system time used */
	long	ru_maxrss;		/* XXX: 0 */
	long	ru_ixrss;		/* XXX: 0 */
	long	ru_idrss;		/* XXX: sum of rm_asrss */
	long	ru_isrss;		/* XXX: 0 */
	long	ru_minflt;		/* any page faults not requiring I/O */
	long	ru_majflt;		/* any page faults requiring I/O */
	long	ru_nswap;		/* swaps */
	long	ru_inblock;		/* block input operations */
	long	ru_oublock;		/* block output operations */
	long	ru_msgsnd;		/* messages sent */
	long	ru_msgrcv;		/* messages received */
	long	ru_nsignals;		/* signals received */
	long	ru_nvcsw;		/* voluntary context switches */
	long	ru_nivcsw;		/* involuntary " */
};

/* large file compilation environment setup */
#if _FILE_OFFSET_BITS == 64
#ifdef __PRAGMA_REDEFINE_EXTNAME
#pragma redefine_extname	setrlimit	setrlimit64
#pragma redefine_extname	getrlimit	getrlimit64
#else
#define	setrlimit		setrlimit64
#define	getrlimit		getrlimit64

#define	rlimit		rlimit64
#endif
#endif	/* _FILE_OFFSET_BITS == 64 */

#if defined(__STDC__)

extern int setrlimit(int, const struct rlimit *);
extern int getrlimit(int, struct rlimit *);

/* transitional large file interfaces */
#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
extern int setrlimit64(int, const struct rlimit64 *);
extern int getrlimit64(int, struct rlimit64 *);
#endif	/* _LARGEFILE64_SOURCE... */

extern int getpriority(int, id_t);
extern int setpriority(int, id_t, int);
extern int getrusage(int, struct rusage *);

#else	/* __STDC__ */

extern int getrlimit();
extern int setrlimit();

/* transitional large file interfaces */
#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
extern int setrlimit64();
extern int getrlimit64();
#endif	/* _LARGEFILE64_SOURCE... */

extern int getpriority();
extern int setpriority();
extern int getrusage();

#endif  /* __STDC__ */

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_RESOURCE_H */
