/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */

/*
 *   Copyright (c) 1994 Sun Microsystems, Inc.  All Rights Reserved.
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	
 *	int isnand(srcD)
 *	double srcD;
 *
 *	This routine returns 1 if the argument is a NaN
 *		     returns 0 otherwise.
 *
 *	int isnan(srcD)
 *	double srcD;
 *	-- functionality is same as isnand().
 *
 *   Syntax:	
 *
 */

	.ident "@(#)isnand.s 1.5	94/09/23 SMI"

	.file	"isnand.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(isnan,function)
	ANSI_PRAGMA_WEAK(isnand,function)

#include "synonyms.h"

#define	DMAX_EXP	0x7ff

	ENTRY2(isnan,isnand)
	stwu	%r1,-16(%r1)
	stfd	%f1,8(%r1)
#ifdef	__LITTLE_ENDIAN
	lwz	%r3,12(%r1)
	lwz	%r4,8(%r1)
#else
	lwz	%r3,8(%r1)
	lwz	%r4,12(%r1)
#endif
	rlwinm	%r5, %r3, 12, 21, 31	/* 11 EXP bits in r3 LSB */
	cmpi	%r5, DMAX_EXP
	rlwinm	%r6, %r3, 0, 12, 31	/* r6 = 0...0xx...xx */
	or	%r6, %r6, %r4
	li	%r3, 0			/* return 0 if not NaN */
	bne	.isnan_done
	cmpi	%r6, 0			/* check if mantissa 0 */
	li	%r3, 1			/* return 1 if NaN */
	bne	.isnan_done
	li	%r3, 0			/* return 0 if infinity */
.isnan_done:
	addi	%r1,%r1,16
	blr
	SET_SIZE(isnan)
	SET_SIZE(isnand)
