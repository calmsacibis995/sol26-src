/*
 * Copyright (c) 1995, 1996 Sun Microsystems, Inc.
 * All Rights Reserved.
 */

        .ident  "@(#)_mutex.s 1.2     96/05/20 SMI"
 
#include <sys/asm_linkage.h>
#include <assym.s>

#define THREAD_REG	%r2
 
/*
 * Atomicly clear lock in mutex and get waiters flag.
 */
        ENTRY(_mutex_unlock_asm)
	li	%r0, 0			! zero for clearing lock
	stw	%r0, MUTEX_OWNER(%r3)	! clear owner
	eieio				! complete all previous stores first
	la	%r4, M_LOCK_WORD(%r3)	! address of lock word
1:
	lwarx	%r5, 0, %r4		! load lock/waiters word
	stwcx.	%r0, 0, %r4		! clear the lock
	bne-	1b			! lost reservation, try again
	rlwinm	%r3, %r5, 0, M_WAITER_BIT, M_WAITER_BIT ! non-zero if waiters
	blr				! return non-zero if waiters
	SET_SIZE(_mutex_unlock_asm)

/*
 * _lock_try_adaptive(mutex_t *mp)
 *
 * Stores an owner if it successfully acquires the mutex.
 */
	ENTRY(_lock_try_adaptive)
	la	%r4, M_LOCK_WORD(%r3)		! get address of lock word
	lis	%r9, M_LOCK_MASK@h		! get lock mask (high bits)
	ori	%r9, %r9, M_LOCK_MASK@l		! get lock mask (low bits)
	la	%r5, MUTEX_OWNER(%r3)		! get address of owner word
2:
	lwarx	%r6, 0, %r4			! fetch and reserve lock
	and.	%r3, %r6, %r9			! already locked?
	bne-	3f				! yes
	or	%r6, %r6, %r9			! set lock values in word
	stwcx.	%r6, 0, %r4			! still reserved?
	bne-	2b				! no, try again
	stw	THREAD_REG, 0(%r5)		! store owner
	isync					! context synchronize
	li	%r3, 1				! got lock
	blr
3:
	stwcx.	%r6, 0, %r4			! clear reservation
	li	%r3, 0				! didn't get lock
	blr
	SET_SIZE(_lock_try_adaptive)

/*
 * _lock_clear_adaptive(mutex_t *mp)
 *
 * Clear lock and owner, making sure the store is pushed out to the
 * bus on MP systems.  We could also check the owner here.
 */
	ENTRY(_lock_clear_adaptive)
	li	%r4, 0
	stw	%r4, MUTEX_OWNER(%r3)		! clear owner
	eieio					! synchronize
	stb	%r4, MUTEX_LOCK(%r3)		! clear lock
	blr
	SET_SIZE(_lock_clear_adaptive)

/*
 * _lock_owner(mutex_t *mp)
 *
 * Return the thread pointer of the owner of the mutex.
 */
	ENTRY(_lock_owner)
	lwz	%r3, MUTEX_OWNER(%r3)		! get owner
	blr
	SET_SIZE(_lock_owner)
