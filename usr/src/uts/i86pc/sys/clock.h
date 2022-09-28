/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_CLOCK_H
#define	_SYS_CLOCK_H

#pragma ident	"@(#)clock.h	1.14	96/10/17 SMI"

#include <sys/psw.h>
#include <sys/time.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(_KERNEL) && !defined(_ASM)

extern void delay(long);
extern void unlock_hres_lock(void);
extern timestruc_t pc_tod_get(void);
extern void pc_tod_set(timestruc_t);

#endif

#define	ADJ_SHIFT 4		/* used in get_hrestime and clock_intr() */

#define	YRBASE		00	/* 1900 - what year 0 in chip represents */

/*
 * CLOCK_LOCK() sets the LSB (bit 0) of the hres_lock. The rest of the
 * 31bits are used as the counter. This lock is acquired
 * around "hrestime" and "timedelta". This lock is acquired to make
 * sure that level10 accounts for changes to this variable in that
 * interrupt itself. The level10 interrupt code also acquires this
 * lock.
 * (Note: It is assumed that the lock_set_spl() uses only bit 0 of the lock.)
 *
 * CLOCK_UNLOCK() increments the lower bytes straight, thus clearing the
 * lock and also incrementing the counter. This way gethrtime()
 * can figure out if the value in the lock got changed or not.
 */
#define	HRES_LOCK_OFFSET 0	/* byte 0 has the lock bit(bit 0 in the byte) */

#define	CLOCK_LOCK()	\
	lock_set_spl(((lock_t *)&hres_lock) + HRES_LOCK_OFFSET, 	\
						ipltospl(XC_HI_PIL))

#define	CLOCK_UNLOCK(spl)		\
	unlock_hres_lock();		\
	(void) splx(spl)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CLOCK_H */
