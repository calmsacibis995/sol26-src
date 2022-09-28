/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */

/*
 *   Copyright (c) 1994 Sun Microsystems, Inc.  All Rights Reserved.
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	
 *	fpsetsticky()	sets fp sticky bits.
 *
 *   Syntax:	
 *
 */

	.ident "@(#)fpsetsticky.s 1.5	94/09/23 SMI"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fpsetsticky,function)
#include "synonyms.h"

	ENTRY(fpsetsticky)
	stwu	%r1,-16(%r1)
	mffs	%f0
	stfd	%f0,8(%r1)
#ifdef	__LITTLE_ENDIAN
	lwz	%r0,8(%r1)
#else	/* __BIG_ENDIAN */
	lwz	%r0,12(%r1)
#endif	/* __LITTLE_ENDIAN */
	andi.	%r3, %r3, 0x9FFC	/* sticky bit mask */
	slwi	%r3, %r3, 19		/* get new mask in position */
	srwi	%r4, %r0, 19		/* save MS 19 bits */
	rlwimi	%r0, %r3, 0, 0, 12	/* insert new mask */
#ifdef	__LITTLE_ENDIAN
	stw	%r0,8(%r1)
#else
	stw	%r0,12(%r1)
#endif
	lfd	%f0,8(%r1)
	andi.	%r3, %r4, 0x9FFC	/* return old sticky bits */	
	mtfsf	0xff, %f0		/* set new sticky bits */
	addi	%r1,%r1,16
	blr
	SET_SIZE(fpsetsticky)

