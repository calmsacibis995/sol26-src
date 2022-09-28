/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */
/*
 *   Copyright (c) 1994 Sun Microsystems, Inc.  All Rights Reserved.
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	
 *	int isnanf(srcF)
 *	float srcF;
 *
 *	This routine returns 1 if the argument is a NaN
 *		     returns 0 otherwise.
 *   Syntax:	
 *
 */

	.ident "@(#)isnanf.s 1.7	94/09/23 SMI"

	.file	"isnanf.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(isnanf,function)

#include "synonyms.h"

#define	FMAX_EXP	0xff

	ENTRY(isnanf)
	stwu	%r1,-16(%r1)
	stfd	%f1,8(%r1)		/* XXXPPC arg expected in Single */
#ifdef	__LITTLE_ENDIAN			/*  	  Precision in %f1	 */
	lwz	%r3,12(%r1)
#else	/* __BIG_ENDIAN */
	lwz	%r3,8(%r1)
#endif	/* __LITTLE_ENDIAN */
	rlwinm	%r4, %r3, 9, 24, 31
	cmpi	%r4, FMAX_EXP
	rlwinm	%r5, %r3, 0, 9, 31	/* r5 = mantissa */
	li	%r3, 0
	bne	.isnanf_done		/* return 0 if not NaN */
	cmpi	%r5, 0
	li	%r3, 1
	bne	.isnanf_done		/* return 1 if NaN */
	li	%r3, 0
.isnanf_done:
	addi	%r1,%r1,16
	blr				/* return 0 if infinity */
	SET_SIZE(isnanf)
