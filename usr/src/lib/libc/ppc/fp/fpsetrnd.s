/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */

/*
 *   Copyright (c) 1994 Sun Microsystems, Inc.  All Rights Reserved.
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	
 *	fpsetround()  sets current round control value direction bits.
 *
 *   Syntax:	
 *
 */

	.ident "@(#)fpsetrnd.s 1.5	94/09/23 SMI"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fpsetround,function)
#include "synonyms.h"

	ENTRY(fpsetround)
	stwu	%r1,-16(%r1)
	mffs	%f0
	stfd	%f0,8(%r1)
#ifdef	__LITTLE_ENDIAN
	lwz	%r0,8(%r1)
#else	/* __BIG_ENDIAN */
	lwz	%r0,12(%r1)
#endif	/* __LITTLE_ENDIAN */
	rlwinm	%r4, %r0, 0, 30, 31	/* get RN bits in r4 */
	rlwimi	%r0, %r3, 0, 30, 31	/* insert new RN bits */
#ifdef	__LITTLE_ENDIAN
	stw	%r0,8(%r1)
#else	/* __BIG_ENDIAN */
	stw	%r0,12(%r1)
#endif	/* __LITTLE_ENDIAN */
	lfd	%f0,8(%r1)
	mr	%r3, %r4		/* return old RN bits */
	mtfsf	0xff, %f0		/* set new RN bits */
	addi	%r1,%r1,16
	blr
	SET_SIZE(fpsetround)

