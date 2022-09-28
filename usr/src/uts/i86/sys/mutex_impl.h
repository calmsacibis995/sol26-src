/*
 *	Copyright (c) 1991,1993 Sun Microsystems, Inc.
 */

#ifndef _SYS_MUTEX_IMPL_H
#define	_SYS_MUTEX_IMPL_H

#pragma ident	"@(#)mutex_impl.h	1.4	94/10/20 SMI"

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
 * The high-order bit of the first word of the mutex is used as the lock
 * for adaptive mutexes.  By leaving this bit set and otherwise unused
 * in other types of mutexes, the adaptive mutex_enter() can simply try
 * to set it without first checking the type.  If the btsl (386) or
 * cmpxchg (486) isn't successful on the first try, then the type field
 * is checked before proceeding.
 *
 * The owner field is stuffed into the first word with the lock so that
 * both the lock and the owner can be cleared at the same time with a single
 * store instruction.  This allows priority inheritance to work correctly
 * even if mutex_exit() is preempted.
 *
 * The 31-bit fields are used to hold the significant bits of either the
 * owning thread pointer or a pointer to the stats package.  This relies
 * on the upper bit of those pointers being 1 (address above 0x80000000).
 *
 * The lock and type fields must be bytes 3 and 7, respectively, in all
 * mutex types.
 */
typedef union mutex_impl {
	/*
	 * Generic mutex.  Fields common to all mutex types.
	 */
	struct generic_mutex {
		char	_filler1[3];	/* 0-2  type specific */
		lock_t	m_lock;		/* 3	reserved for adaptive lock */

		char	_filler2[3];	/* 4-6  type specific */
		uchar_t	m_type;		/* 7	type */
	} m_generic;

	/*
	 * Default adaptive mutex (without stats).
	 */
	struct adaptive_mutex {
		uint_t	m_owner : 31;	/* 31 bits of owner thread_id */
		uint_t	m_lock : 1;	/* 1 bit lock */
		turnstile_id_t	m_waiters;	/* 4-5 turnstile cookie */
		disp_lock_t	m_wlock;	/* 6	waiters field lock */
		uchar_t		m_type;		/* 7	type (zero) */
	} m_adaptive;

	/*
	 * Another way of looking at adaptive mutexes.
	 */
	struct adaptive_mutex2 {
		uint_t		m_owner_lock;	/* 0-3	owner and lock */
		turnstile_id_t	m_waiters;	/* 4-5 turnstile cookie */
		disp_lock_t	m_wlock;	/* 6	waiters field lock */
		uchar_t		m_type;		/* 7	type (zero) */
	} m_adaptive2;

	/*
	 * Spin Mutex.
	 */
	struct spin_mutex {
		ushort_t m_oldspl;	/* 0-1	old %psr value */
		lock_t	m_spinlock;	/* 2	real lock */
		lock_t	m_dummylock;	/* 3	lock (always set) */
		ushort_t m_minspl;	/* 4-5	min PSR_PIL val if lock held */
		char	_filler;	/* 6	unused */
		uchar_t	m_type;		/* 7	type */
	} m_spin;

	/*
	 * Any mutex with statistics.
	 */
	struct stat_mutex {
		uint_t	m_stats_lock;	/* 0-3	stats package ptr */
		uchar_t	_filler[3];	/* 4-6	unused */
		uchar_t	m_type;		/* 7	type */
	} m_stats;
} mutex_impl_t;

/*
 * Macro to retrieve 32-bit pointer field out of mutex.
 * Relies on KERNELBASE (and all thread and mutex_stats pointers)
 * being above 0xe0000000.
 */
#define	MUTEX_OWNER_PTR(owner_lock)	((owner_lock) | PTR24_BASE)
#define	MUTEX_OWNER_LOCK(lp)	(((struct adaptive_mutex2 *)(lp))->m_owner_lock)
#define	MUTEX_PTR(lp)	MUTEX_OWNER_PTR(MUTEX_OWNER_LOCK(lp))
#define	MUTEX_OWNER(lp)	((kthread_id_t)MUTEX_PTR(lp))
#define	MUTEX_NO_OWNER	((kthread_id_t)PTR24_BASE)
#define	MUTEX_STATS_SET(lp, ptr) ((lp)->m_stats.m_stats_lock = (u_int)(ptr))
#define	MUTEX_NO_STATS	((struct mutex_stats *)PTR24_BASE)
#define	MUTEX_STATS(lp)	((struct mutex_stats *)MUTEX_PTR(lp))
#define	MUTEX_ADAPTIVE_HELD	0x1	/* value of m_lock when held */

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

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MUTEX_IMPL_H */
