/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */

/*
 *   Copyright (c) 1994 Sun Microsystems, Inc.  All Rights Reserved.
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	
 *	_QgetRD()  returns current round control value direction bits.
 *	_QgetRP()  returns current round control value precision bits.
 *	_QswapRD(rd)  exchanges rd with the current rounding direction.
 *	_QswapRP(rp)  exchanges rp with the current rounding precision.
 *
 *   Syntax:	
 *
 */

	.ident "@(#)_Q_get_rp_rd.s 1.7	94/09/23 SMI"

#include "synonyms.h"
#include <sys/asm_linkage.h>


	ENTRY(_QgetRD)
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
	SET_SIZE(_QgetRD)


	ENTRY(_QswapRD)
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
	mr	%r3, %r4		/* return old mask */
	mtfsf	0xff, %f0		/* set new mask */
	addi	%r1,%r1,16
	blr
	SET_SIZE(_QswapRD)


	ENTRY(_QgetRP)
	blr
	SET_SIZE(_QgetRP)


	ENTRY(_QswapRP)
	blr
	SET_SIZE(_QswapRP)

