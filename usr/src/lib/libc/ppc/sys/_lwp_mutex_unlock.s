/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)_lwp_mutex_unlock.s	1.8	94/11/18 SMI"

	.file	"_lwp_mutex_unlock.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	_lwp_mutex_unlock - mutual exclusion
 *
 *   Syntax:	int _lwp_mutex_unlock(lwp_mutex_t *mp);
 *
 *   called internally by _lwp_mutex_unlock(), (port/sys/lwp.c)
 */

#include <sys/asm_linkage.h>
#include <../assym.s>

	ANSI_PRAGMA_WEAK(_lwp_mutex_unlock,function)

#include "SYS.h"

	ENTRY(_lwp_mutex_unlock)

	addi	%r6,%r3,MUTEX_LOCK_WORD	! get address of lock word in mutex	
	li	%r5,0			! lock clear value
.L0:
	lwarx	%r4,0,%r6		! fetch and reserve lock word
	stwcx.	%r5,0,%r6		! try and unlock mutex
	bne-	.L0			! someone changed it on us, try again
	lis	%r5,WAITER_MASK >> 16	! upper half of mask
	ori	%r5,%r5,WAITER_MASK&0xffff ! lower half of mask
	and.	%r4,%r4,%r5		! any waiters?
	beq+	.L1			! no, all done
	SYSTRAP(lwp_mutex_unlock)	! do the kernel thing
	SYSLWPERR
	RET
.L1:
	li	%r3,0
	RET

	SET_SIZE(_lwp_mutex_unlock)

/*
 * int
 * ___lwp_mutex_unlock (mp)
 *	mutex_t *mp;
 */
 	ENTRY(___lwp_mutex_unlock)
 	SYSTRAP(lwp_mutex_unlock)
/* 	SYSLWPERR	*/
	bns+	.lwp_ok2
	cmpwi	%r3,ERESTART
	bne+	.lwp_err2
	li	%r3,EINTR
	b	.lwp_err2
.lwp_ok2:
	li	%r3,0
.lwp_err2:
 	RET
 	SET_SIZE(___lwp_mutex_unlock)
