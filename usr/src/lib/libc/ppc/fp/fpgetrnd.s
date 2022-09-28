/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */

/*
 *   Copyright (c) 1994 Sun Microsystems, Inc.  All Rights Reserved.
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	
 *	fpgetround() returns current round control value direction bits.
 *
 *   Syntax:	
 *
 */

	.ident "@(#)fpgetrnd.s 1.5	94/09/23 SMI"


#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fpgetround,function)
#include "synonyms.h"

	ENTRY(fpgetround)
	stwu	%r1,-16(%r1)
	mffs	%f0
	stfd	%f0,8(%r1)
#ifdef	__LITTLE_ENDIAN
	lwz	%r0,8(%r1)
#else	/* __BIG_ENDIAN */
	lwz	%r0,12(%r1)
#endif	/* __LITTLE_ENDIAN */
	rlwinm	%r3, %r0, 0, 30, 31	/* RN bits */
	addi	%r1,%r1,16
	blr
	SET_SIZE(fpgetround)

