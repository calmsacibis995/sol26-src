/*
 *   Copyright (c) 1994 Sun Microsystems, Inc.  All Rights Reserved.
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	
 *
 *   Syntax:	int itrunc(double d)
 *		unsigned uitrunc(double d)
 *
 */

	.ident "@(#)itrunc.s 1.7	95/11/11 SMI"

#include "SYS.h"



	ENTRY(itrunc)			! cvt double to signed integer

	stwu	%r1, -16(%r1)
	fctiw	%f0, %f1
	stfd	%f0,8(%r1)
#if	defined(__LITTLE_ENDIAN)
	lwz	%r3,8(%r1)
#else	/* __BIG_ENDIAN */
	lwz	%r3,12(%r1)
#endif	/* __LITTLE_ENDIAN */
	addi	%r1,%r1,16
	blr

	SET_SIZE(itrunc)



	ENTRY(uitrunc)			! cvt double to unsigned integer

	stwu	%r1, -16(%r1)
	li	%r3, EXT16(0xffff)
	rlwinm	%r4, %r3, 0, 1, 31	! r4 = 0x7FFFFFFF
	li	%r0, 0
#if	defined(__LITTLE_ENDIAN)
	stw	%r0,8(%r1)
	stw	%r4,12(%r1)
#else	/* __BIG_ENDIAN */
	stw	%r0,12(%r1)
	stw	%r4,8(%r1)
#endif	/* __LITTLE_ENDIAN */
	lfd	%f4,8(%r1)		! %f4 = 0x000000007FFFFFFF

	lis	%r5, 0x1000		! %r5 = 0x10000000 == 2^31

	lis	%r4, 0x41b0
#if	defined(__LITTLE_ENDIAN)
	stw	%r0,8(%r1)
	stw	%r4,12(%r1)
#else	/* __BIG_ENDIAN */
	stw	%r0,12(%r1)
	stw	%r4,8(%r1)
#endif	/* __LITTLE_ENDIAN */
	lfd	%f5,8(%r1)		! %f5 = (double)0x10000000 == 2^31

	lis	%r4, 0x41f0
#if	defined(__LITTLE_ENDIAN)
	stw	%r0,8(%r1)
	stw	%r4,12(%r1)
#else	/* __BIG_ENDIAN */
	stw	%r0,12(%r1)
	stw	%r4,8(%r1)
#endif	/* __LITTLE_ENDIAN */
	lfd	%f3,8(%r1)		! %f3 = (double)0x100000000 == 2^32

	li	%r4, 0
#if	defined(__LITTLE_ENDIAN)
	stw	%r0,8(%r1)
	stw	%r4,12(%r1)
#else	/* __BIG_ENDIAN */
	stw	%r0,12(%r1)
	stw	%r4,8(%r1)
#endif	/* __LITTLE_ENDIAN */
	lfd	%f0,8(%r1)		! %f0 = (double)0x0

	fmr	%f2, %f0
	fcmpu	2, %f1, %f0
	blt	2, .store
	fmr	%f2, %f4
	fcmpu	2, %f1, %f3
	bgt	2, .store
	fsub	%f2, %f1, %f5
	fcmpu	2, %f1, %f5
	bge	2, .conv
	fmr	%f2, %f1
.conv:
	fctiw	%f2, %f2
.store:
	stfd	%f2,8(%r1)
#if	defined(__LITTLE_ENDIAN)
	lwz	%r3,8(%r1)
#else	/* __BIG_ENDIAN */
	lwz	%r3,12(%r1)
#endif	/* __LITTLE_ENDIAN */
	blt	2, .return
	add	%r3, %r3, %r5
.return:	
	addi	%r1,%r1,16
	blr
	SET_SIZE(uitrunc)
