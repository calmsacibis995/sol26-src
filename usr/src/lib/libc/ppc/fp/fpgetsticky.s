/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */

/*
 *   Copyright (c) 1994 Sun Microsystems, Inc.  All Rights Reserved.
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	
 *	fpgetsticky()	returns fp sticky bits.
 *
 *   Syntax:	
 *
 */

	.ident "@(#)fpgetsticky.s 1.6	95/11/11 SMI"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fpgetsticky,function)
#include "synonyms.h"

	ENTRY(fpgetsticky)
	stwu	%r1,-16(%r1)
	mffs	%f0
	stfd	%f0,8(%r1)
#ifdef	__LITTLE_ENDIAN
	lwz	%r0,8(%r1)
#else	/* __BIG_ENDIAN */
	lwz	%r0,12(%r1)
#endif	/* __LITTLE_ENDIAN */
	lis	%r3, EXT16(0x9FFC)	/* sticky bits */
	and	%r3, %r3, %r0	
	srwi	%r3, %r3, 19		/* shift right 19 */
	addi	%r1,%r1,16
	blr				/* return sticky bits */
	SET_SIZE(fpgetsticky)

