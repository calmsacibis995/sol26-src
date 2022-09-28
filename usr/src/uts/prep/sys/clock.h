/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_CLOCK_H
#define	_SYS_CLOCK_H

#pragma ident	"@(#)clock.h	1.14	96/01/18 SMI"

#include <sys/psw.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(_KERNEL) && !defined(_ASM)

#include <sys/time.h>

extern void delay(long);
extern void unlock_hres_lock(void);
extern int lock_hres_lock(void);
extern void restore_int_flag(int);
extern timestruc_t pc_tod_get(void);
extern void pc_tod_set(timestruc_t);
extern hrtime_t get_time_base(void);

/*
 * Variables protected by hres_lock.
 * These must be declared volatile to safely use them in C, because their
 * access order with respect to hres_lock is important.
 *
 * hres_lock is used in such a way that code accessing these variables can
 * see whether the lock was grabbed during the access, and if so, repeat the
 * process.  Locking sets the low-order bit of the lock, and unlocking
 * increments the rest of the lock.
 */
extern volatile int	hres_lock;	/* lock over these variables */
extern volatile hrtime_t tb_at_tick;	/* timebase at last tick (non-601) */
extern volatile longlong_t hrestime_adj;	/* adjtime() adjustment */

/*
 * End of variables covered by hres_lock.
 */

/*
 * timebase_period is multiplier to convert the time base to
 * nanoseconds times 2^NSEC_SHIFT.
 */
extern int	timebase_period;	/* nanoseconds * 2^NSEC_SHIFT / tb */
extern int	tbticks_per_10usec;	/* timebase ticks for a 10 us delay */
extern int	dec_incr_per_tick;	/* decrementer increments per HZ */

#endif

#define	NANOSEC	1000000000
#define	ADJ_SHIFT 4		/* used in get_hrestime and clock_intr() */

/*
 * The following shifts are used to scale the timebase in order to reduce
 * the error introduced by integer math.  For example, if the timebase_freq
 * is 16.5 MHz, this corresponds to an update interval (1/f) of 60.6060...
 * nanoseconds/time-base-incr.  If the timebase is simply multiplied by
 * 60, we get a 1% error (43 minutes slow after 3 days).  So, instead
 * scale the multiplier by (2^11) and then divide by (2^11) afterwards,
 * reducing the error to less than 1 ppm for reasonable frequencies.
 */
#define	NSEC_SHIFT	11	/* multiplier scale */
#define	NSEC_SHIFT1	4	/* pre-multiply scale to avoid overflow */
#define	NSEC_SHIFT2	(NSEC_SHIFT-NSEC_SHIFT1) /* post multiply shift */

#define	YRBASE		00	/* 1900 - what year 0 in chip represents */

/*
 * CLOCK_LOCK() puts a lock in the lowest byte of the hres_lock. The
 * higher three bytes are used as a counter. This lock is acquired
 * around "hrestime" and "timedelta". This lock is acquired to make
 * sure that level_clock accounts for changes to this variable in that
 * interrupt itself. The level_clock interrupt code also acquires this
 * lock.
 *
 * CLOCK_UNLOCK() increments the lower bytes straight, thus clearing the
 * lock and also incrementing the counter. This way gethrtime()
 * can figure out if the value in the lock got changed or not.
 */


#ifdef _BIG_ENDIAN
#define	HRES_LOCK_OFFSET 3	/* lock byte offset within hres_lock */
#else
#define	HRES_LOCK_OFFSET 0	/* lock byte offset within hres_lock */
#endif

#ifndef _ASM

#define	CLOCK_LOCK() lock_hres_lock()
#define	CLOCK_UNLOCK(spl) unlock_hres_lock(); restore_int_flag(spl)

#endif	/* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CLOCK_H */
