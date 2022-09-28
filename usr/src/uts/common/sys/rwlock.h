/*
 *	Copyright (c) 1991,1993 Sun Microsystems, Inc.
 */

/*
 * rwlock.h:
 *
 * definitions for thread synchronization primitives: readers/writer locks.
 * This is the public part of the interface to readers/write locks. The
 * private (implementation-specific) part is in <arch>/sys/rwlock_impl.h.
 */

#ifndef _SYS_RWLOCK_H
#define	_SYS_RWLOCK_H

#pragma ident	"@(#)rwlock.h	1.4	96/05/26 SMI"


#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif


#ifndef	_ASM

typedef enum {
	RW_SLEEP,			/* suspend if lock held */
	RW_SLEEP_STAT,			/* RW_SLEEP with statistics */
	RW_DRIVER_NOSTAT = 2,		/* driver (DDI) version */
	RW_DRIVER_STAT = 3,		/* driver with statistics */
	RW_DEFAULT			/* use statistics if enabled */
} krw_type_t;

typedef enum {
	RW_WRITER,
	RW_READER
} krw_t;

#if defined(_LOCKTEST) || defined(_MPSTATS)
#define	RW_DRIVER	RW_DRIVER_STAT
#else
#define	RW_DRIVER	RW_DRIVER_NOSTAT
#endif

typedef struct _krwlock {
	void	*_opaque[3];
} krwlock_t;


#if defined(_KERNEL)

#define	RW_READ_HELD(x)		(rw_read_held((x)))
#define	RW_WRITE_HELD(x)	(rw_write_held((x)))
#define	RW_LOCK_HELD(x)		(rw_lock_held((x)))
#define	RW_ISWRITER(x)		(rw_iswriter(x))

extern	void	rw_init(krwlock_t *, char *, krw_type_t, void *);
extern	void	rw_destroy(krwlock_t *);
extern	void	rw_enter(krwlock_t *, krw_t);
extern	int	rw_tryenter(krwlock_t *, krw_t);
extern	int	rw_read_locked(krwlock_t *);
extern	void	rw_exit(krwlock_t *);
extern	void	rw_downgrade(krwlock_t *);
extern	int	rw_tryupgrade(krwlock_t *);
extern	int	rw_read_held(krwlock_t *);
extern	int	rw_write_held(krwlock_t *);
extern	int	rw_lock_held(krwlock_t *);
extern	int	rw_iswriter(krwlock_t *);
extern	void	rw_mutex_init(void);
extern	struct _kthread *rw_owner(krwlock_t *);

/*
 * The following interfaces are used to do atomic loads and stores
 * of the long long data types. For sparc we use the ldd instructions
 * supported by load_double inline function. For other architectures
 * x86 and ppc  there are not such atomic ldd instructions and
 * therefore we go through the regular method of acquiring the lock
 * and releasing it. Note that the caller has to make sure that he
 * is holding the writers lock in case of rw_store_double.
 */

#if	defined(sparc)
extern u_longlong_t load_double(u_longlong_t *);
extern u_longlong_t store_double(u_longlong_t, u_longlong_t *);
#define	rw_load_double(resultp, valuep, rwlockp) \
	{ \
		*(resultp) = load_double((valuep)); \
	}
#define	rw_store_double(value, resultp) \
	{ \
		store_double((value), (resultp)); \
	}
#else
#define	rw_load_double(resultp, valuep, rwlockp) \
	{ \
		rw_enter((rwlockp), RW_READER); \
		*(resultp) = *(valuep); \
		rw_exit((rwlockp)); \
	}
#define	rw_store_double(value, resultp) \
	{ \
		*(resultp) = (value); \
	}
#endif /* sparc */

#endif	/* defined(_KERNEL) */

#endif	/* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_RWLOCK_H */
