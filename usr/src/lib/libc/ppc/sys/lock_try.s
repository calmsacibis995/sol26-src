/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)lock_try.s	1.8	95/10/20 SMI"

	.file	"lock_try.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	lock_try - try to grab the lock; returns non-zero
 *				on success.
 *
 *   Syntax:	int lock_try(lock_t *lp);
 *
 *
 */

#include <sys/asm_linkage.h>

	.set	.LOCKED,0x01

	ENTRY(_lock_try)

#if defined(__LITTLE_ENDIAN)

	andi.	%r10, %r3, 3		! %r10 is byte offset in word
	rotlwi	%r10, %r10, 3		! %r10 is bit offset in word
	li	%r9, .LOCKED		! %r9 is byte mask
	rotlw	%r9, %r9, %r10		! %r9 is byte mask shifted
	clrrwi	%r8, %r3, 2		! %r8 is the word to reserve

#else	/* !defined(__LITTLE_ENDIAN); i.e., _BIG_ENDIAN */

#if defined(__STDC__)
!!!!!!!!XXXPPC #error Big-Endian version of lock_try does not yet exist....
#endif	/* defined(__STDC__) */

#endif	/* defined(__LITTLE_ENDIAN) */

.L5:
	lwarx	%r5, 0, %r8		! fetch and reserve lock
	and.	%r3, %r5, %r9		! already locked?
	bne-	.L6			! yes
	or	%r5, %r5, %r9		! set lock values in word
	stwcx.	%r5, 0, %r8		! still reserved?
	bne-	.L5			! no,  try again
	li	%r3,.LOCKED		! wasn't locked
	isync				! context synchronize
	blr
.L6:
	stwcx.	%r5, 0, %r8		! clear reservation
	li	%r3, 0			! was locked
	blr
	SET_SIZE(_lock_try)
