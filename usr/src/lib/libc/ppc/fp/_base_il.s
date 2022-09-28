/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 */
/*
 *   Copyright (c) 1994 Sun Microsystems, Inc.  All Rights Reserved.
 *   Solaris C Library Routine
 *====================================================================
 *
 *   Function:	
 *	double max_normal() - returns MAX DOUBLE
 *	double min_normal() - returns MIN DOUBLE
 *	double signaling_nan() - returns a NaN in %f1
 *	double __dabs(dd)  - returns absolute value of double dd
 *	void __get_ieee_flags(dp) - returns fpscr in *dp
 *	void __set_ieee_flags(dp) - sets  fpscr to *dp
 *	void __four_digits_quick(us,s) - returns 4 decimal digits in string s
 *
 *   Syntax:	
 *
 */

	.ident "@(#)_base_il.s 1.9	96/03/21 SMI"

#include <sys/asm_linkage.h>
#include <PIC.h>


	ENTRY(__dabs)
	fabs	%f1, %f1	/* return absolute value in %f1 */
	blr
	SET_SIZE(__dabs)


	ENTRY(__get_ieee_flags)
	mffs	%f0
	stfd	%f0, 0(%r3)	/* XXXPPC any other side-effects ? */
	blr
	SET_SIZE(__get_ieee_flags)


	ENTRY(__set_ieee_flags)
	lfd	%f0, 0(%r3)
	mtfsf	0xff, %f0	/* XXXPPC any other side-effects ? */
	blr
	SET_SIZE(__set_ieee_flags)


	ENTRY(__four_digits_quick)
	andi.	%r10,%r3,65535

	sri	%r11,%r10,1
	addi	%r12,%r0,-4
	and	%r11,%r11,%r12
#ifdef	PIC
	stwu	%r1,-32(%r1)
	mflr	%r0
	stw	%r0,+4(%r1)
	stw	%r31,+20(%r1)
	PIC_SETUP()
	mflr	%r31
	lwz	%r12,__four_digits_quick_table@got(%r31)
	lwz	%r0,+4(%r1)
	lwz	%r31,+20(%r1)
	mtlr	%r0
	addi	%r1, %r1, 32
#else
	addis	%r12,%r0,__four_digits_quick_table@h
	ori	%r12,%r12,__four_digits_quick_table@l
#endif
	addc	%r9,%r12,%r11
	andi.	%r10,%r10,7
	lbz	%r12,+3(%r9)
	sli	%r12,%r12,24
	srawi	%r12,%r12,24
	addc	%r12,%r12,%r10

	cmpi	0,%r12,57
	bgt	..LL34

	stb	%r12,+3(%r4)


	lbz	%r12,+2(%r9)
	sli	%r12,%r12,24
	srawi	%r12,%r12,24
	stb	%r12,+2(%r4)
	b	..LL35
..LL34:

	addic	%r12,%r12,-10
	stb	%r12,+3(%r4)

	lbz	%r12,+2(%r9)
	sli	%r12,%r12,24
	srawi	%r12,%r12,24

	cmpi	0,%r12,56
	bgt	..LL36

	addic	%r12,%r12,1
	stb	%r12,+2(%r4)

	b	..LL35
..LL36:

	addic	%r12,%r12,-9
	stb	%r12,+2(%r4)

	lbz	%r12,+1(%r9)
	sli	%r12,%r12,24
	srawi	%r12,%r12,24

	cmpi	0,%r12,56
	bgt	..LL37

	addic	%r12,%r12,1
	stb	%r12,+1(%r4)

	b	..LL38
..LL37:

	addic	%r12,%r12,-9
	stb	%r12,+1(%r4)

	lbz	%r12,0(%r9)
	sli	%r12,%r12,24
	srawi	%r12,%r12,24
	addic	%r12,%r12,1
	stb	%r12,0(%r4)

	b	..LL39
..LL35:

	lbz	%r12,+1(%r9)
	sli	%r12,%r12,24
	srawi	%r12,%r12,24
	stb	%r12,+1(%r4)
..LL38:

	lbz	%r12,0(%r9)
	sli	%r12,%r12,24
	srawi	%r12,%r12,24
	stb	%r12,0(%r4)

..LL39:

	blr
	SET_SIZE(__four_digits_quick)
