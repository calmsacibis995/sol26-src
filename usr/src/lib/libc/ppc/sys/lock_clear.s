/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */
 
	.ident "@(#)lock_clear.s	1.4	95/10/20 SMI"

	.file	"lock_clear.s"

/*
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	lock_clear - clear lock and force it to appear
 *		unlocked
 *
 *   Syntax:	int lock_clear(lock_t *lp)
 *
 */

#include <sys/asm_linkage.h>

	.set	UNLOCKED,0

	ENTRY(_lock_clear)
	li	%r4,UNLOCKED		# prepare to unlock
	eieio				# synchronize
	stb	%r4,0(%r3)		# unlock
	blr
	SET_SIZE(_lock_clear)
