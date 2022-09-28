/*
 *	Copyright (c) 1991,1993 Sun Microsystems, Inc.
 */

#ifndef _SYS_MUTEX_IMPL_H
#define	_SYS_MUTEX_IMPL_H

#pragma ident	"@(#)mutex_impl.h	1.8	94/12/26 SMI"

#ifndef	_ASM
#include <sys/types.h>
#include <sys/machlock.h>
#include <sys/turnstile.h>
#include <sys/dki_lkinfo.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	_ASM

/*
 * Implementation-specific Mutex structure:
 *
 * The owner field is stuffed into the first word with the lock so that
 * both the lock and the owner can be cleared at the same time with a single
 * store instruction.  This allows priority inheritance to work correctly
 * even if mutex_exit() is preempted.
 */
typedef union mutex_impl {
	/*
	 * Generic mutex.  Fields common to all mutex types.
	 */
	struct generic_mutex {
		u_int	m_owner_lock;	/* 0	owner of mutex */
		char	_filler[3];	/* 4-6  type specific */
		uchar_t	m_type;		/* 7	type */
	} m_generic;

	/*
	 * Default adaptive mutex (without stats).
	 */
	struct adaptive_mutex {
		uint_t	m_owner_lock;		/* owner thread_id */
		turnstile_id_t	m_waiters; 	/* 4-5 turnstile cookie */
		disp_lock_t	m_wlock;	/* 6	waiters field lock */
		uchar_t		m_type;		/* 7	type (zero) */
	} m_adaptive;

	/*
	 * Spin Mutex.
	 */
	struct spin_mutex {
		uint_t	m_owner_lock;	/* owner/lock */
		uchar_t	m_oldspl;	/* 4 old spl value */
		uchar_t	m_minspl;	/* 5 min PSR_PIL val if lock held */
		uchar_t	m_spinlock;	/* 6	spin lock */
		uchar_t	m_type;		/* 7	type */
	} m_spin;

	/*
	 * Any mutex with statistics.
	 */
	struct stat_mutex {
		uint_t	m_stats_lock;	/* stats package ptr */
		uchar_t	_filler[3];	/* 4-6	unused */
		uchar_t	m_type;		/* 7	type */
	} m_stats;
} mutex_impl_t;

/*
 * Macro to retrieve 32-bit pointer field out of mutex.
 * Relies on 32-byte alignment of mutex_stats and thread structures.
 */
#define	MUTEX_OWNER_PTR(owner_lock) ((owner_lock) & MUTEX_PTR_MASK)
#define	MUTEX_OWNER_LOCK(lp)	(((struct adaptive_mutex *)(lp))->m_owner_lock)
#define	MUTEX_PTR(lp)	MUTEX_OWNER_PTR(MUTEX_OWNER_LOCK(lp))
#define	MUTEX_OWNER(lp)	((kthread_id_t)MUTEX_PTR(lp))
#define	MUTEX_NO_OWNER	((kthread_id_t)MUTEX_PTR_OR)
#define	MUTEX_STATS_SET(lp, ptr) ((lp)->m_stats.m_stats_lock = ((u_int) (ptr)))
#define	MUTEX_NO_STATS	((struct mutex_stats *)MUTEX_PTR_OR)
#define	MUTEX_STATS(lp)	((struct mutex_stats *)MUTEX_PTR(lp))

/*
 * Statistics package which replaces the owner field for some mutexes.
 */
#define	LOCK_NAME_LEN	18	/* max length of name stored in lock stats */

struct mutex_stats {
	mutex_impl_t	m_real;			/* real mutex */
	lkinfo_t	lkinfo;			/* lock name and flags */
	lkstat_t	*lkstat;		/* statistics */
	char		name[LOCK_NAME_LEN];	/* name */
	uchar_t		flag;			/* flag */
};

/*
 * flags in mutex_stats.flag
 */
#define	MSTAT_STARTUP_ALLOC	1	/* allocated from startup pool */

#endif	/* _ASM */

#define	MUTEX_NOT_FREE 0x1
#define	MUTEX_PTR_OR	0x0		/* mask to set into 32-bit pointer */
#define	MUTEX_PTR_ALIGN	4		/* lower 2 bits must be zero */
#define	MUTEX_PTR_MASK	~3

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MUTEX_IMPL_H */
