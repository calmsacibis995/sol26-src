/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */
#include <sys/asm_linkage.h>
	.file		"ppc_locks.s"

	.set	.LOCKED, 0x01
	.set	.UNLOCKED, 0

	ENTRY(tnfw_b_get_lock)
	andi.	%r10, %r3, 3		# %r10 is byte offset in word
	rotlwi	%r10, %r10, 3		# %r10 is bit offset in word
	li	%r9, .LOCKED		# %r9 is byte mask
	rotlw	%r9, %r9, %r10		# %r9 is byte mask shifted
	clrrwi	%r8, %r3, 2		# %r8 is the word to reserve
.L5:
	lwarx	%r5, 0, %r8		# fetch and reserve lock
	and.	%r3, %r5, %r9		# already locked?
	bne-	.L6			# yes
	or	%r5, %r5, %r9		# set lock values in word
	stwcx.	%r5, 0, %r8		# still reserved?
	bne-	.L5			# no, try again
	isync				# context synchronize
	blr
.L6:
	stwcx.	%r5, 0, %r8		# clear reservation
	blr
	SET_SIZE(tnfw_b_get_lock)


	ENTRY(tnfw_b_clear_lock)
	li	%r4, .UNLOCKED		# prepare to unlock
	eieio				# synchronize
	stb	%r4, 0(%r3)		# unlock
	blr
	SET_SIZE(tnfw_b_clear_lock)


	ENTRY(tnfw_b_atomic_swap)
	eieio				# synchronize stores
.L2:
	lwarx	%r5, 0, %r3		# load word with reservation
	stwcx.	%r4, 0, %r3		# store conditional
	bne-	.L2			# repeat if something else stored first
	mr	%r3, %r5		# return value
	blr				# return
	SET_SIZE(tnfw_b_atomic_swap)
