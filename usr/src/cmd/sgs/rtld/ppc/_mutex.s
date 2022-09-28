/*
 *	Copyright (c) 1991,1992,1993,1994 by Sun Microsystems, Inc.
 */
#pragma ident	"@(#)_mutex.s	1.7	94/12/21 SMI"


/*
 * This file impliments the system calls required to support mutex's.  Once
 * This stuff gets into libc we can get rid of this file (and SYS.h,
 * PIC.h ...).
 */
#if	!defined(lint)

#include <sys/asm_linkage.h>
#include "SYS.h"

	.file	"mutex.s"

/*
 * int
 * lwp_cond_broadcast(condvar_t * cvp)
 */
	SYSCALL(lwp_cond_broadcast)
	RET
	SET_SIZE(lwp_cond_broadcast)

/*
 * int
 * lwp_cond_signal(condvar_t * cvp)
 */
	SYSCALL(lwp_cond_signal)
	RET
	SET_SIZE(lwp_cond_signal)

/*
 * int
 * lwp_cond_wait(condvar_t * cvp, mutex_t * mp, struct timeval * tv)
 */
	SYSCALL(lwp_cond_wait);
	RET
	SET_SIZE(lwp_cond_wait)

/*
 * void
 * lwp_mutex_lock(mutex_t *)
 */
	SYSCALL_INTRRESTART(lwp_mutex_lock)
	RET
	SET_SIZE(lwp_mutex_lock)

/*
 * void
 * lwp_mutex_unlock(mutex_t *)
 */
	SYSCALL(lwp_mutex_unlock)
	RET
	SET_SIZE(lwp_mutex_unlock)

/*
 * lock_try(unsigned char *)
 */
	.set	.LOCKED,0xff

	ENTRY(_lock_try)
	andi.	%r10,%r3,3		# %r10 is byte offset in word
	rlwinm	%r10,%r10,3,0,31	# %r10 is bit offset in word
	li	%r9,.LOCKED		# %r9 is byte mask
	rlwnm	%r9,%r9,%r10,0,31	# %r9 is byte mask shifted
	rlwinm	%r8,%r3,0,0,29		# %r8 is the word to reserve
.L0:
	lwarx	%r5,0,%r8		# fetch and reserve lock
	and.	%r3,%r5,%r9		# already locked?
	bne-	.L1			# yes
	or	%r5,%r5,%r9		# set lock values in word
	stwcx.	%r5,0,%r8		# still reserved?
	bne-	.L0			# no, try again
	li	%r3,0			# wasn't locked
	isync				# context synchronize
.L1:
	xor	%r3,%r3,%r9		# reverse sense of result
	blr
	SET_SIZE(_lock_try)

/*
 * lock_clear(unsigned char *)
 */
	.set	.UNLOCKED,0

	ENTRY(_lock_clear)
	li	%r0,.UNLOCKED		# clear value
	sync				# synchronize before releasing lock
	stb	%r0,0(%r3)		# clear the lock
	blr
	SET_SIZE(_lock_clear)


#else

/*
 * Define each routine, using all its arguments, just to shut lint up.
 */
#include <thread.h>

extern void	_halt();

int
lwp_cond_broadcast(condvar_t * cvp)
{
	return ((int)cvp->wanted);
}

int
lwp_cond_signal(condvar_t * cvp)
{
	return ((int)cvp->wanted);
}

int
lwp_cond_wait(condvar_t * cvp, mutex_t * mp, struct timeval * tv)
{
	return ((int)((int)cvp->wanted + (int)mp->wanted + (int)tv->tv_sec));
}

void
lwp_mutex_lock(mutex_t * mp)
{
	mp = NULL;
	_halt();
}

void
lwp_mutex_unlock(mutex_t * mp)
{
	mp = NULL;
}

int
_lock_try(unsigned char * lp)
{
	return ((int)*lp);
}

void
_lock_clear(unsigned char * lp)
{
	*lp = 0;
}

#endif
