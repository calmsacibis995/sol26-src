/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */

/*
 *   Copyright (c) 1994 Sun Microsystems, Inc.  All Rights Reserved.
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	
 *	fpgetmask()	returns fp exception mask.
 *
 *   Syntax:	
 *
 */

	.ident "@(#)fpgetmask.s 1.5	94/09/23 SMI"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fpgetmask,function)
#include "synonyms.h"

	ENTRY(fpgetmask)
	stwu	%r1,-16(%r1)
	mffs	%f0
	stfd	%f0,8(%r1)
#ifdef	__LITTLE_ENDIAN
	lwz	%r0,8(%r1)
#else	/* __BIG_ENDIAN */
	lwz	%r0,12(%r1)
#endif	/* __LITTLE_ENDIAN */
	li	%r3, 0x7F8		/* Exception mask bits */
	and	%r3, %r0, %r3	
	srwi	%r3, %r3, 3		/* shift right 3 */
	addi	%r1,%r1,16
	blr				/* return mask bits */
	SET_SIZE(fpgetmask)

