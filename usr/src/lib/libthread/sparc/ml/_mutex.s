/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident "@(#)_mutex.s  1.2     96/05/20"
        .file "_mutex.s"
 
#include <sys/asm_linkage.h>
 
#include "SYS.h"
#include "synch32.h"
#include "../assym.s"

/*
 * Returns > 0 if there are waiters for this lock.
 * Returns 0 if there are no waiters for this lock.
 * Could seg-fault if the lock memory which contains waiter info is freed.
 * The seg-fault is handled by libthread and the PC is advanced beyond faulting
 * instruction.
 *
 * int
 * _mutex_unlock_asm (mp)
 *      mutex_t *mp;
 */
        .global __wrd
        ENTRY(_mutex_unlock_asm)
	clr	[%o0 + MUTEX_OWNER]	! clear owner
        clrb	[%o0 + MUTEX_LOCK]	! clear lock
        ldstub  [%sp - 4], %g0          ! flush CPU store buffer
        clr     %o1                     ! clear to return correct waiters
__wrd:  ldub    [%o0+MUTEX_WAITERS], %o1! read waiters into %o1: could seg-fault
        retl                            ! return
        mov     %o1, %o0                ! return waiters into %o0
        SET_SIZE(_mutex_unlock_asm)


/*
 * _lock_try_adaptive(mutex_t *mp)
 *
 * Stores an owner if it successfully acquires the mutex.
 */
	ENTRY(_lock_try_adaptive)
	ldstub	[%o0 + MUTEX_LOCK], %o1		! try lock
	tst	%o1				! did we get it?
	bz,a	1f				! yes
	st	%g7, [%o0 + MUTEX_OWNER]	! delay (annulled) - set owner
1:
	retl					! return
	xor	%o1, 0xff, %o0			! delay - set return value
	SET_SIZE(_lock_try_adaptive)


/*
 * _lock_clear_adaptive(mutex_t *mp)
 *
 * Clear lock and owner, making sure the store is pushed out to the
 * bus on MP systems.  We could also check the owner here.
 */
	ENTRY(_lock_clear_adaptive)
	clr	[%o0 + MUTEX_OWNER]		! clear owner
	clrb	[%o0 + MUTEX_LOCK]		! clear lock
	retl					! return
	ldstub	[%sp - 4], %g0			! delay - flush out store
	SET_SIZE(_lock_clear_adaptive)

/*
 * _lock_owner(mutex_t *mp)
 *
 * Return the thread pointer of the owner of the mutex.
 */
	ENTRY(_lock_owner)
	retl
	ld	[%o0 + MUTEX_OWNER], %o0	! delay - get owner
	SET_SIZE(_lock_owner)
